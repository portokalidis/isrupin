/*
 * TODO:
 * 	- add support for file descriptor duplication via fcntl(2)
 * 	- add support for non PF_INET* sockets
 * 	- add support for recvmmsg(2)
 */
#include <set>
#include <iostream>
#include <sstream>
#include <stdint.h>
#include <cassert>
#include <fstream>

extern "C" {
#include <errno.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/net.h>
#include <sys/mman.h>
}

#include "pin.H"
#include "branch_pred.h"
#include "libdft_api.h"
#include "libdft_core.h"
#include "tagmap.h"
#include "dta_tool.hpp"
#include "syscall_desc.h"
#include "log.hpp"

#include "dta_tool_opts.hpp"

#ifdef TARGET_IA32E
/* Define number of recv(2) */
# ifndef NR_recv
#  define __NR_recv 1073
# endif
#endif

#define WORD_LEN	4	/* size in bytes of a word value */
#define SYS_SOCKET	1	/* socket(2) demux index for socketcall */

/* thread context */
extern REG thread_ctx_ptr;

/* ins descriptors */
extern ins_desc_t ins_desc[XED_ICLASS_LAST];

/* alert callback */
static struct alert_callbacks callbacks;


/* default suffixes for dynamic shared libraries */
#define DLIB_SUFF	".so"
#define DLIB_SUFF_ALT	".so."

/* set of interesting descriptors (sockets) */
static set<int> fdset;

/* Maximum length of path type spefication strings in fslist entries */
#define PATH_TYPE_MAX	10

/* Set of tracked files */
static set<string> tainted_files;

/* Macro to forward char pointer p, skipping all whitespace */
#define skip_whitespace(p) \
	do {\
		while (isspace(*(p)) && *(p) != '\0') (p)++;\
	} while (0)

/* Use list of tainted files */
static bool use_tainted_filelist = false;


/*
 * 32-bit register assertion (taint-sink, DFT-sink)
 *
 * called before an instruction that uses a register
 * for an indirect branch; returns a positive value
 * whenever the register value or the target address
 * are tainted
 *
 * returns:	0 (clean), >0 (tainted)
 */
static ADDRINT PIN_FAST_ANALYSIS_CALL
assert_reg32(thread_ctx_t *thread_ctx, uint32_t reg, uint32_t addr)
{
	/* 
	 * combine the register tag along with the tag
	 * markings of the target address
	 */
	return thread_ctx->vcpu.gpr[reg] | tagmap_getl(addr);
}

/*
 * 16-bit register assertion (taint-sink, DFT-sink)
 *
 * called before an instruction that uses a register
 * for an indirect branch; returns a positive value
 * whenever the register value or the target address
 * are tainted
 *
 * returns:	0 (clean), >0 (tainted)
 */
static ADDRINT PIN_FAST_ANALYSIS_CALL
assert_reg16(thread_ctx_t *thread_ctx, uint32_t reg, uint32_t addr)
{
	/* 
	 * combine the register tag along with the tag
	 * markings of the target address
	 */
	return (thread_ctx->vcpu.gpr[reg] & VCPU_MASK16)
		| tagmap_getw(addr);
}

/*
 * 32-bit memory assertion (taint-sink, DFT-sink)
 *
 * called before an instruction that uses a memory
 * location for an indirect branch; returns a positive
 * value whenever the memory value (i.e., effective address),
 * or the target address, are tainted
 *
 * returns:	0 (clean), >0 (tainted)
 */
static ADDRINT PIN_FAST_ANALYSIS_CALL
assert_mem32(ADDRINT paddr, ADDRINT taddr)
{
	return tagmap_getl(paddr) | tagmap_getl(taddr);
}

/*
 * 16-bit memory assertion (taint-sink, DFT-sink)
 *
 * called before an instruction that uses a memory
 * location for an indirect branch; returns a positive
 * value whenever the memory value (i.e., effective address),
 * or the target address, are tainted
 *
 * returns:	0 (clean), >0 (tainted)
 */
static ADDRINT PIN_FAST_ANALYSIS_CALL
assert_mem16(ADDRINT paddr, ADDRINT taddr)
{
	return tagmap_getw(paddr) | tagmap_getw(taddr);
}

/*
 * instrument the jmp/call instructions
 *
 * install the appropriate DTA/DFT logic (sinks)
 *
 * @ins:	the instruction to instrument
 */
