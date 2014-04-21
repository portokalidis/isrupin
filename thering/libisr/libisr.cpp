#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <vector>
#include <cassert>


#include <sys/syscall.h>
extern "C" {
//#include <link.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <linux/limits.h>
#include <errno.h>
#include <elf.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <byteswap.h>
#include <unistd.h>
#include "sqlite3.h"
}

#include "image.hpp"
#include "pin.H"
#include "libisr.hpp"
#include "log.hpp"
#include "compiler.h"


//#define SIGNAL_DEBUG
//#define UNKNOWN_DEBUG
//#define UNKNOWN_DEBUG_ORIG
//#define MP_DEBUG
#define MP_PAUSE() do { } while (0)
//#define MP_PAUSE() do { pause(); } while (0)
//#define DL_DEBUG
#define PROACTIVE_CI

#define PAGEOFF_MASK		(0xfff)
#ifdef TARGET_IA32
# ifdef HOST_IA32
#  define KERNEL_BOUNDARY		0xbfffffff	/* x86 - host x86 */
# else
#  define KERNEL_BOUNDARY		0xffffffff	/* x86 - host x86-64*/
# endif
#elif defined(TARGET_IA32E)
# define KERNEL_BOUNDARY		0xffffffff80000000	/* x86_64 */
#endif


using namespace __gnu_cxx;
using namespace std;

typedef struct syscall_data_struct {
	ADDRINT sysnr;
	ADDRINT arg[6];
} syscall_data_t;

// Scratch register for holding syscall state
static REG sysstate_reg;
// Map of loaded images
static map<ADDRINT, Image *> image_map;
// Last loaded image (dummy cache)
static Image *last_fetch_img = NULL;
// A dummy image used to de-randomize with a randomly generate key
static Image *random_image = NULL;
// Running a signal handler (one signal per-process)
static BOOL sighandler = FALSE;
//static BOOL pintool_resolved = FALSE;
// SQLite key DB filename
static string keydbfn;



// Signal trampoline sample
#define SIGTRAMPOLINE "\x58\xb8\x77\x00\x00\x00\xcd\x80"
#define SIGTRAMPOLINE_LEN 8

// Last verified signal trampoline
static struct {
	ADDRINT low, high;
} tramp;

// XED is used for decoding unknown instructions
#ifdef UNKNOWN_DEBUG
extern "C" {
#include "xed-interface.h"
}

static xed_state_t dstate;
static BOOL do_disas = FALSE;
#endif

// Statistics
static UINT64 image_hits = 0;
static UINT64 fcallcount = 0;


static VOID ForceExit(INT32 code);


/*********************/
/* Images Management */
/*********************/
static sqlite3 *OpenDB()
{
	sqlite3 *db;

	//cerr << "sqlite3_open(" << keydbfn.c_str() << ")" << endl;
	if (sqlite3_open(keydbfn.c_str(), &db) != 0) {
		ERRLOG("SQLite error opening keys db\n");
		if (db) {
			stringstream ss;

			ss << "SQLITE3: " << sqlite3_errmsg(db) << endl;
			ERRLOG(ss);
			sqlite3_close(db);
		}
		return NULL;
	}
	return db;
}

static BOOL GetImageKey(const char *path, Image *img)
{
	const void *key_p;
	int r, keylen;
	stringstream sstr;
	sqlite3_stmt *stmt;
	sqlite3 *db = NULL;
	const char *sqlstr = "SELECT image_key.key FROM image_key,image"
		" WHERE image.path=? AND image_key.keyid=image.keyid";

	if ((db = OpenDB()) == NULL)
		return FALSE;

	r = sqlite3_prepare_v2(db, sqlstr, -1, &stmt, NULL);
	if (r != SQLITE_OK) {
query_error:
		sstr << "SQLite error querying keys db: " << 
			sqlite3_errmsg(db) << endl;
		ERRLOG(sstr);
		sqlite3_close(db);
		return FALSE;
	}
	r = sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
	if (r != SQLITE_OK)
		goto query_error;

	r = sqlite3_step(stmt);
	if (r == SQLITE_ROW) {
		keylen = sqlite3_column_bytes(stmt, 0);
		key_p = sqlite3_column_blob(stmt, 0);

		if (keylen != 2) {
			sstr << "ERROR: Invalid key size " << keylen << endl;
			ERRLOG(sstr);
			goto ret;
		}

		// We need to byteswap here if we are on little endian
		img->SetKey(htobe16(*(uint16_t *)key_p));

	} else if (r == SQLITE_DONE) {
#if 0
		OutFile << "NOTICE: No key found for " << path <<
			". Assuming file is not encrypted." << endl;
#endif
	} else
		goto query_error;

ret:
	sqlite3_finalize(stmt);
	sqlite3_close(db);
	return TRUE;
}

// Return the image that contains addr
static Image *FindExecutableImageByAddr(ADDRINT addr)
{
	Image *img = NULL;
	map<ADDRINT, Image *>::iterator it;

	for (it = image_map.begin(); it != image_map.end(); it++) {
		img = it->second;
		if (img->low_addr <= addr) {
			if (addr <= img->high_addr)
				return img;
		} else
			// The map is ordered so there are no more 
			// possible matches
			break;
	}
	return NULL;
}

