#include <set>
#include <map>
#include <cassert>
#include <iostream>
#include <sstream>

extern "C" {
#ifdef TARGET_LINUX
# include <sys/types.h>
# include <sys/syscall.h>
# include <stdlib.h>
# include <signal.h>
# include <errno.h>
#endif
#include "xed-interface.h"
#include "xed-decode.h"
}

#include "pin.H"
#include "RescuePoint.hpp"
#include "threadstate.hpp"
#include "utils.hpp"
#include "libreassure.hpp"
#include "log.hpp"
#include "cache.h"
#include "writeslog.h"
#include "debug.h"
#include "libcrossdev.hpp"

#ifdef TARGET_LINUX
#include "fork.h"

#define tkill(p, s)		syscall(SYS_tkill, (p), (s))
#define tgkill(pp, p, s)	syscall(SYS_tgkill, (pp), (p), (s))
#endif

//! Code executing in this version returns to the correct one
#define AUTOCORRECT_VERSION	0
//! Code in this version executes "normally"
#define NORMAL_VERSION 		1
//! Code in this version performs checkpointing
#define CHECKPOINT_VERSION	2

//! Pin scratch register for switching between execution versions 
static REG version_reg;

// Thread state
// List head of thread states
static map<THREADID, struct thread_state *> tsmap;
// Lock for modifying list
static PIN_LOCK tsmap_lock;
// Register for holding per thread ts pointer
static REG tsreg;

// Lock to enforce only one active checkpoint at a time
static PIN_LOCK checkpoint_lock;

// Use fork() for performing checkpoints
static BOOL fork_checkpoints = FALSE;

// Blocking checkpoint globals
static BOOL runtime_blocks = FALSE;
static THREADID blocking_tid = -1;
static ADDRINT block_threads = 0, running_threads = 0;
static PIN_LOCK blocking_checkpoint_lock;

// Do we have any blocking RPs
static bool has_blocking_rp = false;

// Traces that we should instrument with a block
// (needs ClientVM lock or Client lock )
static set<ADDRINT> block_traces; 
static PIN_LOCK block_traces_lock;

// Blocking checkpoint defines
#define TBLOCK_SIGNAL 		SIGUSR2

//! Hash map of rescue points, by name of routine
static map<string, RescuePoint *> rescue_points_byname;

//! Hash map of rescue points, by end address of routine
static map<ADDRINT, RescuePoint *> rescue_points_byaddr;

//! Hash map of rescue points, by name of image
static multimap<string, RescuePoint *> rescue_points_byimg;

#ifdef SYSEXIT_DECODE
static xed_state_t dstate;
#endif

// Statistics
//#define COLLECT_STATS
#ifdef COLLECT_STATS
static unsigned long long stats_checkpoints, stats_commits, stats_rollbacks;
static unsigned long long cache_accesses = 0, cache_misses = 0;
#endif


//////////////////
// Helper
//////////////////

#ifdef BLOCKINGRP
static inline VOID InvalidateRoutine(RTN rtn)
{
	ADDRINT start_addr, stop_addr;

	start_addr = RTN_Address(rtn);
	stop_addr = start_addr + RTN_Size(rtn) - 1;
#ifdef INVALIDATE_DEBUG
	UINT32 traces = CODECACHE_InvalidateRange(start_addr, stop_addr);
	stringstream ss;
	ss << " Invalidated range " << (void *)start_addr << '-' << 
		(void *)stop_addr << " = " << traces << endl;
	DBGLOG(ss);
#else
	CODECACHE_InvalidateRange(start_addr, stop_addr);
#endif
}

static inline VOID InvalidateTraceAt(ADDRINT addr)
{
#ifdef INVALIDATE_DEBUG
	UINT32 traces = CODECACHE_InvalidateTraceAtProgramAddress(addr);
	stringstream ss;

	ss << " Invalidated PC " << (void *)addr << " = " << 
		traces << endl;
	DBGLOG(ss);
#else
	CODECACHE_InvalidateTraceAtProgramAddress(addr);
#endif
}

static inline BOOL SignalThreads(struct thread_state *ts)
{
	map<THREADID, struct thread_state *>::iterator it;
	struct thread_state *tsit;
	stringstream ss;
	OS_THREAD_ID ptid;
	BOOL retry, still_running = FALSE;

	ptid = PIN_GetPid();

#ifdef THREAD_DEBUG
	ss << "PIN [" << ts->tid << "] thread is signaling thread " << endl;
	DBGLOG(ss);
#endif

sigall:
	retry = FALSE;
	PIN_GetLock(&tsmap_lock, ts->tid + 1);
	for (it = tsmap.begin(); it != tsmap.end(); it++) {
		tsit = (*it).second;

		if (tsit->real_tid > 0 && ts->tid != tsit->tid && 
				!tsit->in_syscall && !tsit->blocked) {
#ifdef THREAD_DEBUG
			ss << "PIN [" << ts->tid << "] thread " << 
				tsit->real_tid << " will be signaled to "
				"block " << endl;
			DBGLOG(ss);
#endif
			still_running = TRUE;
			if (tgkill(ptid, tsit->real_tid, TBLOCK_SIGNAL) != 0) {
				ss << "WARNING: there was a problem signaling"
					" thread " << tsit->real_tid << ' ' << 
					strerror(errno) << endl;
				OUTLOG(ss);
				retry = TRUE;
				break;
			} // tgkill
		} 
	} // for (it ..)
	PIN_ReleaseLock(&tsmap_lock);

	// If there was an error signaling, sleep and try again
	if (retry) {
		PIN_Sleep(1);
		goto sigall;
	}

#ifdef THREAD_DEBUG
	if (!still_running) {
		ss << "PIN [" << ts->tid << "] no threads to signal" << endl;
		DBGLOG(ss);
	}
#endif
	return still_running;
}

static inline BOOL WaitForThreads(struct thread_state *ts)
{
	map<THREADID, struct thread_state *>::iterator it;
	struct thread_state *tsit;
	BOOL ret = FALSE;
#ifdef THREAD_DEBUG
	stringstream ss;
#endif

	PIN_GetLock(&tsmap_lock, ts->tid + 1);
	for (it = tsmap.begin(); it != tsmap.end(); it++) {
		tsit = (*it).second;
		if (tsit->real_tid > 0 && ts->tid != tsit->tid && 
				!tsit->in_syscall && !tsit->blocked) {
#ifdef THREAD_DEBUG

			ss << "PIN [" << ts->tid << "] thread " << tsit->tid << 
				" still running" << endl;
			DBGLOG(ss);
#endif
			ret = TRUE;
			break;

		}
	}
	PIN_ReleaseLock(&tsmap_lock);

#ifdef THREAD_DEBUG
	if (!ret) {
		ss << "PIN [" << ts->tid << "] all threads blocked" << endl;
		DBGLOG(ss);
	}
#endif

	return ret;
}

static inline VOID RemoveAllBlocks(void)
{
	set<ADDRINT>::iterator it;

	// We don't acquire block_traces_lock because we already have
	// GetVmLock(). Check CheckpointReturn().
	
	// We invalidate all the block traces here
	while ((it = block_traces.begin()) != block_traces.end()) {
		InvalidateTraceAt(*it);
		block_traces.erase(it);
	}
}

static VOID InsertBlock(THREADID tid, ADDRINT pc)
{
	pair<set<ADDRINT>::iterator, bool> ret;

	//*log << "Marking " <<  (void *)pc << " for block trace" << endl;
	PIN_GetLock(&block_traces_lock, tid + 1);
	ret = block_traces.insert(pc);
	if (ret.second)
		InvalidateTraceAt(pc);
	PIN_ReleaseLock(&block_traces_lock);
}
#endif // BLOCKINGRP

#ifdef SYSEXIT_DECODE
static VOID DecodeInstruction(ADDRINT addr, void *buf, size_t size)
{
        xed_decoded_inst_t xedd;
        char xedbuf[1024];
        int r;
        size_t off;
	stringstream ss;

	off = 0;
        while (off < size) {
                xed_decoded_inst_zero_set_mode(&xedd, &dstate);
                r = xed_decode(&xedd, (const xed_uint8_t *)buf + off,
                                size - off);
                switch (r) {
                case XED_ERROR_NONE:
                        break;
                case XED_ERROR_BUFFER_TOO_SHORT:
                        ss << "XED: Not enough bytes to decode "
                                "instruction" << endl;
			DBGLOG(ss);
                        return;
                case XED_ERROR_GENERAL_ERROR:
                        ss << "XED: Unable to decode input" << endl;
			DBGLOG(ss);
                        return;
                default:
                        ss << "XED: Some error happened..." << endl;
			DBGLOG(ss);
                        return;
                }

                //xed_decoded_inst_dump(&xedd, xedbuf, sizeof(xedbuf));
                xed_format_att(&xedd, xedbuf, sizeof(xedbuf), addr + off);
                xedbuf[sizeof(xedbuf) - 1] = '\0';
                ss << "XED  " << (void *)(addr + off) << ": " << xedbuf << endl;
		DBGLOG(ss);
                off += xed_decoded_inst_get_length(&xedd);
        }
}
#endif