static void
dta_instrument_jmp_call(INS ins)
{
	/* temporaries */
	REG reg;

	/* 
	 * we only care about indirect calls;
	 * optimized branch
	 */
	if (unlikely(INS_IsIndirectBranchOrCall(ins))) {
		/* perform operand analysis */

		/* call via register */
		if (INS_OperandIsReg(ins, 0)) {
			/* extract the register from the instruction */
			reg = INS_OperandReg(ins, 0);

			/* size analysis */

			/* 32-bit register */
			if (REG_is_gr32(reg))
				/*
				 * instrument assert_reg32() before branch;
				 * conditional instrumentation -- if
				 */
				INS_InsertIfCall(ins,
					IPOINT_BEFORE,
					(AFUNPTR)assert_reg32,
					IARG_FAST_ANALYSIS_CALL,
					IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG32_INDX(reg),
					IARG_REG_VALUE, reg,
					IARG_END);
			else
				/* 16-bit register */
				/*
				 * instrument assert_reg16() before branch;
				 * conditional instrumentation -- if
				 */
				INS_InsertIfCall(ins,
					IPOINT_BEFORE,
					(AFUNPTR)assert_reg16,
					IARG_FAST_ANALYSIS_CALL,
					IARG_REG_VALUE, thread_ctx_ptr,
					IARG_UINT32, REG16_INDX(reg),
					IARG_REG_VALUE, reg,
					IARG_END);
		}
		else {
		/* call via memory */
			/* size analysis */
				
			/* 32-bit */
			if (INS_MemoryReadSize(ins) == WORD_LEN)
				/*
				 * instrument assert_mem32() before branch;
				 * conditional instrumentation -- if
				 */
				INS_InsertIfCall(ins,
					IPOINT_BEFORE,
					(AFUNPTR)assert_mem32,
					IARG_FAST_ANALYSIS_CALL,
					IARG_MEMORYREAD_EA,
					IARG_BRANCH_TARGET_ADDR,
					IARG_END);
			/* 16-bit */
			else
				/*
				 * instrument assert_mem16() before branch;
				 * conditional instrumentation -- if
				 */
				INS_InsertIfCall(ins,
					IPOINT_BEFORE,
					(AFUNPTR)assert_mem16,
					IARG_FAST_ANALYSIS_CALL,
					IARG_MEMORYREAD_EA,
					IARG_BRANCH_TARGET_ADDR,
					IARG_END);
		}
		/*
		 * instrument alert() before branch;
		 * conditional instrumentation -- then
		 */
		INS_InsertThenCall(ins,
			IPOINT_BEFORE,
			(AFUNPTR)callbacks.alter_flow,
			IARG_THREAD_ID,
			IARG_CONST_CONTEXT,
			IARG_BRANCH_TARGET_ADDR,
			IARG_END);
	}
}

/*
 * instrument the ret instruction
 *
 * install the appropriate DTA/DFT logic (sinks)
 *
 * @ins:	the instruction to instrument
 */
static void
dta_instrument_ret(INS ins)
{
	/* size analysis */
				
	/* 32-bit */
	if (INS_MemoryReadSize(ins) == WORD_LEN)
		/*
		 * instrument assert_mem32() before ret;
		 * conditional instrumentation -- if
		 */
		INS_InsertIfCall(ins,
			IPOINT_BEFORE,
			(AFUNPTR)assert_mem32,
			IARG_FAST_ANALYSIS_CALL,
			IARG_MEMORYREAD_EA,
			IARG_BRANCH_TARGET_ADDR,
			IARG_END);
	/* 16-bit */
	else
		/*
		 * instrument assert_mem16() before ret;
		 * conditional instrumentation -- if
		 */
		INS_InsertIfCall(ins,
			IPOINT_BEFORE,
			(AFUNPTR)assert_mem16,
			IARG_FAST_ANALYSIS_CALL,
			IARG_MEMORYREAD_EA,
			IARG_BRANCH_TARGET_ADDR,
			IARG_END);
	
	/*
	 * instrument alert() before ret;
	 * conditional instrumentation -- then
	 */
	INS_InsertThenCall(ins,
		IPOINT_BEFORE,
		(AFUNPTR)callbacks.alter_flow,
		IARG_THREAD_ID,
		IARG_CONST_CONTEXT,
		IARG_BRANCH_TARGET_ADDR,
		IARG_END);
}