static inline bool TryDeleteExecutableImage(Image *img, 
		ADDRINT low, ADDRINT high)
{
	// Image:              |--------------|
	// Delete case 1:   |--------|
	// Delete case 2:               |---------|
	// Delete case 3:          |----|
	// Delete case 4:   |----------------------|
	
	if (low <= img->low_addr && img->low_addr <= high) {
		// Case 1 & 4. Low addr of image in delete segment
		// Adjust it!
		img->low_addr = high + 1;
	} else if (low <= img->high_addr && img->high_addr <= high) {
		// Case 2 & 4. High addr of image in delete segment
		// Adjust it!
		img->high_addr = low - 1;
	} else if (low > img->low_addr && high < img->high_addr) {
		// Case 3. Delete segment within image.
		// Requires splitting the image
		
		// Create new image for high address part
		Image *newimg = new Image(high + 1, img->high_addr);
		newimg->SetKey(img->key);
		image_map.insert(pair<ADDRINT, Image *>(high + 1, newimg));

		// Resize low address part 
		img->high_addr = low - 1;
		return false;
	}

	// Check if image should be deleted
	if (img->low_addr >= img->high_addr)
		return true;
	return false;
}

// Delete or resize images so that [low, high] does not map to any image
static VOID DeleteExecutableImage(ADDRINT low, ADDRINT high)
{
	map<ADDRINT, Image *>::iterator it, prev_it;
	Image *img;

	// Page align low and high addresses
	low &= ~PAGEOFF_MASK;
	high |= PAGEOFF_MASK;

	for (it = image_map.begin(); it != image_map.end(); ) {
		img = it->second;
		prev_it = it;
		it++;

		if (TryDeleteExecutableImage(img, low, high)) {
			// There was a match. Delete this entry
			image_map.erase(prev_it);
		
			// Make sure the cache remains valid
			if (unlikely(img == last_fetch_img))
				last_fetch_img = NULL;

			delete img;
		} else if (img->low_addr > high)
			// The map is ordered so there are no more 
			// possible matches
			break;
	}
}

// Create new image and add it in the image map.
// low and high addresses are inclusive
static Image *AddExecutableImage(const string &name, 
		ADDRINT low, ADDRINT high)
{
	stringstream sstr;
	Image *img;
	pair<map<ADDRINT, Image *>::iterator, bool> res;

	// Page align low and high addresses
	low &= ~PAGEOFF_MASK;
	high |= PAGEOFF_MASK;

	img = new Image(low, high);
	assert(img);
	if (!GetImageKey(name.c_str(), img))
		ForceExit(1);

	res  = image_map.insert(pair<ADDRINT, Image *>(low, img));
	if (!res.second) {
		delete img;
		img = res.first->second;
		sstr << "WARNING: low address already mapped "
			<<  hex << img->low_addr << '-' << img->high_addr
			<< endl;
		OUTLOG(sstr);
		return img;
	}

	if (img->IsEncrypted()) {
		sstr << "Found randomized image!" << endl;
		OUTLOG(sstr);
	}

	return img;
}

/**************************************/
/* Proactice Code-Injection Detection */
/**************************************/
#ifdef PROACTIVE_CI
static VOID CodeInjection(const CONTEXT *ctx, THREADID tid)
{
	EXCEPTION_INFO einfo;
	ADDRINT addr;

	addr = PIN_GetContextReg(ctx, REG_INST_PTR);
	PIN_InitExceptionInfo(&einfo, EXCEPTCODE_PRIVILEGED_INS, addr);
	PIN_RaiseException(ctx, tid, &einfo);
}
#endif


/********************/
/* Memory Isolation */
/********************/
#ifdef ISOLATE_MEMORY

#ifdef HOST_IA32E
// 4GB address space on x86-64 hosts
#define MEMPROTECTOR_SIZE	(1024 * 1024)
#endif
#ifdef HOST_IA32
// 3 GB address space
#define MEMPROTECTOR_SIZE	(768 * 1024)
#endif
#define MEMPROTECTOR_MAXADDR	(KERNEL_BOUNDARY)


static UINT8 *memory_protector = NULL;
static ADDRINT heap_end = 0;


static VOID mp_set(ADDRINT start, ADDRINT size, UINT8 val)
{
	ADDRINT stop;
#ifdef MP_DEBUG
	stringstream sstr;
#endif

	stop = start + size;
	if (stop & 0xfff)
		stop = (stop >> 12) + 1;
	else
		stop >>= 12;

	// Check that we don't attempt to protect kernel memory
	// E.g., the VDSO may reside in kernel space, but it's r-x only
        if (start > KERNEL_BOUNDARY) {
#ifdef MP_DEBUG
                sstr << "Attempting to protect kernel memory " << (void *)start 
			<< '-' << (void *)(stop << 12) << endl;
		DBGLOG(sstr);
#endif
                return;
        }

#ifdef MP_DEBUG
	sstr << (VOID *)start << "-" << (VOID *)(stop << 12) << endl;
	DBGLOG(sstr);
#endif

	start >>= 12;
	while (start < stop)
		memory_protector[start++] = val;
}