////////////////////////////////////////////////////
//	Analysis
////////////////////////////////////////////////////


/**
 * Cache statistics macros
 */
#ifdef COLLECT_STATS
# define CACHE_ACCESSED() do { cache_accesses++; } while (0)
# define CACHE_MISS() do { cache_misses++; } while (0)
#else
# define CACHE_ACCESSED() do { } while (0)
# define CACHE_MISS() do { } while (0)
#endif

#ifdef TARGET_LINUX
/**
 * Bitmap filter analysis functions.
 */
#if FILTER_TYPE == FILTER_BITMAP

/**
 * ForkMarkB is always within the same bucket, so it is defined separately as
 * there in ForkMarkExtB.
 */
static VOID PIN_FAST_ANALYSIS_CALL ForkMarkB(struct thread_state *ts,
		ADDRINT addr)
{
	FLogMarkB(&ts->memcheckp.flog->filter, addr);
}


/**
 * Macro for defining ForkMark functions.
 * These functions return non-zero if the write spills to next bucket.
 */
#define FORKMARK_FUNCTION(W) \
static ADDRINT PIN_FAST_ANALYSIS_CALL ForkMark ## W (struct thread_state *ts, \
		ADDRINT addr)\
{\
	return FLogMark ## W(&ts->memcheckp.flog->filter, addr);\
}
FORKMARK_FUNCTION(W)
FORKMARK_FUNCTION(L)
FORKMARK_FUNCTION(Q)
FORKMARK_FUNCTION(DQ)
FORKMARK_FUNCTION(QQ)


/**
 * Macro for defining ForkMarkExt functions.
 */
#define FORKMARKEXT(W) \
static VOID PIN_FAST_ANALYSIS_CALL ForkMarkExt ## W (struct thread_state *ts, \
		ADDRINT addr)\
{\
	FLogMarkExt ## W (&ts->memcheckp.flog->filter, addr);\
}

FORKMARKEXT(W)
FORKMARKEXT(L)
FORKMARKEXT(Q)
FORKMARKEXT(DQ)
FORKMARKEXT(QQ)

/**
 * Write log filter analysis functions.
 */
#elif FILTER_TYPE == FILTER_WLOG
#define FORKMARK(W, bytes) \
static VOID PIN_FAST_ANALYSIS_CALL ForkMark ## W \
	(struct thread_state *ts, ADDRINT addr)\
{\
	FLOGMARK(&ts->memcheckp.flog->filter, addr, bytes);\
	WRITESCACHE_UPDATE(ts->memcheckp.flog->filter.cache, addr, bytes);\
	CACHE_MISS();\
}
FORKMARK(B, 1)
FORKMARK(W, 2)
FORKMARK(L, 4)
FORKMARK(Q, 8)
FORKMARK(DQ, 16)
FORKMARK(QQ, 32)

#endif

#endif // TARGET_LINUX

/* Analysis routines for write log that store overwritten memory contents */

static ADDRINT PIN_FAST_ANALYSIS_CALL 
LogNeedsExpansion(struct thread_state *ts, UINT32 entries)
{
	return WLogFits(ts->memcheckp.wlog, entries);
}

static VOID LogExpand(struct thread_state *ts)
{
	WLogExtend(ts->memcheckp.wlog);
}

/**
 * Macro for defining CheckCache functions.
 */
#define CHECKCACHE_FUNCTION(suffix)\
static ADDRINT PIN_FAST_ANALYSIS_CALL CheckCache ## suffix \
	(struct thread_state *ts, ADDRINT addr) \
{\
	CACHE_ACCESSED();\
	return WritesCacheCheck ## suffix (ts->memcheckp.wlog->cache, addr);\
}
CHECKCACHE_FUNCTION(B)
CHECKCACHE_FUNCTION(W)
CHECKCACHE_FUNCTION(L)
CHECKCACHE_FUNCTION(Q)
CHECKCACHE_FUNCTION(DQ)
CHECKCACHE_FUNCTION(QQ)

/**
 * Macro for defining CheckCacheOff functions.
 */
#define CHECKCACHEOFF_FUNCTION(suffix)\
static ADDRINT PIN_FAST_ANALYSIS_CALL CheckCacheOff ## suffix \
	(struct thread_state *ts, ADDRINT addr, UINT32 off) \
{\
	CACHE_ACCESSED();\
	return WritesCacheCheck ## suffix \
		(ts->memcheckp.wlog->cache, (addr) + (off));\
}
CHECKCACHEOFF_FUNCTION(B)
CHECKCACHEOFF_FUNCTION(W)
CHECKCACHEOFF_FUNCTION(L)
CHECKCACHEOFF_FUNCTION(Q)
CHECKCACHEOFF_FUNCTION(DQ)
CHECKCACHEOFF_FUNCTION(QQ)

/**
 * Generic macro for copying data to temporary variable
 */
#ifdef USE_SAFECOPY
# define COPY_DATA(data, addr, len) \
	do {\
		if (unlikely(PIN_SafeCopy(&(data), (VOID *)(addr),\
						(len)) < (len)))\
			return;\
	} while (0)
#else
# define COPY_DATA(data, addr, len, type) \
	do {\
		(data) = *(type *)addr;\
	} while (0)
#endif

/**
 * Macro for defining LogWrite functions.
 */
#define LOGWRITE_FUNCTION(suffix, len, type, umember) \
static VOID PIN_FAST_ANALYSIS_CALL LogWrite ## suffix \
	(struct thread_state *ts, ADDRINT addr)\
{\
	type data;\
	COPY_DATA(data, addr, len, type);\
	WLOG_WRITE(ts->memcheckp.wlog, addr, data, len, umember);\
	WRITESCACHE_UPDATE(ts->memcheckp.wlog->cache, addr, len);\
	CACHE_MISS();\
}
LOGWRITE_FUNCTION(B, 1, UINT8, byte)
LOGWRITE_FUNCTION(W, 2, UINT16, word)
LOGWRITE_FUNCTION(L, 4, UINT32, dword)
LOGWRITE_FUNCTION(Q, 8, UINT64, qword)

/**
 * Macro for defining LogWriteOff functions.
 */
#define LOGWRITEOFF_FUNCTION(suffix, len, type, umember) \
static VOID PIN_FAST_ANALYSIS_CALL LogWriteOff ## suffix \
	(struct thread_state *ts, ADDRINT addr, UINT32 off)\
{\
	type data;\
	COPY_DATA(data, (addr) + (off), len, type);\
	WLOG_WRITE(ts->memcheckp.wlog, (addr) + (off), data, len, umember);\
	WRITESCACHE_UPDATE(ts->memcheckp.wlog->cache, (addr) + (off), len);\
	CACHE_MISS();\
}
LOGWRITEOFF_FUNCTION(B, 1, UINT8, byte)
LOGWRITEOFF_FUNCTION(W, 2, UINT16, word)
LOGWRITEOFF_FUNCTION(L, 4, UINT32, dword)
LOGWRITEOFF_FUNCTION(Q, 8, UINT64, qword)


/**
 * Double quad-word writes use copy instead of direct assignment.
 */
static VOID PIN_FAST_ANALYSIS_CALL LogWriteDQ(struct thread_state *ts, 
		ADDRINT addr)
{
	WLOG_WRITE_COPY(ts->memcheckp.wlog, addr, 16, dqword);
	WRITESCACHE_UPDATE(ts->memcheckp.wlog->cache, addr, 16);
	CACHE_MISS();
}

/**
 * Double quad-word writes use copy instead of direct assignment.
 */
static VOID PIN_FAST_ANALYSIS_CALL LogWriteOffDQ(struct thread_state *ts, 
		ADDRINT addr, UINT32 off)
{
	WLOG_WRITE_COPY(ts->memcheckp.wlog, (addr + off), 16, dqword);
	WRITESCACHE_UPDATE(ts->memcheckp.wlog->cache, addr + off, 16);
	CACHE_MISS();
}