/*
 * read(2) handler (taint-source)
 */
static void
post_read_hook(syscall_ctx_t *ctx)
{
        /* read() was not successful; optimized branch */
        if (unlikely((long)ctx->ret <= 0))
                return;
	
	/* taint-source */
	if (fdset.find(ctx->arg[SYSCALL_ARG0]) != fdset.end())
        	/* set the tag markings */
	        tagmap_setn(ctx->arg[SYSCALL_ARG1], (size_t)ctx->ret);
	else
        	/* clear the tag markings */
	        tagmap_clrn(ctx->arg[SYSCALL_ARG1], (size_t)ctx->ret);
}

/*
 * mmap(2) handler (taint-source or taint cleanup)
 */
static void
post_mmap_hook(syscall_ctx_t *ctx)
{
	/* syscall arguments */
	int flags, fd;
	size_t length = (size_t)ctx->arg[SYSCALL_ARG1];
	/* Should we taint or clean the mapped area */
	bool tainted = false;

	/* mmap() was not successful; optimized branch */
	if (unlikely((void *)ctx->ret == MAP_FAILED))
		return;

	/* estimate length; optimized branch */
	if (unlikely((length % PAGE_SZ) != 0))
		length += (PAGE_SZ - (length % PAGE_SZ));

	flags = (int)ctx->arg[SYSCALL_ARG3];
	/* grow downwards; optimized branch */
	if (unlikely(flags & MAP_GROWSDOWN))
		/* fix starting address */
		ctx->ret -= (length - 1);

	/* anonymous or files map */
	fd = (int)ctx->arg[SYSCALL_ARG4];
	if ((flags & MAP_ANONYMOUS) == 0 && fd >= 0) {
		/* taint-source */
		if (fdset.find(fd) != fdset.end())
			tainted = true;
	}

	if (tainted)
		/* set the tag markings */
		tagmap_setn((size_t)ctx->ret, length);
	else
		/* clear the tag markings */
		tagmap_clrn((size_t)ctx->ret, length);
}


/*
 * readv(2) handler (taint-source)
 */
static void
post_readv_hook(syscall_ctx_t *ctx)
{
	/* iterators */
	int i;
	struct iovec *iov;
	set<int>::iterator it;

	/* bytes copied in a iovec structure */
	size_t iov_tot;

	/* total bytes copied */
	size_t tot = (size_t)ctx->ret;

	/* readv() was not successful; optimized branch */
	if (unlikely((long)ctx->ret <= 0))
		return;
	
	/* get the descriptor */
	it = fdset.find((int)ctx->arg[SYSCALL_ARG0]);

	/* iterate the iovec structures */
	for (i = 0; i < (int)ctx->arg[SYSCALL_ARG2] && tot > 0; i++) {
		/* get an iovec  */
		iov = ((struct iovec *)ctx->arg[SYSCALL_ARG1]) + i;
		
		/* get the length of the iovec */
		iov_tot = (tot >= (size_t)iov->iov_len) ?
			(size_t)iov->iov_len : tot;
	
		/* taint interesting data and zero everything else */	
		if (it != fdset.end())
                	/* set the tag markings */
                	tagmap_setn((size_t)iov->iov_base, iov_tot);
		else
                	/* clear the tag markings */
                	tagmap_clrn((size_t)iov->iov_base, iov_tot);

                /* housekeeping */
                tot -= iov_tot;
        }
}

#ifdef TARGET_IA32
/*
 * socketcall(2) handler
 *
 * attach taint-sources in the following
 * syscalls:
 * 	socket(2), accept(2), recv(2),
 * 	recvfrom(2), recvmsg(2)
 *
 * everything else is left intact in order
 * to avoid taint-leaks
 */