static BOOL MemoryProtectorInit(void)
{
	memory_protector = (UINT8 *)mmap(0, MEMPROTECTOR_SIZE,
			PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (memory_protector == MAP_FAILED) {
		cerr << "Could not allocate memory protector" << endl;
		return FALSE;
	}
	memset(memory_protector, 0xff, 768 * 1024);
	return TRUE;
}

static ADDRINT PIN_FAST_ANALYSIS_CALL CheckByteAccess(ADDRINT addr)
{
	return *(memory_protector + (addr >> 12));
}

static VOID InvalidAccess(CONTEXT *ctx, THREADID tid, ADDRINT addr, UINT32 sz)
{
	stringstream sstr;
	EXCEPTION_INFO einfo;

#ifdef MP_DEBUG
	sstr << "WARNING: MP Exception at " << (void *)addr << 
		'(' << sz << ')' << endl;
	ERRLOG(sstr);
#endif
	MP_PAUSE();

	PIN_InitAccessFaultInfo(&einfo, EXCEPTCODE_ACCESS_DENIED,
			PIN_GetContextReg(ctx, REG_INST_PTR), addr,
			FAULTY_ACCESS_WRITE);
	PIN_RaiseException(ctx, tid, &einfo);
}

static ADDRINT PIN_FAST_ANALYSIS_CALL CheckWordAccess(ADDRINT addr)
{
	UINT8 p = *(memory_protector + ((UINT32)addr >> 12));
	p |= *(memory_protector + (((UINT32)addr + 1) >> 12));
	return p;
}

static ADDRINT PIN_FAST_ANALYSIS_CALL CheckDWordAccess(ADDRINT addr)
{
	UINT8 p = *(memory_protector + ((UINT32)addr >> 12));
	p |= *(memory_protector + (((UINT32)addr + 3) >> 12));
	return p;
}

static ADDRINT PIN_FAST_ANALYSIS_CALL CheckQWordAccess(ADDRINT addr)
{
	UINT8 p = *(memory_protector + ((UINT32)addr >> 12));
	p |= *(memory_protector + (((UINT32)addr + 7) >> 12));
	return p;
}

static ADDRINT PIN_FAST_ANALYSIS_CALL CheckDQWordAccess(ADDRINT addr)
{
	UINT8 p = *(memory_protector + ((UINT32)addr >> 12));
	p |= *(memory_protector + (((UINT32)addr + 15) >> 12));
	return p;
}

static ADDRINT PIN_FAST_ANALYSIS_CALL CheckNByteAccess(ADDRINT addr, UINT32 sz)
{
	UINT8 p = *(memory_protector + ((UINT32)addr >> 12));
	p |= *(memory_protector + (((UINT32)addr + sz - 1) >> 12));
	return p;
}

#ifdef MP_DEBUG
static VOID PIN_FAST_ANALYSIS_CALL DebugAddress(VOID *addr, UINT32 sz)
{
	if (((UINT32)addr + sz - 1) > MEMPROTECTOR_MAXADDR) {
		stringstream sstr;

		sstr << "Invalid memory address " << addr <<
			"(" << sz << ")" << endl;
		DBGLOG(sstr);
	}
}
#endif

static inline VOID MemoryIsolate(INS ins, VOID *v)
{
	USIZE wsize;
	AFUNPTR fptr;

	if (INS_IsMemoryWrite(ins) == FALSE)
		return;

#ifdef MP_DEBUG
	INS_InsertPredicatedCall(ins, IPOINT_BEFORE,
			(AFUNPTR)DebugAddress,
			IARG_FAST_ANALYSIS_CALL,
			IARG_MEMORYWRITE_EA,
			IARG_MEMORYWRITE_SIZE,
			IARG_END);
#endif

	switch ((wsize = INS_MemoryWriteSize(ins))) {
	case 1:
		fptr = (AFUNPTR)CheckByteAccess;
		break;
	case 2:
		fptr = (AFUNPTR)CheckWordAccess;
		break;
	case 4:
		fptr = (AFUNPTR)CheckDWordAccess;
		break;
	case 8:
		fptr = (AFUNPTR)CheckQWordAccess;
		break;
	case 16:
		fptr = (AFUNPTR)CheckDQWordAccess;
		break;

	default:
		INS_InsertIfPredicatedCall(ins, IPOINT_BEFORE,
				(AFUNPTR)CheckNByteAccess,
				IARG_FAST_ANALYSIS_CALL,
				IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE, 
				IARG_END);
		goto thencall;
	}
	INS_InsertIfPredicatedCall(ins, IPOINT_BEFORE, fptr,
				IARG_FAST_ANALYSIS_CALL,
				IARG_MEMORYWRITE_EA, IARG_END);
thencall:
	INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)InvalidAccess,
			IARG_CONTEXT, IARG_THREAD_ID,
			IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE, IARG_END);
}
#endif /* ifdef ISOLATE_MEMORY */

static BOOL ResolveFd(string &path, int fd)
{
	char procfd[64], pathstr[PATH_MAX];
	stringstream sstr;
	int r;

	sprintf(procfd, "/proc/%d/fd/%d", PIN_GetPid(), fd);
	r = readlink(procfd, pathstr, PATH_MAX);
	if (r <= 0) {
		DBGLOG(string("Cannot resolve fd (") + procfd + "): " +
			strerror(errno) + "\n");
		return FALSE;
	}

	path = string(pathstr, r);
	return TRUE;
}

static VOID ThreadCreate(THREADID tid, CONTEXT *ctx, INT32 flags, VOID *v)
{
	syscall_data_t *sysdata;

	// Allocate syscall state and assign it to scratch register
	sysdata = (syscall_data_t *)calloc(1, sizeof(syscall_data_t));
	assert(sysdata);

	PIN_SetContextReg(ctx, sysstate_reg, (ADDRINT)sysdata);
}

static VOID ThreadDestroy(THREADID tid, const CONTEXT *ctx, INT32 code, VOID *v)
{
	syscall_data_t *sysdata;

	// Free syscall state
	sysdata = (syscall_data_t *)PIN_GetContextReg(ctx, sysstate_reg);
	free(sysdata);
}

static VOID SyscallEntry(THREADID tid, CONTEXT *ctx,
		SYSCALL_STANDARD std, VOID *v)
{
	syscall_data_t *sys_state;
	unsigned i, max_argno = 0;
	bool save_arg[6] = { false, };

	sys_state = (syscall_data_t *)PIN_GetContextReg(ctx, sysstate_reg);
	sys_state->sysnr = PIN_GetSyscallNumber(ctx, std);

	switch (sys_state->sysnr) {
	case SYS_mmap:
#ifdef TARGET_IA32
	case SYS_mmap2:
#endif
		save_arg[1] = save_arg[2] = true;
		save_arg[4] = true;
		max_argno = 5;
		break;

	case SYS_munmap:
		save_arg[0] = save_arg[1] = true;
		max_argno = 2;
		break;

#ifdef ISOLATE_MEMORY
	case SYS_brk:
		save_arg[0] = true;
		max_argno = 1;
		break;

	case SYS_mremap:
		save_arg[0] = save_arg[1] = save_arg[2] = true;
		max_argno = 3;
		break;
#endif
	}

	for (i = 0; i < max_argno; i++) {
		if (!save_arg[i])
			continue;
		sys_state->arg[i] = PIN_GetSyscallArgument(ctx, std, i);
	}
}