/**
 * Quad quad-word writes use copy instead of direct assignment.
 */
static VOID PIN_FAST_ANALYSIS_CALL LogWriteQQ(struct thread_state *ts, 
		ADDRINT addr)
{
	WLOG_WRITE_COPY(ts->memcheckp.wlog, addr, 32, qqword);
	WRITESCACHE_UPDATE(ts->memcheckp.wlog->cache, addr, 32);
	CACHE_MISS();
}

/**
 * Quad quad-word writes use copy instead of direct assignment.
 */
static VOID PIN_FAST_ANALYSIS_CALL LogWriteOffQQ(struct thread_state *ts, 
		ADDRINT addr, UINT32 off)
{
	WLOG_WRITE_COPY(ts->memcheckp.wlog, (addr + off), 32, qqword);
	WRITESCACHE_UPDATE(ts->memcheckp.wlog->cache, addr + off, 32);
	CACHE_MISS();
}

// Block threads is global since we can only have one checkpoint at a time
static ADDRINT PIN_FAST_ANALYSIS_CALL ShouldBlock(void)
{
	return block_threads;
}

static VOID Block(struct thread_state *ts)
{
	if (ts->tid != blocking_tid) {
#ifdef CHECKPOINT_DEBUG
		stringstream ss;

		ss << "PIN [" << ts->tid << "] blocking" << endl;
		DBGLOG(ss);
#endif
		ts->blocked = true;
		PIN_GetLock(&blocking_checkpoint_lock, ts->tid + 1);
		PIN_ReleaseLock(&blocking_checkpoint_lock);
		ts->blocked = false;
#ifdef CHECKPOINT_DEBUG
		ss << "PIN [" << ts->tid << "] resuming" << endl;
		DBGLOG(ss);
#endif
	}
}

static VOID Block2(struct thread_state *ts)
{
	if (ts->tid != blocking_tid && block_threads) {
#ifdef CHECKPOINT_DEBUG
		stringstream ss;

		ss << "PIN [" << ts->tid << "] blocking2" << endl;
		DBGLOG(ss);
#endif
		ts->blocked = true; 
		PIN_GetLock(&blocking_checkpoint_lock, ts->tid + 1);
		PIN_ReleaseLock(&blocking_checkpoint_lock);
		ts->blocked = false; 
#ifdef CHECKPOINT_DEBUG
		ss << "PIN [" << ts->tid << "] resuming2" << endl;
		DBGLOG(ss);
#endif
	} 
}

static ADDRINT Checkpoint(struct thread_state *ts, 
		const CONTEXT *ctx, RescuePoint *rp)
{
	stringstream ss;

#ifdef CHECKPOINT_DEBUG
	ss << "PIN [" << ts->tid << "] enter checkpoint " << rp << endl;
	DBGLOG(ss);
#endif

#ifdef COLLECT_STATS
	stats_checkpoints++;
#endif

#ifdef BLOCKINGRP
	if (rp->blockType() == RPBLOCKOTHERS) {
		// I may block due to multiple threads trying to enter a
		// checkpoint
		ts->blocked = true;
		//ASSERT(has_blocking_rp);
		// Initiate block of other threads
		PIN_GetLock(&blocking_checkpoint_lock, ts->tid + 1);
		ts->blocked = false;
# ifdef CHECKPOINT_DEBUG
		ss << "PIN [" << ts->tid << "] blocking threads" << endl;
		DBGLOG(ss);
# endif // CHECKPOINT_DEBUG
		block_threads = 1;
		blocking_tid = ts->tid;
		PIN_Yield(); // Allow other threads to block
		// Wait for all threads to be blocked
		if (runtime_blocks)
			while (SignalThreads(ts))
				PIN_Sleep(1);
		else
			while (WaitForThreads(ts))
				PIN_Sleep(1);
	}
#endif // BLOCKINGRP

	switch (ts->state) {
	case NORMAL:
#ifdef CHECKPOINT_DEBUG
		ss << "PIN [" << ts->tid << "] setting up checkpoint " << endl;
		DBGLOG(ss);
#endif
		CheckpointAlloc(ts, (fork_checkpoints)? FORK_CHECKP : 
				WLOG_CHECKP);
		PIN_SaveContext(ctx, ts->checkpoint);
		ts->rp = rp;

		// This enables the writes logging for the thread
		ts->state = CHECKPOINTING;

#if TARGET_LINUX
		if (fork_checkpoints) {
			if (CheckpointFork(ts->memcheckp.flog) != 0)
				abort();
		}
#endif

		break;

	case CHECKPOINTING:
		ss << "Checkpoint within checkpoint not supported" << endl;
		ERRLOG(ss);
		PIN_ExitProcess(1);
		break;

	default:
		ss << "Unexpected thread state " << ts->state <<
			" at checkpoint" << endl;
		ERRLOG(ss);
		PIN_ExitProcess(1);
		break;
	}

#ifdef CHECKPOINT_DEBUG
	ss << "PIN [" << ts->tid << "] checkpoint setup done" << endl;
	ERRLOG(ss);
#endif

	// Update current version
	return CHECKPOINT_VERSION;
}

#ifdef BLOCKINGRP
// Exiting blocking RP, Remove blocks from threads.
// Assumes that the RP is of type RPBLOCKOTHERS
static VOID ExitBlockingRP(struct thread_state *ts, BOOL vmlock = FALSE)
{
	//ASSERT(has_blocking_rp);
	block_threads = 0;
	blocking_tid = -1;
	if (runtime_blocks) {
		if (vmlock)
			GetVmLock();
		RemoveAllBlocks();
		if (vmlock)
			ReleaseVmLock();
	}
#ifdef CHECKPOINT_DEBUG
	stringstream ss;

	ss << "PIN [" << ts->tid << "] resume threads" << endl;
	DBGLOG(ss);
#endif
	PIN_ReleaseLock(&blocking_checkpoint_lock);
}
#endif // BLOCKINGRP

static ADDRINT CheckpointReturn(struct thread_state *ts,
		ADDRINT *ret_p, BOOL hasret, ADDRINT retval)
{
	stringstream ss;

#ifdef CHECKPOINT_DEBUG
	ss << "PIN [" << ts->tid << "] checkpoint return" << endl;
	DBGLOG(ss);
#endif

	switch (ts->state) {
	case CHECKPOINTING:
		// Commit checkpoint
#ifdef CHECKPOINT_DEBUG
		ss << "PIN [" << ts->tid << "] committing" << endl;
		DBGLOG(ss);
#endif

#ifdef TARGET_LINUX
		if (fork_checkpoints) {
			// Let the forked process know that we have committed
			CheckpointForkCommit(ts->memcheckp.flog);
		}
#endif

#ifdef COLLECT_STATS
		stats_commits++;
#endif
		break;

	case ROLLINGBACK:
		// Rollback checkpoint
#ifdef CHECKPOINT_DEBUG
		ss << "PIN [" << ts->tid << "] rolling back";
#endif

		// Correct return value according to RP
		if (hasret) {
#ifdef CHECKPOINT_DEBUG
			ss << ", setting return value to " << (int)retval;
#endif
			*ret_p = retval;
		}
#ifdef CHECKPOINT_DEBUG
		ss << ", and exiting rescue point" << endl;
		DBGLOG(ss);
#endif

#ifdef COLLECT_STATS
		stats_rollbacks++;
#endif

		break;

	default:
		DBGLOG("Unexpected thread state " + decstr(ts->state) + 
			" at checkpoint return\n");
		PIN_ExitProcess(EXIT_FAILURE);
	}

#ifdef BLOCKINGRP
	// Remove blocks
	if (ts->rp->blockType() == RPBLOCKOTHERS) {
		// TRUE for acquiring VMlock
		ExitBlockingRP(ts, TRUE);
	}
#endif // BLOCKINGRP

	// Free checkpoint memory
	CheckpointFree(ts, (fork_checkpoints)? FORK_CHECKP : WLOG_CHECKP);

	// Set state to normal
	ts->state = NORMAL;

	// Set instrumentation version to normal
	return NORMAL_VERSION;
}


////////////////////////////////////////////////////
//	Instrumentation
////////////////////////////////////////////////////

#ifdef TARGET_LINUX
#if FILTER_TYPE == FILTER_BITMAP
/**
 * Instrument memory writes to update filter with the memory locations written
 * by a thread. This handler is for bitmap filters.
 *
 * @param ins Instrumented write instruction
 * @param width Width of write in bits
 */