static void
post_socketcall_hook(syscall_ctx_t *ctx)
{
	/* message header; recvmsg(2) */
	struct msghdr *msg;

	/* iov bytes copied; recvmsg(2) */
	size_t iov_tot;

	/* iterators */
	size_t i;
	struct iovec *iov;
	set<int>::iterator it;
	
	/* total bytes received */
	size_t tot;
	
	/* socket call arguments */
	unsigned long *args = (unsigned long *)ctx->arg[SYSCALL_ARG1];

	/* demultiplex the socketcall */
	switch ((int)ctx->arg[SYSCALL_ARG0]) {
		case SYS_SOCKET:
			/* not successful; optimized branch */
			if (unlikely((long)ctx->ret < 0))
				return;

			/*
			 * PF_INET and PF_INET6 descriptors are
			 * considered interesting
			 */
			if (likely(args[SYSCALL_ARG0] == PF_INET ||
				args[SYSCALL_ARG0] == PF_INET6))
				/* add the descriptor to the monitored set */
				fdset.insert((int)ctx->ret);

			/* done */
			break;
		case SYS_ACCEPT:
		case SYS_ACCEPT4:
			/* not successful; optimized branch */
			if (unlikely((long)ctx->ret < 0))
				return;
			/*
			 * if the socket argument is interesting,
			 * the returned handle of accept(2) is also
			 * interesting
			 */
			if (likely(fdset.find(args[SYSCALL_ARG0]) !=
						fdset.end()))
				/* add the descriptor to the monitored set */
				fdset.insert((int)ctx->ret);
		case SYS_GETSOCKNAME:
		case SYS_GETPEERNAME:
			/* not successful; optimized branch */
			if (unlikely((long)ctx->ret < 0))
				return;

			/* addr argument is provided */
			if ((void *)args[SYSCALL_ARG1] != NULL) {
				/* clear the tag bits */
				tagmap_clrn(args[SYSCALL_ARG1],
					*((int *)args[SYSCALL_ARG2]));
				
				/* clear the tag bits */
				tagmap_clrn(args[SYSCALL_ARG2], sizeof(int));
			}
			break;
		case SYS_SOCKETPAIR:
			/* not successful; optimized branch */
			if (unlikely((long)ctx->ret < 0))
				return;
	
			/* clear the tag bits */
			tagmap_clrn(args[SYSCALL_ARG3], (sizeof(int) * 2));
			break;
		case SYS_RECV:
			/* not successful; optimized branch */
			if (unlikely((long)ctx->ret <= 0))
				return;
			
			/* taint-source */	
			if (fdset.find((int)args[SYSCALL_ARG0]) != fdset.end())
				/* set the tag markings */
				tagmap_setn(args[SYSCALL_ARG1],
							(size_t)ctx->ret);
			else
				/* clear the tag markings */
				tagmap_clrn(args[SYSCALL_ARG1],
							(size_t)ctx->ret);
			break;
		case SYS_RECVFROM:
			/* not successful; optimized branch */
			if (unlikely((long)ctx->ret <= 0))
				return;
	
			/* taint-source */	
			if (fdset.find((int)args[SYSCALL_ARG0]) != fdset.end())
				/* set the tag markings */
				tagmap_setn(args[SYSCALL_ARG1],
						(size_t)ctx->ret);
			else
				/* clear the tag markings */
				tagmap_clrn(args[SYSCALL_ARG1],
						(size_t)ctx->ret);

			/* sockaddr argument is specified */
			if ((void *)args[SYSCALL_ARG4] != NULL) {
				/* clear the tag bits */
				tagmap_clrn(args[SYSCALL_ARG4],
					*((int *)args[SYSCALL_ARG5]));
				
				/* clear the tag bits */
				tagmap_clrn(args[SYSCALL_ARG5], sizeof(int));
			}
			break;
		case SYS_GETSOCKOPT:
			/* not successful; optimized branch */
			if (unlikely((long)ctx->ret < 0))
				return;
	
			/* clear the tag bits */
			tagmap_clrn(args[SYSCALL_ARG3],
					*((int *)args[SYSCALL_ARG4]));
			
			/* clear the tag bits */
			tagmap_clrn(args[SYSCALL_ARG4], sizeof(int));
			break;
		case SYS_RECVMSG:
			/* not successful; optimized branch */
			if (unlikely((long)ctx->ret <= 0))
				return;
			
			/* get the descriptor */
			it = fdset.find((int)ctx->arg[SYSCALL_ARG0]);

			/* extract the message header */
			msg = (struct msghdr *)args[SYSCALL_ARG1];

			/* source address specified */
			if (msg->msg_name != NULL) {
				/* clear the tag bits */
				tagmap_clrn((size_t)msg->msg_name,
					msg->msg_namelen);
				
				/* clear the tag bits */
				tagmap_clrn((size_t)&msg->msg_namelen,
						sizeof(int));
			}
			
			/* ancillary data specified */
			if (msg->msg_control != NULL) {
				/* taint-source */
				if (it != fdset.end())
					/* set the tag markings */
					tagmap_setn((size_t)msg->msg_control,
						msg->msg_controllen);
					
				else
					/* clear the tag markings */
					tagmap_clrn((size_t)msg->msg_control,
						msg->msg_controllen);
					
				/* clear the tag bits */
				tagmap_clrn((size_t)&msg->msg_controllen,
						sizeof(int));
			}
			
			/* flags; clear the tag bits */
			tagmap_clrn((size_t)&msg->msg_flags, sizeof(int));	
			
			/* total bytes received */	
			tot = (size_t)ctx->ret;

			/* iterate the iovec structures */
			for (i = 0; i < msg->msg_iovlen && tot > 0; i++) {
				/* get the next I/O vector */
				iov = &msg->msg_iov[i];

				/* get the length of the iovec */
				iov_tot = (tot > (size_t)iov->iov_len) ?
						(size_t)iov->iov_len : tot;
				
				/* taint-source */	
				if (it != fdset.end())
					/* set the tag markings */
					tagmap_setn((size_t)iov->iov_base,
								iov_tot);
				else
					/* clear the tag markings */
					tagmap_clrn((size_t)iov->iov_base,
								iov_tot);
		
				/* housekeeping */
				tot -= iov_tot;
			}
			break;
#if LINUX_KERNEL >= 2633
		case SYS_RECVMMSG:
#endif
		default:
			/* nothing to do */
			return;
	}
}
#endif // TARGET_IA32