static VOID SyscallExit(THREADID tid, CONTEXT *ctx,
		SYSCALL_STANDARD std, VOID *v)
{
	syscall_data_t *sys_state;
	ADDRINT ret, len, *arg;
	int fd, prot;
	string fdpath;
	stringstream sstr;

	sys_state = (syscall_data_t *)PIN_GetContextReg(ctx, sysstate_reg);

	switch (sys_state->sysnr) {
	case SYS_mmap:
#ifdef TARGET_IA32
	case SYS_mmap2:
#endif
		ret = PIN_GetSyscallReturn(ctx, std);
		if (unlikely((void *)ret == MAP_FAILED))
			break;

		len = sys_state->arg[1];
		prot = sys_state->arg[2];
		fd = sys_state->arg[4];

		// Delete any pre-existing map
		DeleteExecutableImage(ret, ret + len - 1);

		if ((prot | PROT_EXEC) && !(prot & PROT_WRITE) && fd >= 0) {
			if (ResolveFd(fdpath, fd)) {
				sstr << "Mapping image " << hex << 
					(ret & ~PAGEOFF_MASK) << '-' <<
					((ret + len - 1) | PAGEOFF_MASK) <<
					' ' << fdpath << endl;
				OUTLOG(sstr);
				AddExecutableImage(fdpath, ret, ret + len - 1);
			}
		}

#ifdef ISOLATE_MEMORY
		if (ret > MEMPROTECTOR_MAXADDR)
			break;
# ifdef MP_DEBUG
		DBGLOG("mmap2: ");
# endif
		mp_set(ret, len, 0);
#endif
		break;

	case SYS_munmap:
		arg = sys_state->arg;
		ret = PIN_GetSyscallReturn(ctx, std);
		if (ret != 0)
			break;
		DeleteExecutableImage(arg[0], arg[0] + arg[1] - 1);

#ifdef ISOLATE_MEMORY
# ifdef MP_DEBUG
		DBGLOG("munmap: ");
# endif
		mp_set(arg[0], arg[1], 0xff);
#endif
		break;


#ifdef ISOLATE_MEMORY
	case SYS_brk:
		ret = PIN_GetSyscallReturn(ctx, std);
# ifdef MP_DEBUG
		sstr << "brk: " << (void *)ret << ' ';
		DBGLOG(sstr);
# endif
		if (sys_state->arg[0] == 0)
			heap_end = ret;
		// Ensure that we already know where the heap starts and ends
		assert(heap_end > 0);
		if (ret > heap_end) {
			mp_set(heap_end, ret - heap_end, 0);
			heap_end = ret;
		}
# ifdef MP_DEBUG
		else {
			DBGLOG("\n");
		}
# endif
		break;

	case SYS_mremap:
		ret = PIN_GetSyscallReturn(ctx, std);
		if (ret > MEMPROTECTOR_MAXADDR)
			break;
		arg = sys_state->arg;
# ifdef MP_DEBUG
		DBGLOG("mremap: ");
# endif
		if (ret == arg[0]) { // map was resized
			if (arg[1] < arg[2]) { // enlarged
				mp_set(ret, arg[2], 0);
			} else if (arg[2] < arg[1]) { // shrunk
				DeleteExecutableImage(ret + arg[2],
						ret + arg[1] - 1);
				mp_set(ret + arg[2], arg[2] - arg[1], 0xff);
			}
			// else it remained the same
		} else { // map was moved
			// delete old location
			DeleteExecutableImage(arg[0], arg[0] + arg[1] - 1);
			mp_set(arg[0], arg[1], 0xff);
			// add new location
			mp_set(ret, arg[2], 0);
			// XXX: We do not remap executables as a security
			// precaution. It would be interesting to see if this
			// feature is used at all
		}
		break;
#endif // ISOLATE_MEMORY

	/* Unsupported */
	case SYS_remap_file_pages:
		ERRLOG("Unsupported old remap\n");
		ForceExit(1);
	}
}

#ifdef UNKNOWN_DEBUG
static VOID disas_ins(string *dis_str)
{
	stringstream sstr;

	sstr << "Running instruction " << dis_str->c_str() << endl;
	DBGLOG(sstr);
}
#endif

static bool IsTrampoline(ADDRINT addr, void *buf, size_t size)
{
#ifdef SIGNAL_DEBUG
	stringstream sstr;
#endif
	int r;

	// Make sure we are in a signal handler, and it can fit in the 
	// instruction buffer
	if (likely(!sighandler || size < SIGTRAMPOLINE_LEN))
		return FALSE;

	// Check whether we are in a trampoline we have already verified
	if (likely(tramp.low <= addr && addr < tramp.high))
		return TRUE;

	r = memcmp(SIGTRAMPOLINE, buf, SIGTRAMPOLINE_LEN);
	if (r == 0) {
		tramp.low = addr;
		tramp.high = addr + SIGTRAMPOLINE_LEN;
#ifdef SIGNAL_DEBUG
		sstr << "Detected trampoline " << (void *)tramp.low << " - "
			<< (void *)tramp.high << endl;
		DBGLOG(sstr);
#endif
		return TRUE;
	}
	return FALSE;
}