static VOID ForkWritesHandler(INS ins, UINT32 width)
{
	stringstream ss;
	AFUNPTR logwrite_fptr, logwrite_ext_fptr;

        switch (width) {
        case 8:
		logwrite_fptr = (AFUNPTR)ForkMarkB;
		logwrite_ext_fptr = NULL;
		break;
        case 16:
                logwrite_fptr = (AFUNPTR)ForkMarkW;
		logwrite_ext_fptr = (AFUNPTR)ForkMarkExtW;
                break;
        case 32:
                logwrite_fptr = (AFUNPTR)ForkMarkL;
		logwrite_ext_fptr = (AFUNPTR)ForkMarkExtL;
                break;
        case 64:
                logwrite_fptr = (AFUNPTR)ForkMarkQ;
		logwrite_ext_fptr = (AFUNPTR)ForkMarkExtQ;
                break;
        case 128:
                logwrite_fptr = (AFUNPTR)ForkMarkDQ;
		logwrite_ext_fptr = (AFUNPTR)ForkMarkExtDQ;
                break;
        case 256:
                logwrite_fptr = (AFUNPTR)ForkMarkQQ;
		logwrite_ext_fptr = (AFUNPTR)ForkMarkExtQQ;
                break;
        default:
                ss << "[ERROR] reassure could not find width(" << width << 
                        ") to write operand" << endl;
                ERRLOG(ss);
                PIN_ExitProcess(1);
        }

	if (logwrite_ext_fptr) {
		INS_InsertIfPredicatedCall(ins, IPOINT_BEFORE, 
				(AFUNPTR)logwrite_fptr,
				IARG_FAST_ANALYSIS_CALL,
				IARG_REG_VALUE, tsreg, 
				IARG_MEMORYWRITE_EA,
				IARG_END);
		INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, 
				(AFUNPTR)logwrite_ext_fptr,
				IARG_FAST_ANALYSIS_CALL,
				IARG_REG_VALUE, tsreg, 
				IARG_MEMORYWRITE_EA,
				IARG_END);
	} else {
		INS_InsertPredicatedCall(ins, IPOINT_BEFORE, 
				(AFUNPTR)logwrite_fptr,
				IARG_FAST_ANALYSIS_CALL,
				IARG_REG_VALUE, tsreg, 
				IARG_MEMORYWRITE_EA,
				IARG_END);
	}
}
#elif FILTER_TYPE == FILTER_WLOG
/**
 * Instrument memory writes to update filter with the memory locations written
 * by a thread. This handler is for writes log filters.
 *
 * @param ins Instrumented write instruction
 * @param width Width of write in bits
 */
static VOID ForkWritesHandler(INS ins, UINT32 width)
{
	stringstream ss;
	AFUNPTR logwrite_fptr, checkwrite_fptr;

	switch (width) {
	case 8:
		logwrite_fptr = (AFUNPTR)ForkMarkB;
		checkwrite_fptr = (AFUNPTR)CheckCacheB;
		break;
	case 16:
		logwrite_fptr = (AFUNPTR)ForkMarkW;
		checkwrite_fptr = (AFUNPTR)CheckCacheW;
		break;
	case 32:
		logwrite_fptr = (AFUNPTR)ForkMarkL;
		checkwrite_fptr = (AFUNPTR)CheckCacheL;
		break;
	case 64:
		logwrite_fptr = (AFUNPTR)ForkMarkQ;
		checkwrite_fptr = (AFUNPTR)CheckCacheQ;
		break;
	case 128:
		logwrite_fptr = (AFUNPTR)ForkMarkDQ;
		checkwrite_fptr = (AFUNPTR)CheckCacheDQ;
		break;
	case 256:
		logwrite_fptr = (AFUNPTR)ForkMarkQQ;
		checkwrite_fptr = (AFUNPTR)CheckCacheQQ;
		break;
	default:
		ss << "[ERROR] reassure could not find width(" << width << 
			") to write operand" << endl;
		ERRLOG(ss);
		PIN_ExitProcess(1);
		return; // Keep the compiler happy
	}

#if 1
	INS_InsertIfPredicatedCall(ins, IPOINT_BEFORE, 
			(AFUNPTR)checkwrite_fptr,
			IARG_FAST_ANALYSIS_CALL,
			IARG_REG_VALUE, tsreg, 
			IARG_MEMORYWRITE_EA,
			IARG_END);
	INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, 
			(AFUNPTR)logwrite_fptr,
			IARG_FAST_ANALYSIS_CALL,
			IARG_REG_VALUE, tsreg, 
			IARG_MEMORYWRITE_EA,
			IARG_END);
#else
	INS_InsertPredicatedCall(ins, IPOINT_BEFORE, 
			(AFUNPTR)logwrite_fptr,
			IARG_FAST_ANALYSIS_CALL,
			IARG_REG_VALUE, tsreg, 
			IARG_MEMORYWRITE_EA,
			IARG_END);
#endif
}

#else
# error "unsupported filter type for fork checkpointing"
#endif

#endif // TARGET_LINUX

/**
 * Compute how many WLog entries are needed to store the target of a write
 * of width bits
 *
 * @param width	The size of the write in bits.
 * @return	The number of the entries needed, or -1 on error.
 */
static int WLogComputeEntries(UINT32 width)
{
	int slot;
	UINT32 w;

	// Writing less than 8 bits is not possible
	assert((width & 0x7) == 0);

	// The width's bits can help us calculate on how many entries we need.
	// 8, 16, 32, 64, 128, 256 (max 504 bits) take a slot each
	for (slot = 0; width & 0x1ff; slot++) {
		// clear the least significant bit set
		width &= width - 1;
	}

	// 512 bits or longer widths require many max size slots
	// Limit to WLOG_BLOCK_SIZE_MIN slots
	while (width && slot < WLOG_BLOCK_SIZE_MIN) {
		// clear the least significant bit set and assign to w
		w = width & (width - 1);

		// How many slots to store this many bits
		slot += (w ^ width) >> 8;

		width = w;
	}

	if (slot < WLOG_BLOCK_SIZE_MIN)
		return slot;

	// Slot is too big
	WARNLOG("Write width " + decstr(width) + " requires too many slots (" +
			decstr(slot) + ") in the writes log\n");
	return -1;
}

/**
 * Check if the log needs to expand to accommodate multiple entries.
 *
 * @param ins	  Pin write instruction
 * @param entries Number of entries required
 */
static inline void WLogExpand(INS ins, UINT32 entries)
{
	// Expand writes log if necessary
        INS_InsertIfCall(ins, IPOINT_BEFORE, 
                        (AFUNPTR)LogNeedsExpansion, IARG_FAST_ANALYSIS_CALL,
                        IARG_REG_VALUE, tsreg, 
			IARG_UINT32, entries,
			IARG_END);
        INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)LogExpand, 
			IARG_REG_VALUE, tsreg, IARG_END);
}

/**
 * Instrument a write to enter an entry in the writes log. Can only log a part
 * of the write to support long writes.
 *
 * @param ins	Pin instruction
 * @param width	Width of data to be saved
 * @param off	Offset from effect address of the write to save
 */
static inline void WLogWritePartial(INS ins, UINT32 width, UINT32 off)
{
	AFUNPTR logwrite_fptr, checkwrite_fptr;
	stringstream ss;

	switch (width) {
	case 8:
		logwrite_fptr = (AFUNPTR)LogWriteOffB;
		checkwrite_fptr = (AFUNPTR)CheckCacheOffB;
		break;
	case 16:
		logwrite_fptr = (AFUNPTR)LogWriteOffW;
		checkwrite_fptr = (AFUNPTR)CheckCacheOffW;
		break;
	case 32:
		logwrite_fptr = (AFUNPTR)LogWriteOffL;
		checkwrite_fptr = (AFUNPTR)CheckCacheOffL;
		break;
	case 64:
		logwrite_fptr = (AFUNPTR)LogWriteOffQ;
		checkwrite_fptr = (AFUNPTR)CheckCacheOffQ;
		break;
	case 128:
		logwrite_fptr = (AFUNPTR)LogWriteOffDQ;
		checkwrite_fptr = (AFUNPTR)CheckCacheOffDQ;
		break;
	case 256:
		logwrite_fptr = (AFUNPTR)LogWriteOffQQ;
		checkwrite_fptr = (AFUNPTR)CheckCacheOffQQ;
		break;
	default:
		ERRLOG("Fatal did not expect write width " + decstr(width) +
				"> 256 bits in a single WLog slot\n");
		PIN_ExitProcess(1);
		return;
	}


#if 1 // Check if we have logged this entry before using an associative cache
	INS_InsertIfCall(ins, IPOINT_BEFORE, checkwrite_fptr,
			IARG_FAST_ANALYSIS_CALL, IARG_REG_VALUE, tsreg, 
			IARG_MEMORYWRITE_EA, 
			IARG_UINT32, off,
			IARG_END);
	// Log it if necessary
	INS_InsertThenCall(ins, IPOINT_BEFORE, logwrite_fptr,
			IARG_FAST_ANALYSIS_CALL, IARG_REG_VALUE, tsreg, 
			IARG_MEMORYWRITE_EA,
			IARG_UINT32, off,
			IARG_END);
#else // Log everything
	INS_InsertCall(ins, IPOINT_BEFORE, logwrite_fptr,
			IARG_FAST_ANALYSIS_CALL, IARG_REG_VALUE, tsreg, 
			IARG_MEMORYWRITE_EA,
			IARG_UINT32, off,
			IARG_END);
#endif
}