/*
 * auxiliary (helper) function
 *
 * duplicated descriptors are added into
 * the monitored set
 */
static void
post_dup_hook(syscall_ctx_t *ctx)
{
	/* not successful; optimized branch */
	if (unlikely((long)ctx->ret < 0))
		return;
	
	/*
	 * if the old descriptor argument is
	 * interesting, the returned handle is
	 * also interesting
	 */
	if (likely(fdset.find((int)ctx->arg[SYSCALL_ARG0]) != fdset.end()))
		fdset.insert((int)ctx->ret);
}

/*
 * auxiliary (helper) function
 *
 * whenever close(2) is invoked, check
 * the descriptor and remove if it was
 * inside the monitored set of descriptors
 */
static void
post_close_hook(syscall_ctx_t *ctx)
{
	/* not successful; optimized branch */
	if (unlikely((long)ctx->ret < 0))
		return;
	
	/*
	 * if the descriptor (argument) is
	 * interesting, remove it from the
	 * monitored set
	 */
	fdset.erase((int)ctx->arg[SYSCALL_ARG0]);
}

static bool
fslist_contains_file(int fd)
{
	int l;
	char pathname[80], realfn[PATH_MAX];
	set<string>::iterator res;

	snprintf(pathname, sizeof(pathname), "/proc/self/fd/%d", fd);

	/* Get the original filename. It can fail if fd is a socket or pipe */
	if ((l = readlink(pathname, realfn, sizeof(realfn))) < 0)
		return false;
	realfn[l] = '\0';

	res = tainted_files.find(realfn);
	if (res != tainted_files.end())
		return true;
	DBGLOG(string("--> ") + realfn + ") ");

	return false;
}

/*
 * auxiliary (helper) function
 *
 * whenever open(2)/creat(2) is invoked,
 * add the descriptor inside the monitored
 * set of descriptors
 *
 * NOTE: it does not track dynamic shared
 * libraries
 */
static void
post_open_hook(syscall_ctx_t *ctx)
{
	bool track_fd = false;

	/* not successful; optimized branch */
	if (unlikely((long)ctx->ret < 0))
		return;

	DBGLOG(string("open(") + (char *)ctx->arg[SYSCALL_ARG0] + ") ");

	if (use_tainted_filelist) {
		track_fd = fslist_contains_file((int)ctx->ret);
	} else if (strstr((char *)ctx->arg[SYSCALL_ARG0], DLIB_SUFF) == NULL &&
			strstr((char *)ctx->arg[SYSCALL_ARG0], 
				DLIB_SUFF_ALT) == NULL)
		track_fd = true;
	/* else ignore dynamic shared libraries */

	if (track_fd) {
		DBGLOG("TAINTED FILE\n");
		fdset.insert((int)ctx->ret);
	} else {
		DBGLOG("CLEAR FILE\n");
		fdset.erase((int)ctx->ret);
	}
}

