#include <cassert>
#include <iostream>
#include <sstream>

#ifdef TARGET_LINUX
extern "C" {
#include <stdlib.h>
#include <unistd.h>
#include <syscall.h>
}
#endif

#include "pin.H"
#include "threadstate.hpp"
#include "libreassure.hpp"
//#include "syscall.hpp"
#include "writeslog.h"
#include "fork.h"
#include "log.hpp"

       
void CheckpointAlloc(struct thread_state *ts, checkp_t type)
{       
	if (type == WLOG_CHECKP)
		ts->memcheckp.wlog = WLogAlloc(0); // XXX: Reinstate hint
#ifdef TARGET_LINUX
	else if (type == FORK_CHECKP)
		ts->memcheckp.flog = FLogAlloc();
#endif
	else
		abort();
        
        ASSERT(ts->checkpoint == NULL,
			"Entering checkpoint, but state is not clear\n");
        ts->checkpoint = (CONTEXT *)calloc(1, sizeof(CONTEXT));
        ASSERT(ts->checkpoint, "Error allocating checkpoint state\n");
}

void CheckpointRollback(struct thread_state *ts, checkp_t type, CONTEXT *ctx)
{       
	if (type == WLOG_CHECKP)
		WLogRollback(ts->memcheckp.wlog);
#ifdef TARGET_LINUX
	else if (type == FORK_CHECKP)
		CheckpointForkRollback(ts->memcheckp.flog);
#endif
	else
		abort();
        
	PIN_SaveContext(ts->checkpoint, ctx);
}

void CheckpointFree(struct thread_state *ts, checkp_t type)
{
	if (type == WLOG_CHECKP) {
		WLogFree(ts->memcheckp.wlog);
		ts->memcheckp.wlog = NULL;
#ifdef TARGET_LINUX
	} else if (type == FORK_CHECKP) {
		FLogFree(ts->memcheckp.flog);
		ts->memcheckp.flog = NULL;
#endif
	} else
		abort();


        free(ts->checkpoint);
        ts->checkpoint = NULL;
}      

void ThreadstateInit(struct thread_state *ts, THREADID tid)
{
        ts->real_tid = get_thread_id();
        ts->in_syscall = 0;
        ts->blocked = 0;
        ts->checkpoint = NULL;
	ts->tid = tid;

        //SyscallStackAlloc(&ts->sstack);
}
                
void ThreadstateCleanup(struct thread_state *ts, checkp_t type)
{
        ts->real_tid = 0;
        if (ts->state == CHECKPOINTING)
                CheckpointFree(ts, type);
                
        //SyscallStackFree(&ts->sstack);
}