#if defined(ISOLATE_MEMORY) || defined(UNKNOWN_DEBUG) || defined(PROACTIVE_CI)
static VOID InstrumentTrace(TRACE trace, VOID *v)
{
	BBL bbl;
	INS ins;

# ifdef UNKNOWN_DEBUG
	string dis;

	if (do_disas) {
		for (bbl = TRACE_BblHead(trace); 
				BBL_Valid(bbl); 
				bbl = BBL_Next(bbl)) {
			for (ins = BBL_InsHead(bbl); 
					INS_Valid(ins);
					ins = INS_Next(ins)) {
				dis = INS_Disassemble(ins);
				INS_InsertCall(ins, IPOINT_BEFORE,
						(AFUNPTR)disas_ins,
						IARG_PTR, new string(dis),
						IARG_END);
			}
		}
	}
# endif

# ifdef PROACTIVE_CI
	ADDRINT addr;
	BOOL img_found = FALSE;

	// Check injection at instrumentation time
	addr = TRACE_Address(trace);
	// Check last used image first
	if (likely(last_fetch_img && last_fetch_img->low_addr <= addr &&
			addr <= last_fetch_img->high_addr)) {
		img_found = TRUE;
	} else if (FindExecutableImageByAddr(addr) != NULL)
		img_found = TRUE;
	if (!img_found) {
		for (bbl = TRACE_BblHead(trace); BBL_Valid(bbl); 
				bbl = BBL_Next(bbl)) {
			for (ins = BBL_InsHead(bbl); INS_Valid(ins);
					ins = INS_Next(ins)) {
				addr = INS_Address(ins);
				if (addr >= tramp.low && addr < tramp.high)
					continue;
				INS_InsertCall(ins, IPOINT_BEFORE,
						(AFUNPTR)CodeInjection, 
						IARG_CONST_CONTEXT, 
						IARG_THREAD_ID,
						IARG_END);
			}
		}
		return;
	}
# endif

# ifdef ISOLATE_MEMORY
	for (bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
		for (ins = BBL_InsHead(bbl); INS_Valid(ins);
				ins = INS_Next(ins)) {
			MemoryIsolate(ins, v);
		}
	}
# endif // ISOLATE_MEMORY
}
#endif

static void ProcessMMaps(void)
{
	FILE *maps_fp;
	ADDRINT low, high;
	char line[2048], map[128];
	int r;
	Image *img;

	maps_fp = fopen("/proc/self/maps", "r");
	if (!maps_fp)
		return;

	while (fgets(line, sizeof(line), maps_fp) != NULL) {
#if __WORDSIZE == 64
		r = sscanf(line, "%lx-%lx", &low, &high);
#else
		r = sscanf(line, "%x-%x", &low, &high);
#endif
		if (r != 2) {
			ERRLOG("ERROR: Could not parse memory maps!\n");
			ForceExit(1);
		}

		r = sscanf(line, "%*x-%*x %*s %*s %*s %*s %128s", map);
		if (r != 1)
			map[0] = '\0';

		// Make high address inclusive
		--high;
#if 0
		cerr << (void *)low << "-" << (void *)high << " "
			<< map << endl;
		//cerr << "Mapping: " << (void *)prot_start << " "
			//<< prot_len << endl;
#endif

		if (strcmp(map, "[vdso]") == 0) {
			img = new Image(low, high);
			assert(img);
			image_map.insert(pair<ADDRINT, Image *>(low, img));
#ifdef ISOLATE_MEMORY
#ifdef MP_DEBUG
			DBGLOG("VDSO ");
#endif
			mp_set(low, high - low, 0);
#endif

		} 
#ifdef ISOLATE_MEMORY
		// All memory maps initially belong to Pin,
		// except the stack and heap
		else if (strcmp(map, "[heap]") == 0) {
			heap_end = high;
#ifdef MP_DEBUG
			DBGLOG("HEAP ");
#endif
			mp_set(low, high - low, 0);
		} else if (strcmp(map, "[stack]") == 0) {
#ifdef MP_DEBUG
			DBGLOG("STACK ");
#endif
			mp_set(low, high - low, 0);
		}
#endif
	}
	fclose(maps_fp);
}

#if 0
/* XXX: Should i remove this feature? Is it still needed? */
static int dl_callback(struct dl_phdr_info *info, size_t size, void *data)
{
	int j;
	stringstream sstr;
	BOOL found = FALSE;
	ADDRINT low_addr, high_addr, addr = (ADDRINT)data;

#ifdef DL_DEBUG
	sstr << "Looking at library " << info->dlpi_name << endl;
	DBGLOG(sstr);
	for (j = 0; j < info->dlpi_phnum; j++) {
		low_addr = info->dlpi_addr + info->dlpi_phdr[j].p_vaddr;
		high_addr = low_addr + info->dlpi_phdr[j].p_memsz;

		sstr << "\tlow " << (void *)low_addr <<
			", high " << (void *)high_addr << endl;
		DBGLOG(sstr);
	}
#endif

	for (j = 0; j < info->dlpi_phnum; j++) {
		low_addr = info->dlpi_addr + info->dlpi_phdr[j].p_vaddr;
		high_addr = low_addr + info->dlpi_phdr[j].p_memsz;


		if (addr >= low_addr && addr <= high_addr) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		AddExecutableImage(info->dlpi_name, low_addr, high_addr);
		pintool_resolved = TRUE;
	}

	return 0;
}
#endif

bool libisr_known_image(ADDRINT addr)
{
	return (FindExecutableImageByAddr(addr) != NULL);
}