/**
 * Instrument a write to enter a single entry in the writes log. 
 *
 * @param ins	Pin instruction
 * @param width	Width of data to be saved
 */
static inline void WLogWriteSingle(INS ins, UINT32 width)
{
	AFUNPTR logwrite_fptr, checkwrite_fptr;
	stringstream ss;

	switch (width) {
	case 8:
		logwrite_fptr = (AFUNPTR)LogWriteB;
		checkwrite_fptr = (AFUNPTR)CheckCacheB;
		break;
	case 16:
		logwrite_fptr = (AFUNPTR)LogWriteW;
		checkwrite_fptr = (AFUNPTR)CheckCacheW;
		break;
	case 32:
		logwrite_fptr = (AFUNPTR)LogWriteL;
		checkwrite_fptr = (AFUNPTR)CheckCacheL;
		break;
	case 64:
		logwrite_fptr = (AFUNPTR)LogWriteQ;
		checkwrite_fptr = (AFUNPTR)CheckCacheQ;
		break;
	case 128:
		logwrite_fptr = (AFUNPTR)LogWriteDQ;
		checkwrite_fptr = (AFUNPTR)CheckCacheDQ;
		break;
	case 256:
		logwrite_fptr = (AFUNPTR)LogWriteQQ;
		checkwrite_fptr = (AFUNPTR)CheckCacheQQ;
		break;
	default:
		ERRLOG("Fatal did not expect write width " + decstr(width) +
				"> 256 bits in a single WLog slot\n");
		PIN_ExitProcess(1);
		return;
	}


#if 1 // Check if we have logged this entry before using an associative cache
	INS_InsertIfCall(ins, IPOINT_BEFORE, checkwrite_fptr,
			IARG_FAST_ANALYSIS_CALL, IARG_REG_VALUE, tsreg, 
			IARG_MEMORYWRITE_EA, 
			IARG_END);
	// Log it if necessary
	INS_InsertThenCall(ins, IPOINT_BEFORE, logwrite_fptr,
			IARG_FAST_ANALYSIS_CALL, IARG_REG_VALUE, tsreg, 
			IARG_MEMORYWRITE_EA,
			IARG_END);
#else // Log everything
	INS_InsertCall(ins, IPOINT_BEFORE, logwrite_fptr,
			IARG_FAST_ANALYSIS_CALL, IARG_REG_VALUE, tsreg, 
			IARG_MEMORYWRITE_EA, IARG_END);
#endif
}

/**
 * Instrument long writes.
 *
 * @param ins	  Pin instruction.
 * @param width	  Width of data to be saved.
 * @param entries Number entries that the write will take.
 */
static void WLogWriteLong(INS ins, UINT32 width, UINT32 entries)
{
	UINT32 w, bits, slots, off;

	// Writing less than 8 bits is not possible
	assert((width & 0x7) == 0);

	// The width's bits can help us calculate on how many entries we need.
	// 8, 16, 32, 64, 128, 256 (max 504 bits) take a slot each
	for (off = 0; width & 0x1ff; entries--) {
		// clear the least significant bit set and assign to w
		w = width & (width - 1);

		bits = w ^ width;

#ifdef WLOG_DEBUG
		DBGLOG("WLogWriteLong " + decstr(bits) + 
				"@" + decstr(off) + "\n");
#endif
		WLogWritePartial(ins, bits, off);

		off += bits;
		width = w;
	}

	// 512 bits or longer widths require many max size slots
	// Limit to WLOG_BLOCK_SIZE_MIN slots
	while (width) {
		// clear the least significant bit set and assign to w
		w = width & (width - 1);

		// How many slots to store this many bits
		
		for (slots = (w ^ width) >> 8; slots--; entries--, off += 256) {
#ifdef WLOG_DEBUG
			DBGLOG("WLogWriteLong 256@" + decstr(off) + "\n");
#endif
			WLogWritePartial(ins, 256, off);
		}

		width = w;
	}

	assert(entries == 0);
}

// Handle memory writes when a writes log is used for checkpointing
static VOID WLogWritesHandler(INS ins, UINT32 width) 
{
	int entries;

	entries = WLogComputeEntries(width);
	assert(entries > 0);

	WLogExpand(ins, entries);

	if (entries == 1) {
		WLogWritePartial(ins, width, 0);
	} else {
#ifdef WLOG_DEBUG
		DBGLOG("Long write! Width=" + decstr(width) + 
				" entries=" + decstr(entries) + ".\n");
#endif
		WLogWriteLong(ins, width, entries);
	}
}

static VOID MemWriteHandler(INS ins, VOID *v) 
{
	UINT32 i, width;

	if (!INS_IsMemoryWrite(ins))
		return;

	for (i = 0, width = 0; i < INS_OperandCount(ins); i++)
		if (INS_OperandIsMemory(ins, i) && INS_OperandWritten(ins, i)) {
			width = INS_OperandWidth(ins, i);
			break;
		}

#ifdef TARGET_LINUX
	if (fork_checkpoints)
		ForkWritesHandler(ins, width);
	else
#endif
		WLogWritesHandler(ins, width);
}

static VOID CheckpointInstrument(TRACE trace, RescuePoint *rp)
{
	INS ins;
	BBL bbl;

	// XXX: For this to work accurately i also need to block signals
	// while in a rescue point
	
	for (bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
		for (ins = BBL_InsHead(bbl); INS_Valid(ins); 
				ins = INS_Next(ins)) {
			//*log << " >> " << INS_Disassemble(ins) << endl;
	
			// Memory writes need to be rolled backed
			// XXX: Optimize this
			if (INS_IsMemoryWrite(ins))
				MemWriteHandler(ins, NULL);

			if (INS_IsRet(ins) && rp) {
				DBGLOG("Installing Checkpoint return\n");
				INS_InsertCall(ins, IPOINT_BEFORE, 
					(AFUNPTR)CheckpointReturn,
					IARG_REG_VALUE, tsreg, 
					IARG_FUNCRET_EXITPOINT_REFERENCE,
					IARG_BOOL, rp->hasReturnValue(),
					IARG_ADDRINT, rp->returnValue(),
					IARG_RETURN_REGS, version_reg,
					IARG_END);
				BBL_SetTargetVersion(bbl, NORMAL_VERSION);
			}
		}
	}
}

static VOID BlockInstrument(TRACE trace)
{
	set<ADDRINT>::iterator it;
	ADDRINT addr;

	addr = TRACE_Address(trace);

	PIN_GetLock(&block_traces_lock, 1);
	for (it = block_traces.begin(); it != block_traces.end(); it++) {
		if (*it >= addr && (*it - addr) < TRACE_Size(trace)) {
			/*
			*log << "Trace instrumenting with block " << 
				(void *)TRACE_Address(trace) << endl;
			*/
#if 0
			TRACE_InsertIfCall(trace, IPOINT_BEFORE, 
					(AFUNPTR)ShouldBlock2, 
					IARG_FAST_ANALYSIS_CALL, 
					IARG_THREAD_ID, IARG_END);
			TRACE_InsertThenCall(trace, IPOINT_BEFORE, 
					(AFUNPTR)Block2, IARG_THREAD_ID, 
					IARG_END);
#else
			TRACE_InsertCall(trace, IPOINT_BEFORE, (AFUNPTR)Block2,
					IARG_REG_VALUE, tsreg, IARG_END);
#endif
			break;
		}
	}
	PIN_ReleaseLock(&block_traces_lock);
}