/*
 * write(2) handler (taint-sink)
 */
static void
pre_write_hook(syscall_ctx_t *ctx)
{
	size_t len;
	ADDRINT buf_addr;

	buf_addr = ctx->arg[SYSCALL_ARG1];
	len = ctx->arg[SYSCALL_ARG2];

	/* Sanity check, make sure we wont overflow the tagmap */
	if ((~0UL - buf_addr) < len)
		return;

	/* check tagmap */
	if (tagmap_issetn(buf_addr, len) != 0)
		callbacks.written(ctx);
}

#ifdef TARGET_IA32E

/* recvfrom(2) handler (taint source) */
static void
post_recvfrom_hook(syscall_ctx_t *ctx)
{
        /* not successful; optimized branch */
        if (unlikely((long)ctx->ret <= 0))
                return;

        /* taint-source */	
        if (fdset.find((int)ctx->arg[SYSCALL_ARG0]) != fdset.end())
                /* set the tag markings */
                tagmap_setn(ctx->arg[SYSCALL_ARG1],
                                (size_t)ctx->ret);
        else
                /* clear the tag markings */
                tagmap_clrn(ctx->arg[SYSCALL_ARG1],
                                (size_t)ctx->ret);

        /* sockaddr argument is specified */
        if ((void *)ctx->arg[SYSCALL_ARG4] != NULL) {
                /* clear the tag bits */
                tagmap_clrn(ctx->arg[SYSCALL_ARG4],
                                *((int *)ctx->arg[SYSCALL_ARG5]));

                /* clear the tag bits */
                tagmap_clrn(ctx->arg[SYSCALL_ARG5], sizeof(int));
        }
}

/* recvmsg(2) handler (taint source) */
static void
post_recvmsg_hook(syscall_ctx_t *ctx)
{
	/* message header; recvmsg(2) */
	struct msghdr *msg;

	/* iov bytes copied; recvmsg(2) */
	size_t iov_tot;

	/* iterators */
	size_t i;
	struct iovec *iov;
	set<int>::iterator it;
	
	/* total bytes received */
	size_t tot;

        /* not successful; optimized branch */
        if (unlikely((long)ctx->ret <= 0))
                return;

        /* get the descriptor */
        it = fdset.find((int)ctx->arg[SYSCALL_ARG0]);

        /* extract the message header */
        msg = (struct msghdr *)ctx->arg[SYSCALL_ARG1];

        /* source address specified */
        if (msg->msg_name != NULL) {
                /* clear the tag bits */
                tagmap_clrn((size_t)msg->msg_name,
                                msg->msg_namelen);

                /* clear the tag bits */
                tagmap_clrn((size_t)&msg->msg_namelen,
                                sizeof(int));
        }

        /* ancillary data specified */
        if (msg->msg_control != NULL) {
                /* taint-source */
                if (it != fdset.end())
                        /* set the tag markings */
                        tagmap_setn((size_t)msg->msg_control,
                                        msg->msg_controllen);

                else
                        /* clear the tag markings */
                        tagmap_clrn((size_t)msg->msg_control,
                                        msg->msg_controllen);

                /* clear the tag bits */
                tagmap_clrn((size_t)&msg->msg_controllen,
                                sizeof(int));
        }

        /* flags; clear the tag bits */
        tagmap_clrn((size_t)&msg->msg_flags, sizeof(int));	

        /* total bytes received */	
        tot = (size_t)ctx->ret;

        /* iterate the iovec structures */
        for (i = 0; i < msg->msg_iovlen && tot > 0; i++) {
                /* get the next I/O vector */
                iov = &msg->msg_iov[i];

                /* get the length of the iovec */
                iov_tot = (tot > (size_t)iov->iov_len) ?
                        (size_t)iov->iov_len : tot;

                /* taint-source */	
                if (it != fdset.end())
                        /* set the tag markings */
                        tagmap_setn((size_t)iov->iov_base,
                                        iov_tot);
                else
                        /* clear the tag markings */
                        tagmap_clrn((size_t)iov->iov_base,
                                        iov_tot);

                /* housekeeping */
                tot -= iov_tot;
        }
}