bool libisr_code_injection(INT32 sig, const CONTEXT *ctx, 
		const EXCEPTION_INFO *pExceptInfo)
{
	ADDRINT feip;
	unsigned char tmpbuf[1];
	
	feip = PIN_GetContextReg(ctx, REG_INST_PTR);

	// We are trying to execute unknown code, from a memory
	// accessible area --> code-injection
	if (!libisr_known_image(feip) &&
			PIN_SafeCopy(tmpbuf, (VOID *)feip, 1) == 1) {
		/* It's a CI if we are executing from an accessible, but unknown
		 * image */
		return true;
	}
	return false;
}


#ifdef UNKNOWN_DEBUG_ORIG
static void DecodeInstruction(ADDRINT addr, void *buf, size_t size)
{
	xed_decoded_inst_t xedd;
	stringstream sstr;
	char xedbuf[1024];
	int r;
	size_t off = 0;

	while (off < size) {
		xed_decoded_inst_zero_set_mode(&xedd, &dstate);
		r = xed_decode(&xedd, (const xed_uint8_t *)buf + off,
				size - off);
		switch (r) {
		case XED_ERROR_NONE:
			break;
		case XED_ERROR_BUFFER_TOO_SHORT:
			DBGLOG("XED: Not enough bytes to decode instruction\n");
			return;
		case XED_ERROR_GENERAL_ERROR:
			DBGLOG("XED: Unable to decode input\n");
			return;
		default:
			DBGLOG("XED: Some error happened...\n");
			return;
		}

		//xed_decoded_inst_dump(&xedd, xedbuf, sizeof(xedbuf));
		xed_format_att(&xedd, xedbuf, sizeof(xedbuf), addr + off);
		xedbuf[sizeof(xedbuf) - 1] = '\0';
		sstr << "XED  " << (void *)(addr + off) << 
			": " << xedbuf << endl;
		DBGLOG(sstr);
		off += xed_decoded_inst_get_length(&xedd);
	}
}
#endif

static size_t FetchInstruction(void *buf, ADDRINT addr, size_t size,
        EXCEPTION_INFO *pExceptInfo, VOID *v)
{
	size_t effective_size;
	Image *img;
	stringstream sstr;

	fcallcount++;

	// Check last used image first
	if (likely(last_fetch_img && last_fetch_img->low_addr <= addr &&
			addr <= last_fetch_img->high_addr)) {
		image_hits++;
		img = last_fetch_img;
	} else if ((img = FindExecutableImageByAddr(addr)) != NULL)
		last_fetch_img = img;

	effective_size = PIN_SafeCopyEx(buf, (UINT8 *)addr, size, pExceptInfo);

	if (unlikely(img == NULL)) { // No image found
		// Check if this is a trampoline injected by libc
		if (IsTrampoline(addr, buf, effective_size)) {
#ifdef UNKNOWN_DEBUG
			do_disas = TRUE;
			CODECACHE_FlushCache();
#endif
			return effective_size;
		}

		// Use a random keyed image to mangle instructions
		img = random_image;

		sstr << "WARNING: Fetching from uknown image " <<
			(void *)addr << " bytes:" << effective_size << endl;
		OUTLOG(sstr);

#ifdef UNKNOWN_DEBUG
		do_disas = TRUE;
		CODECACHE_FlushCache();
# ifdef UNKNOWN_DEBUG_ORIG
		char hbuf[effective_size * 2 + 1];


		for (size_t i = 0; i < effective_size; i++)
			sprintf(hbuf + i*2, "%02x",
					(int)*((UINT8 *)buf + i));
		hbuf[effective_size * 2] = '\0';
		sstr << "Decoding " << (void *)addr << "=" << 
			hbuf << endl;
		DBGLOG(sstr);
		sstr.str("");
		DecodeInstruction(addr, buf, effective_size);
# endif
#endif

	} else { // Quickly return unencrypted image data
		if (!img->IsEncrypted())
			return effective_size;
	}

	img->DecodeBuf(addr, (uint8_t *)buf, effective_size);
	//dcount += copied;

#ifdef UNKNOWN_DEBUG_ORIG
	char hbuf[effective_size * 2 + 1];


	for (size_t i = 0; i < effective_size; i++)
		sprintf(hbuf + i*2, "%02x",
				(int)*((UINT8 *)buf + i));
	hbuf[effective_size * 2] = '\0';
	sstr << "Decoded " << (void *)addr << "=" << 
		hbuf << endl;
	DBGLOG(sstr);
	sstr.str("");
	DecodeInstruction(addr, buf, effective_size);
#endif

	return effective_size;
}

static bool SectionIsExecutable(IMG img, ADDRINT sec_addr)
{
	SEC sec;

	for (sec = IMG_SecHead(img); 
			SEC_Valid(sec) && SEC_Address(sec) <= sec_addr;
			sec = SEC_Next(sec)) {
		if (!SEC_IsExecutable(sec))
			continue;
		if (SEC_Address(sec) <= sec_addr && 
				sec_addr < (SEC_Address(sec) + SEC_Size(sec)))
			return true;
	}

	return false;
}

/* Relocation actually overwrites the randomized text with the wrong values.
 * We need to patch these locations with the correct values. */