/**
 * Instrument a trace to switch to the correct instrumentation version
 *
 * @param trace Pin trace to instrument
 */
static inline void AutocorrectVersion(TRACE trace)
{
	INS ins;
	BBL bbl;
	for (bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
		for (ins = BBL_InsHead(bbl); INS_Valid(ins); 
				ins = INS_Next(ins)) {
			INS_InsertVersionCase(ins, version_reg, 
					NORMAL_VERSION, 
					NORMAL_VERSION, 
					IARG_END);
			INS_InsertVersionCase(ins, version_reg, 
					CHECKPOINT_VERSION, 
					CHECKPOINT_VERSION, 
					IARG_END);
		} // for (ins)
	} // for (bbl)
}

/**
 * Find a rescue point (if it exists) for a given instruction or trace address
 *
 * @param addr Address to look for
 *
 * @return Pointer to a rescue point object
 */
static inline RescuePoint *find_rp(ADDRINT addr)
{
	map<ADDRINT, RescuePoint *>::iterator it;
	RescuePoint *rp;

	//cerr << "find_rp(" << hex << "0x" << addr << ')' << endl;

	// Returns an iterator pointing to the first element in the container
	// whose key does not compare less than x (using the container's
	// comparison object), i.e. it is either equal or greater.
	// This should be a rescue point for the function containing the
	// address, since the map uses the routine end address as a key, or the
	// rescue point for a function following addr.
	it = rescue_points_byaddr.lower_bound(addr);
	if (it == rescue_points_byaddr.end())
		return NULL;

	rp = it->second;
	if (addr >= rp->address() && addr <= rp->endAddress())
		return rp;
	return NULL;
}

/**
 * Instrument a trace
 *
 * @param trace	Pin trace to instrument
 * @param v 	Opaque value passed by the call back
 */
static VOID trace_instrument(TRACE trace, VOID *v)
{
	ADDRINT version, addr;
	stringstream ss;
	RescuePoint *rp = NULL;

	// Trace address
	addr = TRACE_Address(trace);

	// Current version we are instrumenting in
	version = TRACE_Version(trace);

#ifdef TRACE_DEBUG
	ss << "Instrumenting trace (v." << version << ") at " << 
		(void *)addr << endl;
	DBGLOG(ss);
#endif


	// Correct instrumentation version
	if (version == AUTOCORRECT_VERSION) {
		AutocorrectVersion(trace);
		return;
	}

	// We setup blocking RP stuff, only if one existed in the configuration
	// XXX: Debug
	if (has_blocking_rp) {
		if (runtime_blocks) { 
			// runtime blocks are inserted on demand
			if (block_threads)
				// We need to instruments traces with blocks
				// because a thread has entered a blocking RP
				BlockInstrument(trace);
		} else {
			// fixed blocks are inserted at the beginning of 
			// every trace
			TRACE_InsertIfCall(trace, IPOINT_BEFORE,
					(AFUNPTR)ShouldBlock,
					IARG_FAST_ANALYSIS_CALL, IARG_END);
			TRACE_InsertThenCall(trace, IPOINT_BEFORE,
					(AFUNPTR)Block, 
					IARG_REG_VALUE, tsreg,
					IARG_END);
		}
	}

	// Find a rescue point
	rp = find_rp(addr);

	if (rp && rp->retAddress() == 0) {
		ERRLOG("Found a RP but it is not associated with a RET "
			"and it will be ignored\n");
		rp = NULL;
	}

	// Instrument code not checkpointing
	if (version == NORMAL_VERSION) {
		// First trace in RP
		if (rp && rp->address() == addr) {
			ss << "Installing rescue point for " << rp << endl;
			OUTLOG(ss);

			// Insert code to enter the checkpoint
			INS_InsertCall(BBL_InsHead(TRACE_BblHead(trace)), 
					IPOINT_BEFORE, (AFUNPTR)Checkpoint, 
					IARG_REG_VALUE, tsreg, 
					IARG_CONST_CONTEXT, IARG_PTR, rp,
					IARG_RETURN_REGS, version_reg,
					IARG_END);
			// Switch to checkpointing version
			BBL_SetTargetVersion(TRACE_BblHead(trace), 
					CHECKPOINT_VERSION);

			// The rest of the code should also be instrumented for
			// checkpointing
			CheckpointInstrument(trace, rp);
		} 
	} else if (version == CHECKPOINT_VERSION) {
		// Instrument code with checkpointing code
		CheckpointInstrument(trace, rp);
	}
}

/**
 * Find a RET instruction for a RP using a routine. Routine must be already
 * open.
 *
 * @param rtn Pin routine
 * @param rp Pointer to rescue point 
 *
 * @return true if a RET instruction was found, or false otherwise
 */
static bool find_rtn_rp_ret(RTN rtn, RescuePoint *rp)
{
	INS ins;
	bool found = false;

	// Iterate over all of the routines's instructions, until we are beyond
	// the RP
	for (ins = RTN_InsHead(rtn);
			INS_Valid(ins) && INS_Address(ins) <= rp->endAddress(); 
			ins = INS_Next(ins)) {

		// Skip all instructions before the RP and non-RETs
		if ((INS_Address(ins) < rp->address()) || !INS_IsRet(ins))
			continue;

		// Set and return
		rp->setRetAddress(INS_Address(ins));
		found = true;
#ifdef RESCUE_POINT_DEBUG
		stringstream ss;
		ss << "RP: " << rp << " found RET at 0x" << hex << 
			rp->retAddress() << endl;
		DBGLOG(ss);
#endif
		break;
	}

	return found;
}


/**
 * Check if a rescue point is defined for a routine. 
 * Rescue points defined by name are identified and their address resolved here.
 * We also look for a RET instruction for the RP.
 *
 * @param rtn Pin routine
 */
static void find_rtn_rp(RTN rtn)
{
	string rname, dname;
	ADDRINT addr;
	stringstream ss;
	RescuePoint *rp;
	map<string, RescuePoint *>::iterator rp_it;

	addr = RTN_Address(rtn);

	RTN_Open(rtn);
	rname = RTN_Name(rtn);
	// Obtain demangled routine name
	dname = PIN_UndecorateSymbolName(rname, UNDECORATION_NAME_ONLY);
	// Find RP by name
	rp_it = rescue_points_byname.find(dname);

	if (rp_it == rescue_points_byname.end())
		goto close_rtn;

	rp = rp_it->second;

	// Set the address range of the routine
	rp->setAddress(addr);
	rp->setEndAddress(addr + RTN_Size(rtn) - 1);

	// Now that we know it's address, make sure we can find it  easily
	rescue_points_byaddr.insert(
			pair<ADDRINT, RescuePoint *>(rp->endAddress(), rp));

	// Find a RET instruction for this routine
	find_rtn_rp_ret(rtn, rp);

#ifdef RESCUE_POINT_DEBUG
	ss << "RP: " << rp << " resolved name to address" << endl;
	DBGLOG(ss);
#endif

close_rtn:
	RTN_Close(rtn);
}

/**
 * Find all RPs relative to a particular binary image, adjust their address to
 * match the address the image was loaded at, and add it to the RPs by address
 * structure for quick lookup. 
 *
 * @param img_name Image name
 * @param img_addr Address the image was loaded at
 */
static void adjust_rela_rps(const string &img_name, ADDRINT img_addr)
{
	pair<multimap<string, RescuePoint *>::iterator, 
		multimap<string, RescuePoint *>::iterator> ret;
	multimap<string, RescuePoint *>::iterator rp_it;
	pair<map<ADDRINT, RescuePoint *>::iterator, bool> ins_ret;
	RescuePoint *rp;

	// Get all RPs for this image by name
	ret = rescue_points_byimg.equal_range(img_name);
	for (rp_it = ret.first; rp_it != ret.second; 
			rescue_points_byimg.erase(rp_it++)) {
		rp = (*rp_it).second;

		// Set RPs base address (can only be done once)
		rp->setBaseAddress(img_addr);

#ifdef RESCUE_POINT_DEBUG
		stringstream ss;
		ss << "RP: " << rp << " adjusted relative addresses" << endl;
		DBGLOG(ss);
#endif



		// Add to map by end address for quick lookup
		ins_ret = rescue_points_byaddr.insert(
			pair<ADDRINT, RescuePoint *>(rp->endAddress(), rp));
		if (!ins_ret.second) {
			stringstream ss;

			ss << "Duplicate rescue point for " << rp << endl;
			ERRLOG(ss);
		}
	}
}