/* recv(2) handler (taint source) */
static void
post_recv_hook(syscall_ctx_t *ctx)
{
        /* not successful; optimized branch */
        if (unlikely((long)ctx->ret <= 0))
                return;

        /* taint-source */	
        if (fdset.find((int)ctx->arg[SYSCALL_ARG0]) != fdset.end())
                /* set the tag markings */
                tagmap_setn(ctx->arg[SYSCALL_ARG1],
                                (size_t)ctx->ret);
        else
                /* clear the tag markings */
                tagmap_clrn(ctx->arg[SYSCALL_ARG1],
                                (size_t)ctx->ret);
}

/* accept(2) handler (taint source) */
static void
post_accept_hook(syscall_ctx_t *ctx)
{
        /* not successful; optimized branch */
        if (unlikely((long)ctx->ret < 0))
                return;
        /*
         * if the socket argument is interesting,
         * the returned handle of accept(2) is also
         * interesting
         */
        if (likely(fdset.find(ctx->arg[SYSCALL_ARG0]) !=
                                fdset.end()))
                /* add the descriptor to the monitored set */
                fdset.insert((int)ctx->ret);
}

/* socket(2) handler (taint source) */
static void
post_socket_hook(syscall_ctx_t *ctx)
{
        /* not successful; optimized branch */
        if (unlikely((long)ctx->ret < 0))
                return;

        /*
         * PF_INET and PF_INET6 descriptors are
         * considered interesting
         */
        if (likely(ctx->arg[SYSCALL_ARG0] == PF_INET ||
                                ctx->arg[SYSCALL_ARG0] == PF_INET6))
                /* add the descriptor to the monitored set */
                fdset.insert((int)ctx->ret);
}
#endif // TARGET_IA32E

/**
 * External definition of global syscalls description table.
 */
extern syscall_desc_t syscall_desc[];

/**
 * Parse a line of the tainted files configuration file.
 * NOTE: It modifies the string pointed to by line
 */
static bool fslist_parse_line(char *line, size_t len)
{
	char *cur, *type, *path;
	pair<set<string>::iterator, bool> res;

	/* Ignore whitespace */
	cur = line;
	skip_whitespace(cur);

	/* Stop processing if line is a comment or empty */
	if (*cur == '#' || *cur == '\0')
		return true;

	/* Split line to TYPE PATH */
	type = cur;
	path = strchr(type, ' ');
	if (!path)
		path = strchr(type, '\t');
	if (!path)
		return false;
	*path++ = '\0';
	/* Ignore whitespace */
	skip_whitespace(path);

	/* check type */
	if (strcmp(type, "FILE") == 0) {
		DBGLOG(string("Tracking file ") + path + "\n");

		/* Add file to set of tracked files */
		res = tainted_files.insert(path);
	} else {
		ERRLOG(string("Parse error in '") + line + "'\n");
		return false;
	}

	return true;
}

/**
 * Load list of files that should be considered tainted from file specified by
 * string fn
 */
static bool fslist_load(const char *fn)
{
	size_t lineno;
	ifstream fin;
	bool ret = true;
	char line[PATH_MAX + PATH_TYPE_MAX];

	fin.open(fn, ios_base::in);
	if (fin.fail())
		goto ioerror;

	lineno = 0;
	while (true) {
		lineno++;
		/* read a line from the file */
		fin.getline(line, sizeof(line));
		if (fin.eof())
			break;
		else if (fin.fail()) {
			ERRLOG(string("Error while reading from ") + fn + 
					": " + strerror(errno) + '\n');
			goto ioerror;
		}

		/* try to parse it */
		if (!fslist_parse_line(line, fin.gcount())) {
			ERRLOG("Parse error in line " + decstr(lineno) + 
					" of "+ fn + '\n');
			ret = false;
			break;
		}
		memset(line, 0, fin.gcount());
	}

	fin.close();
	return ret;

ioerror:
	ERRLOG(string("Failed to read tracked files from ") + fn + "\n");
	if (fin.is_open())
		fin.close();
	return false;
}

/*
 * Initialize dta using libdft
 */