static void PatchRelocations(IMG img, SEC sec, ADDRINT orig_map, Image *image)
{
	ADDRINT imglow, reloc_start, reloc_stop;
	ADDRINT offset, type;
	ADDRINT addend_r, fixed_addr, sym_addr, corrupted_addr;
	INT addend;

	imglow = IMG_LowAddress(img);
	
	// Process relocation entries
	reloc_start = SEC_Address(sec);
	reloc_stop = reloc_start + SEC_Size(sec);
	for (; reloc_start < reloc_stop; reloc_start += 2 * sizeof(ADDRINT)) {
		// Relocation offset
		offset = *(ADDRINT *)reloc_start;

		// Only executable sections have been randomized.
		// Skip all others
		if (!SectionIsExecutable(img, imglow + offset))
			continue;

		// Type of relocation
		type = ELF32_R_TYPE(*(ADDRINT *)(reloc_start + 
					sizeof(ADDRINT)));
		// Randomized addend before it was overwritten by relocation
		addend_r = *(ADDRINT *)(orig_map + offset);
		// The corrupted address produced by relocation
		corrupted_addr = *(ADDRINT *)(imglow + offset);

		// De-randomized addend
		addend = image->DecodeLong(offset, addend_r);

		switch (type) {
		case R_386_PC32:
			// Data = Symbol_addr + addend (stored in offset) -
			// offset
			//cerr << "R_386_PC32 Offset: " << hex << offset << endl;

			// Calculate symbol address
			sym_addr = corrupted_addr - addend_r + offset;
			// Calculate correct address that need to be patched in
			fixed_addr = sym_addr + addend - offset;
			//cerr << "R_386_PC32 fixed addr " << hex << fixed_addr << dec << endl;
			// Randomize fixed_addr
			fixed_addr = image->DecodeLong(offset, fixed_addr);

			// Patch it in 
			*(ADDRINT *)(imglow + offset) = fixed_addr;
			break;

		case R_386_32:
			// Data = Symbol_addr + addend (stored in offset)
			//cerr << "R_386_32 Offset: " << hex << offset << endl;

			// Calculate symbol address
			sym_addr = corrupted_addr - addend_r;
			// Calculate correct address that need to be patched in
			fixed_addr = sym_addr + addend;
			// Randomize fixed_addr
			fixed_addr = image->DecodeLong(offset, fixed_addr);

			// Patch it in 
			*(ADDRINT *)(imglow + offset) = fixed_addr;
			break;

		case R_386_RELATIVE:
			// Data = Base_addr + addend (stored in offset)

			/*
			cerr << "Offset: " << hex << offset <<
				" current: " << corrupted_addr << 
				" original randomized: " << addend_r <<
				" original: " << addend <<
				dec << endl;
			*/

			// Calculate correct address that need to be patched in
			fixed_addr = imglow + addend;
			// Randomize fixed_addr
			fixed_addr = image->DecodeLong(offset, fixed_addr);

			// Patch it in 
			*(ADDRINT *)((ADDRINT)imglow + offset) = fixed_addr;
			break;

		// None or data
		case R_386_NONE:
		case R_386_GLOB_DAT:
			break;

		default:
			cerr << "Type: " << type << endl;
			break;
		}
	}
}

/* Retrieve end address of mapped image */
static ADDRINT GetImageEndAddress(IMG img, ADDRINT start_addr)
{
	int r;
	FILE *fp;
	char line[PATH_MAX + 60];
	stringstream sstr;
	ADDRINT start, end, ret;

	fp = fopen("/proc/self/maps", "r");
	if (!fp) {
		sstr << "WARNING: error opening /proc/self/maps" << endl;
		ERRLOG(sstr);
		goto backup;
	}

	ret = 0;
	while (fgets(line, sizeof(line), fp) != NULL) {
#if __WORDSIZE == 64
		r = sscanf(line, "%lx-%lx", &start, &end);
#else
		r = sscanf(line, "%x-%x", &start, &end);
#endif
		if (r != 2) {
			sstr << "WARNING: error parsing "
				"/proc/self/maps" << endl;
			ERRLOG(sstr);
			break;
		}
		if (start == start_addr) {
			ret = end;
			break;
		}
	}

	fclose(fp);
	if (ret == 0)
		goto backup;

	return (end - 1);

backup:
	return IMG_HighAddress(img);
}

/* Handle image loading */
static VOID LoadImage(IMG img, VOID *v)
{
	ADDRINT imglow, imgend, imgsize;
	stringstream sstr;
	string name;
	SEC sec;
	Image *image;
	int fd;
	void *orig_map;
	const char *fname;
	bool memprotect;

	name = IMG_Name(img);

	// Get image area used by the program
	imglow = IMG_LowAddress(img);
	// Get actual end address of loaded image (just the executable part)
	imgend = GetImageEndAddress(img, imglow);
	imgsize = imgend - imglow + 1;

	sstr << "Loading image " << hex << imglow << '-' << imgend <<
		' ' << name << endl;
	OUTLOG(sstr);
	image = AddExecutableImage(name, imglow, imgend);

#ifdef ISOLATE_MEMORY
	// Mark all loaded images, except the pintool, as accessible
	if (IMG_Name(img).find("isr.so") == string::npos) {
		mp_set(imglow, IMG_HighAddress(img) - imglow, 0);
	}
#endif

	// No more processing for these images
	if (!image->IsEncrypted())
		return;
	/* XXX: Does the main executable need special handling
	if (IMG_IsMainExecutable(img))
		return;*/


	/*
	cerr << IMG_Name(img) << ' ' << hex << imglow << '-' << imgend <<
		" offset: " << IMG_LoadOffset(img) << 
		" main: " << IMG_IsMainExecutable(img) << endl;
	*/

	orig_map = MAP_FAILED;
	fd = -1;
	memprotect = false;
	for (sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
		// XXX: Check if we need to also handle .rel.plt
		if (SEC_Type(sec) != SEC_TYPE_DYNREL ||
				SEC_Name(sec) != ".rel.dyn")
			continue;

		// I need to temporarily map this image as writeable to 
		// patch the relocated locations
		memprotect = true;
		mprotect((void *)imglow, imgsize, 
				PROT_READ | PROT_EXEC | PROT_WRITE);

		// Map the library to obtain the original pre-relocation data
		if (orig_map == MAP_FAILED) {
			fname = IMG_Name(img).c_str();
			fd = open(fname, O_RDONLY);
			if (fd < 0) {
				sstr << "WARNING: could not open file " << fname
					<< " for reading relocation data" 
					<< endl;
				ERRLOG(sstr);
				break;
			}

			orig_map = mmap(NULL, imgsize, PROT_READ, 
					MAP_PRIVATE, fd, 0);
			if (orig_map == MAP_FAILED) {
				sstr << "WARNING: could not map file " << fname 
					<< " for reading relocation data" 
					<< endl;
				ERRLOG(sstr);
				break;
			}
		}

		PatchRelocations(img, sec, (ADDRINT)orig_map, image);
	}

	// Cleanup
	if (orig_map != MAP_FAILED)
		munmap(orig_map, imgsize);
	if (fd >= 0)
		close(fd);
	if (memprotect) 
		mprotect((void *)imglow, imgsize, PROT_READ | PROT_EXEC);
}