/**
 * Find a RET instruction for all RPs defined for an image by address.
 *
 * @param start_addr	Image start address
 * @param end_addr	Image end address
 */
static void find_image_rps_ret(ADDRINT start_addr, ADDRINT end_addr)
{
	RTN rtn;
	RescuePoint *rp;
	map<ADDRINT, RescuePoint *>::iterator it;

	// First RP with end address bigger than the image's start address
	it = rescue_points_byaddr.upper_bound(start_addr);
	for (; it != rescue_points_byaddr.end(); it++) {
		rp = it->second;
		if (rp->address() < start_addr || rp->endAddress() > end_addr)
			continue;
		rtn = RTN_FindByAddress(rp->address());
		if (!RTN_Valid(rtn))
			continue;

		RTN_Open(rtn);
		find_rtn_rp_ret(rtn, rp);
		RTN_Close(rtn);
	}
}

/**
 * Image instrumentation routine.
 * Looks for any RPs defined for this image, and RET instructions to associate
 * with them.
 *
 * @param img	Pin image reference
 * @param v 	Opaque value passed by the call back
 */
static VOID image_instrument(IMG img, VOID *v)
{
	SEC sec;
	RTN rtn;
	string img_name;
	ADDRINT img_addr;
	size_t sep_idx;

	// Get image name stripping the path
	sep_idx = IMG_Name(img).find_last_of(DIRECTORY_SEP_CHAR);
	if (sep_idx == string::npos)
		img_name = IMG_Name(img);
	else 
		img_name = IMG_Name(img).substr(sep_idx + 1);
	// Image base address
	img_addr = IMG_StartAddress(img);

	// Process any relative rescue points defined for this image, adjust
	// their start/end address, and add them to the find by address map
	adjust_rela_rps(img_name, img_addr);

	// Find the RET instructions for all RPs defined for this image
	find_image_rps_ret(img_addr, IMG_HighAddress(img));

	// Process RPs defined by name 
	for (sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
		for (rtn = SEC_RtnHead(sec); 
				RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
			find_rtn_rp(rtn);
		}
	}
}

static VOID ThreadStart(THREADID tid, CONTEXT *ctx, INT32 flags, VOID *v)
{
	struct thread_state *newts;
	stringstream ss;
	ADDRINT version;

	running_threads++;
	// PIN stops all threads while in this call
#ifdef THREAD_DEBUG
	ss << "PIN [" << tid << "] thread starting, real: " << get_thread_id()
		<< ", total running = " << running_threads << endl;
	DBGLOG(ss);
#endif // THREAD_DEBUG
#ifdef BLOCKINGRP
	if (block_threads && runtime_blocks)
		InsertBlock(tid, PIN_GetContextReg(ctx, REG_INST_PTR));
#endif // BLOCKINGRP

	// Allocate new thread state
	newts = (struct thread_state *)calloc(1, sizeof(struct thread_state));
	ASSERT(newts, "Failed to allocate thread state data\n");
	// Initialize
	ThreadstateInit(newts, tid);
	// Assign it to the thread
	PIN_SetContextReg(ctx, tsreg, (ADDRINT)newts);

	// Set version of newly created thread
	version = PIN_GetContextReg(ctx, version_reg);
	if (version == AUTOCORRECT_VERSION) {
		PIN_SetContextReg(ctx, version_reg, NORMAL_VERSION);
#ifdef VERSION_DEBUG
		ss << "Thread " << tid << " switched to version " << 
			NORMAL_VERSION << endl;
		DBGLOG(ss);
#endif
	}

	// Add to global list of thread states
	PIN_GetLock(&tsmap_lock, tid + 1);
	tsmap[tid] = newts;
	PIN_ReleaseLock(&tsmap_lock);
}

static VOID ThreadFini(THREADID tid, const CONTEXT *ctx, INT32 code, VOID *v)
{
	struct thread_state *ts;

	--running_threads;

	ts = (struct thread_state *)PIN_GetContextReg(ctx, tsreg);
	ASSERT(ts, "Invalid pointer to thread data in thread termination\n");

#ifdef THREAD_DEBUG
	{
		stringstream ss;

		ss << "PIN [" << tid << "] thread exiting, real: " << 
			ts->real_tid << ", remaining = " << 
			running_threads << endl;
		DBGLOG(ss);
	}
#endif // THREAD_DEBUG

	if (ts->state == CHECKPOINTING) {
#ifdef TARGET_LINUX
		if (fork_checkpoints)
			CheckpointForkCommit(ts->memcheckp.flog);
#endif
#ifdef BLOCKINGRP
		// Remove blocks
		if (ts->rp->blockType() == RPBLOCKOTHERS) {
			ExitBlockingRP(ts);
		}
#endif // BLOCKINGRP
	}
	ThreadstateCleanup(ts, (fork_checkpoints)? FORK_CHECKP: WLOG_CHECKP);
	tsmap.erase(tid);
	free(ts);
}

#ifdef BLOCKINGRP
static BOOL BlockThreadHandler(THREADID tid, INT32 sig, CONTEXT *ctx, 
		BOOL hasHandler, const EXCEPTION_INFO *pExceptInfo, VOID *v)
{
	struct thread_state *ts;

	ts = (struct thread_state *)PIN_GetContextReg(ctx, tsreg);

	if (block_threads && !ts->blocked) {
#ifdef THREAD_DEBUG
		stringstream ss;
		ss << "PIN [" << ts->tid << "] block signal delivered" << endl;
		DBGLOG(ss);
#endif // THREAD_DEBUG
		InsertBlock(tid, PIN_GetContextReg(ctx, REG_INST_PTR));
	} 
#ifdef THREAD_DEBUG
	else {
		stringstream ss;
		ss << "PIN [" << tid << "] block signal ignored" << endl;
		DBGLOG(ss);
	}
#endif // THREAD_DEBUG
	return FALSE;
}
#endif //BLOCKINGRP

static VOID SysEnter(THREADID tid, CONTEXT *ctx, SYSCALL_STANDARD std, VOID *v)
{
	struct thread_state *ts;

	ts = (struct thread_state *)PIN_GetContextReg(ctx, tsreg);
	ts->in_syscall = PIN_GetSyscallNumber(ctx, std);

	if (ts->state == CHECKPOINTING) {
#if 0
		if (!HandleCheckpointSysEnter(tid, ts, ctx, std)) {
			stringstream ss;

			ss << "PIN [" << tid << "] WARNING performing system "
				"call " << ts->in_syscall << " in checkpoint" 
				<< endl;
			ERRLOG(ss);
		}
#else
		stringstream ss;

		ss << "PIN [" << tid << "] WARNING performing system "
			"call " << ts->in_syscall << " in checkpoint" 
			<< endl;
		OUTLOG(ss);
#endif
	}
}

static VOID SysExit(THREADID tid, CONTEXT *ctx, SYSCALL_STANDARD std, VOID *v)
{
	struct thread_state *ts;

	ts = (struct thread_state *)PIN_GetContextReg(ctx, tsreg);
	ts->in_syscall = 0;

	if (ts->state == CHECKPOINTING) {
		/* HandleCheckpointSysExit(tid, tsarray + tid, ctx,
				PIN_GetSyscallReturn(ctx, std));*/
	}
#ifdef BLOCKINGRP
	else if (runtime_blocks && block_threads) {
# ifdef THREAD_DEBUG
		stringstream ss;

		ss << "PIN [" << tid << "] just exited syscall and needs to "
			"be blocked" << endl;
		DBGLOG(ss);
# endif	
# ifdef SYSEXIT_DECODE
		char hbuf[20];
		ADDRINT eip;
		size_t copied;

		eip = PIN_GetContextReg(ctx, REG_INST_PTR);
		copied = PIN_SafeCopy(hbuf, (void *)eip, 20);
		DecodeInstruction(eip, hbuf, copied);
# endif
		InsertBlock(tid, PIN_GetContextReg(ctx, REG_INST_PTR));
	}
#endif // BLOCKINGRP
}

static VOID Fini(INT32 code, VOID *v)
{
	stringstream ss;

#ifdef COLLECT_STATS
	ss << "Process pid " << PIN_GetPid() << " exiting..." << endl;
	ss << "Number of checkpoints: " << stats_checkpoints << endl;
	ss << "Number of rollbacks  : " << stats_rollbacks << endl;
	ss << "Number of commits    : " << stats_commits << endl;
	ss << "Cache hit ratio      : " << ((float)(cache_accesses - 
				cache_misses) / cache_accesses) * 100;
	ss << " misses=" << cache_misses << " hits=" << 
		cache_accesses - cache_misses << endl;
	DBGLOG(ss);
#endif
}