int dta_tool_init(struct alert_callbacks *cbs)
{
	/* initialize the core tagging engine */
	if (unlikely(libdft_init() != 0)) {
		ERRLOG("libdft initialization failed\n");
		/* failed */
		return -1;
	}

	memcpy(&callbacks, cbs, sizeof(struct alert_callbacks));
	
	/* 
	 * handle control transfer instructions
	 *
	 * instrument the branch instructions, accordingly,
	 * for installing taint-sinks (DFT-logic) that check
	 * for tainted targets (i.e., tainted operands or
	 * tainted branch targets) -- For brevity I omitted
	 * checking the result of each instrumentation for
	 * success or failure
	 */

	switch (alert_mode.Value()) {
	case ALERT_ALTER_FLOW:
		if (!callbacks.alter_flow) {
			ERRLOG("libdft missing alter flow callback!\n");
			return -1;
		}

		/* instrument call */
		(void)ins_set_post(&ins_desc[XED_ICLASS_CALL_NEAR],
				dta_instrument_jmp_call);
		
		/* instrument jmp */
		(void)ins_set_post(&ins_desc[XED_ICLASS_JMP],
				dta_instrument_jmp_call);

		/* instrument ret */
		(void)ins_set_post(&ins_desc[XED_ICLASS_RET_NEAR],
				dta_instrument_ret);
		break;

	case ALERT_WRITTEN:
		if (!callbacks.written) {
			ERRLOG("libdft missing write(2) callback!\n");
			return -1;
		}

		(void)syscall_set_pre(&syscall_desc[__NR_write],
				pre_write_hook);
		break;

	default:
		ERRLOG("libdft unknown alert mode given!\n");
		return -1;
	}

	/* 
	 * install taint-sources
	 *
	 * all network-related I/O calls are
	 * assumed to be taint-sources; we
	 * install the appropriate wrappers
	 * for tagging the received data
	 * accordingly -- Again, for brevity
	 * I assume that all calls to
	 * syscall_set_post() are successful
	 */
	/* read(2) */
	(void)syscall_set_post(&syscall_desc[__NR_read], post_read_hook);

	/* readv(2) */
	(void)syscall_set_post(&syscall_desc[__NR_readv], post_readv_hook);

	/* socket(2), accept(2), recv(2), recvfrom(2), recvmsg(2) */
	if (net_source.Value() != 0) {
#if defined(TARGET_IA32E)
		(void)syscall_set_post(&syscall_desc[__NR_socket],
			post_socket_hook);
		(void)syscall_set_post(&syscall_desc[__NR_accept],
			post_accept_hook);
		(void)syscall_set_post(&syscall_desc[__NR_recv],
			post_recv_hook);
		(void)syscall_set_post(&syscall_desc[__NR_recvfrom],
			post_recvfrom_hook);
		(void)syscall_set_post(&syscall_desc[__NR_recvmsg],
			post_recvmsg_hook);
#elif defined(TARGET_IA32)
		(void)syscall_set_post(&syscall_desc[__NR_socketcall],
			post_socketcall_hook);
#else
# error "Unknown architecture"
#endif
	}

	/* dup(2), dup2(2) */
	(void)syscall_set_post(&syscall_desc[__NR_dup], post_dup_hook);
	(void)syscall_set_post(&syscall_desc[__NR_dup2], post_dup_hook);

	/* close(2) */
	(void)syscall_set_post(&syscall_desc[__NR_close], post_close_hook);
	
	/* open(2), creat(2) */
	if (fs_source.Value() != 0) {
		(void)syscall_set_post(&syscall_desc[__NR_open],
				post_open_hook);
		(void)syscall_set_post(&syscall_desc[__NR_creat],
				post_open_hook);
		(void)syscall_set_post(&syscall_desc[__NR_mmap],
				post_mmap_hook);
#ifdef __NR_mmap2
		(void)syscall_set_post(&syscall_desc[__NR_mmap2],
				post_mmap_hook);
#endif

		/* write (2) */
		if (!fs_list.Value().empty()) {
			if (!fslist_load(fs_list.Value().c_str())) {
				/* Failed */
				return -1;
			}
			use_tainted_filelist = true;
		}
	}

	/* add stdin to the interesting descriptors set */
	if (sin_source.Value() != 0) {
		fdset.insert(STDIN_FILENO);
	}

	return 0;
}