static VOID UnloadImage(IMG img, VOID *v)
{
	stringstream sstr;
	ADDRINT low, high;
	
	low = IMG_LowAddress(img);
	high = IMG_HighAddress(img);
	sstr << "Unloading " << hex << (low & ~PAGEOFF_MASK) << '-' << 
		(high | PAGEOFF_MASK) << ' ' << IMG_Name(img) << endl;
	OUTLOG(sstr);
	DeleteExecutableImage(low, high);
}

// This function is called when the application exits
static VOID Fini(INT32 code, VOID *v)
{
	stringstream sstr;

	// Write to a file since cout and cerr maybe closed by the application
	sstr << "Fetch called " << fcallcount << endl;
	sstr << "Last image hits " << image_hits << endl;
	OUTLOG(sstr);
}

static VOID ForceExit(INT32 code)
{
	Fini(code, NULL);
	PIN_ExitProcess(code);
}

static VOID ContextSwitch(THREADID threadIndex, CONTEXT_CHANGE_REASON reason,
		const CONTEXT *from, CONTEXT *to, INT32 info, VOID *v) 
{
#ifdef SIGNAL_DEBUG
	stringstream sstr;
#endif

	switch (reason) {
	case CONTEXT_CHANGE_REASON_SIGNAL:
#ifdef SIGNAL_DEBUG
		sstr << "SIGNAL " << info << endl;
		DBGLOG(sstr);
#endif
		sighandler = TRUE;
		tramp.low = tramp.high = 0;
		break;
	case CONTEXT_CHANGE_REASON_SIGRETURN:
#ifdef SIGNAL_DEBUG
		sstr << "SIGRETURN " << endl;
		DBGLOG(sstr);
#endif
		sighandler = FALSE;
		tramp.low = KERNEL_BOUNDARY;
		tramp.high = 0;
		break;
	default:
		break;
	}
}

static VOID Fork(THREADID tid, const CONTEXT *ctx, VOID *v)
{
	uint16_t randkey;

	// Clear statistics counters
	image_hits = fcallcount = 0;
	// Use a different random key for decoding unknown images on every
	// process
	randkey = (rand() % 65535) + 1;
	random_image->SetKey(randkey);
}

int libisr_init(const char *dbfn)
{
	sqlite3 *db;
	uint16_t randkey;

#ifdef UNKNOWN_DEBUG
	xed_tables_init();
	xed_decode_init();

	xed_state_zero(&dstate);
	xed_state_init(&dstate, XED_MACHINE_MODE_LEGACY_32,
			XED_ADDRESS_WIDTH_32b, XED_ADDRESS_WIDTH_32b);
#endif

	// Claim register for syscall state
	sysstate_reg = PIN_ClaimToolRegister();
	assert(sysstate_reg != REG_INVALID());

#ifdef ISOLATE_MEMORY
	if (!MemoryProtectorInit())
		PIN_ExitProcess(EXIT_FAILURE);
#endif

	// Open the key DB to ensure it is at least initially accessible
	keydbfn = dbfn;
	if ((db = OpenDB()) == NULL)
		return -1;
	sqlite3_close(db);

	// Capture image loading/unloading
	IMG_AddInstrumentFunction(LoadImage, NULL);
	IMG_AddUnloadFunction(UnloadImage, NULL);

	// System calls needs to be tracked both for capturing image loading, 
	// as well as for the memory protector
	PIN_AddSyscallEntryFunction(SyscallEntry, NULL);
	PIN_AddSyscallExitFunction(SyscallExit, NULL);
	PIN_AddThreadStartFunction(ThreadCreate, NULL);
	PIN_AddThreadFiniFunction(ThreadDestroy, NULL);

#if defined(ISOLATE_MEMORY) || defined(UNKNOWN_DEBUG) || defined(PROACTIVE_CI)
	TRACE_AddInstrumentFunction(InstrumentTrace, 0);
#endif

	// An empty image to de-randomize unknown code
	// We can also try to read real random data from /dev/random for the
	// seed
	srand(time(NULL));
	random_image = new Image(0, 0);
	randkey = (rand() % 65535) + 1;
	random_image->SetKey(randkey);

	// Handle signals
	tramp.low = KERNEL_BOUNDARY;
	tramp.high = 0;
	PIN_AddContextChangeFunction(ContextSwitch, NULL);

	// Decode instructions when fetching
	PIN_AddFetchFunction(FetchInstruction, NULL);

	// Register Fini to be called when the application exits
	PIN_AddFiniFunction(Fini, 0);

	// Add VDSO page to mapped images
	ProcessMMaps();

	PIN_AddForkFunction(FPOINT_AFTER_IN_CHILD, Fork, 0);
	//cerr << "Last image " << (VOID *)random_image << endl;

	return 0;
}

void libisr_cleanup(void) 
{
	// XXX
}