// Free all thread states except the one forking
static VOID Fork(THREADID tid, const CONTEXT *ctx, VOID *v)
{
	map<THREADID, struct thread_state *>::iterator it;
	struct thread_state *ts, *tsit;
	checkp_t type;

	ts = (struct thread_state *)PIN_GetContextReg(ctx, tsreg);
	type = (fork_checkpoints)? FORK_CHECKP : WLOG_CHECKP;

	if (ts->state != NORMAL) {
		ERRLOG("Fork while in checkpoint not supported\n");
		PIN_ExitProcess(1);
	}

	PIN_GetLock(&tsmap_lock, tid + 1);
	// Delete all other threads except myself
	for (it = tsmap.begin(); it != tsmap.end(); ) {
		tsit = (*it).second;
		if (tsit == ts) {
			it++;
		} else {
			ThreadstateCleanup(tsit, type);
			free(tsit);
			tsmap.erase(it++);
		}
	}
	PIN_ReleaseLock(&tsmap_lock);

	// Setup globals
	blocking_tid = -1;
	block_threads = 0;
	running_threads = 1;
#ifdef COLLECT_STATS
	stats_checkpoints = stats_rollbacks = stats_commits = 0;
	cache_accesses = cache_misses = 0;
#endif
}

// Return pointer to thread state structure based on thread id
static struct thread_state *ThreadstateFind(THREADID tid)
{
	map<THREADID, struct thread_state *>::iterator it;

	it = tsmap.find(tid);
	if (it != tsmap.end())
		return (*it).second;
	return NULL;
}

/**
 * Generic fault handler for both external and Pin internal faults.
 *
 * @param tid Thread id that received the fault
 * @param ctx Pointer to CPU state, can be also updated
 *
 * @return true if recovery was triggered, or false otherwise
 */
static bool GenericFaultHandler(struct thread_state *ts, CONTEXT *ctx)
{
	stringstream ss;

	if (ts->state == ROLLINGBACK) {
		ss << "PIN [" << ts->tid << "] "
			"Fault while rolling back!" << endl <<
			"Submit a bug report!" << endl;
		ERRLOG(ss);
		return false;
	} else if (ts->state != CHECKPOINTING) {
		ss << "PIN [" << ts->tid << "] Fault "
			"outside a rescue point" << endl;
		OUTLOG(ss);
		return false;
	}

	ss << "PIN [" << ts->tid << "] Fault within a rescue point" << endl;
#ifdef CHECKPOINT_DEBUG
	DBGLOG(ss);
#else
	OUTLOG(ss);
#endif

	// Set state as rolling back
	ts->state = ROLLINGBACK;

	// Rollback memory changes. We can only update the context here, not
	// from CheckpointReturn() because analysis routines cannot update the
	// CONTEXT
	PIN_LockClient();
	CheckpointRollback(ts, 
			(fork_checkpoints)? FORK_CHECKP : WLOG_CHECKP, ctx);
	PIN_UnlockClient();

	// Correct instrumentation version
	PIN_SetContextReg(ctx, version_reg, CHECKPOINT_VERSION);

	// Redirect execution to the RET instruction associated with the RP
	PIN_SetContextReg(ctx, REG_INST_PTR, ts->rp->retAddress());
#ifdef CHECKPOINT_DEBUG
	ss << "PIN [" << ts->tid << "] execution will be redirected to 0x" << 
		hex << ts->rp->retAddress() << endl;
	DBGLOG(ss);
#endif

	return true;
}

/**
 * Handle a fault such as a signal or exception that would terminate the
 * application.
 *
 * @param tid Thread id that received the fault
 * @param ctx Pointer to CPU state, can be also updated
 *
 * @return One of the types defined by the REASSURE_EHANDLING_RESULT
 */
reassure_ehandling_result_t libreassure_handle_fault(THREADID tid, CONTEXT *ctx)
{
	struct thread_state *ts;
	string desc;

	ts = ThreadstateFind(tid);
	ASSERT(ts, "Invalid pointer to thread data in fault handling\n");

	if (GenericFaultHandler(ts, ctx))
		return RHR_RESCUED;
	return RHR_ERROR;
}

/**
 * Handle an internal fault.
 *
 * @param tid Thread id that received the fault
 * @param pExceptInfo Pointer to exception information
 * @param ctx Pointer to store CPU state, if we need to resume execution with
 * updated state (when returning RHR_UPDATESTATE).
 *
 * @return One of the types defined by the REASSURE_EHANDLING_RESULT
 */
reassure_ehandling_result_t libreassure_handle_internal_fault(THREADID tid, 
		CONTEXT *ctx, EXCEPTION_INFO *info)
{
	map<THREADID, struct thread_state *>::iterator it;
	struct thread_state *ts;

	ts = ThreadstateFind(tid);
	ASSERT(ts, "Invalid pointer to thread data in fault handling\n");

#if FILTER_TYPE == FILTER_WLOG && defined(TARGET_LINUX)
	if (fork_checkpoints) {
		if (filter_handle_internal_fault(&ts->memcheckp.flog->filter,
					info))
			return RHR_HANDLED;
		CheckpointForkBail(ts->memcheckp.flog);
		return RHR_ERROR;
	}
#endif
	return RHR_ERROR;
}

/**
 * Return true if it can be determined that the exception was due to a
 * null pointer.
 *
 * @param einfo 	Pointer to exception information
 *
 * @return true if it is a null pointer or false otherwise
 */
bool reassure_exception_nullpointer(const EXCEPTION_INFO *einfo)
{
	EXCEPTION_CODE code;
	ADDRINT fault_addr;

	if (!einfo)
		return false;

	code = PIN_GetExceptionCode(einfo);
	if (PIN_GetExceptionClass(code) != EXCEPTCLASS_ACCESS_FAULT)
		return false;

	if (!PIN_GetFaultyAccessAddress(einfo, &fault_addr))
		return false;

	if (fault_addr != 0)
		return false;

	return true;
}

/**
 * Initialize the reassure library.
 *
 * @param conf_fn Filename to read configuration (i.e., rescue points
 * definitions)
 * @param rb True if runtime blocks should be used
 * @param usefork True if fork() should be used to perform checkpoints
 *
 * @return 0 on success, or -1 on error
 */
int reassure_init(const char *conf_fn, BOOL rb, BOOL usefork)
{
#ifdef SYSEXIT_DECODE
        xed_tables_init();
        xed_decode_init();

        xed_state_zero(&dstate);
        xed_state_init(&dstate, XED_MACHINE_MODE_LEGACY_32,
                        XED_ADDRESS_WIDTH_32b, XED_ADDRESS_WIDTH_32b);
#endif

	if (ParseConf(conf_fn, rescue_points_byname, rescue_points_byaddr, 
				rescue_points_byimg, &has_blocking_rp) != 0)
		return -1;

	// Allocate version register
	version_reg = PIN_ClaimToolRegister();
	if (version_reg == REG_INVALID()) {
no_scratch_reg:
		PIN_ERROR("Could not allocate scratch register\n");
		return -1;
	}

	// Thread state keeping
	tsreg = PIN_ClaimToolRegister();
	if (tsreg == REG_INVALID())
		goto no_scratch_reg;
	PIN_InitLock(&tsmap_lock);

	runtime_blocks = rb;
	PIN_InitLock(&blocking_checkpoint_lock);
	PIN_InitLock(&block_traces_lock);
	PIN_InitLock(&checkpoint_lock);

	fork_checkpoints = usefork;

	PIN_AddThreadStartFunction(ThreadStart, 0);
	PIN_AddThreadFiniFunction(ThreadFini, 0);

	TRACE_AddInstrumentFunction(trace_instrument, 0);
	IMG_AddInstrumentFunction(image_instrument, 0);

	PIN_AddSyscallEntryFunction(SysEnter, 0);
	PIN_AddSyscallExitFunction(SysExit, 0);

#if BLOCKINGRP
	if (has_blocking_rp && runtime_blocks) {
		PIN_UnblockSignal(TBLOCK_SIGNAL, TRUE);
		PIN_InterceptSignal(TBLOCK_SIGNAL, BlockThreadHandler, 0);
	}
#endif // BLOCKINGRP

	PIN_AddFiniFunction(Fini, 0);
#ifdef TARGET_LINUX
	PIN_AddForkFunction(FPOINT_AFTER_IN_CHILD, Fork, 0);
#endif

	return 0;
}
