#include <cassert>
#include <iostream>
#include <sstream>

extern "C" {
#include <sys/syscall.h>
#include <sys/mman.h>
#include <string.h>
}

#include "pin.H"
#include "threadstate.hpp"
#include "syscall.hpp"
#include "libreassure.hpp"
#include "log.hpp"

static VOID AddSyscallInstance(struct thread_state *ts, ADDRINT retval)
{
	wlog_entry_t *ent;
	Syscall *syscall;

	if (ts->wlog_idx >= ts->wlog_end) {
		WLogExtend(ts);
	}

	syscall = new Syscall(ts->in_syscall, retval, ts->sysargs);
	assert(syscall);

	ent = ts->wlog_idx;
	ent->len = SYSCALL_ENTRY; // Use this magic value to indicate a syscall
	ent->data.ptr = syscall;
	ts->wlog_idx++;
}

BOOL HandleCheckpointSysEnter(THREADID tid, struct thread_state *ts, 
		CONTEXT *ctx, SYSCALL_STANDARD std)
{
	switch (ts->in_syscall) {
	case SYS_mmap2:
		ts->sysargs[1] = PIN_GetSyscallArgument(ctx, std, 1);
		break;
#if 0
	case SYS_munmap:
		ts->sysargs[0] = PIN_GetSyscallArgument(ctx, std, 0);
		ts->sysargs[1] = PIN_GetSyscallArgument(ctx, std, 1);
		// Perform getpid() instead of munmap(). This will effectively
		// delay the un-mapping until the checkpoint commits
		break;
#endif
	default:
		return FALSE;
	}
	return TRUE;
}

VOID HandleCheckpointSysExit(THREADID tid, struct thread_state *ts, 
		CONTEXT *ctx, ADDRINT retval)
{
	switch (ts->in_syscall) {
	case SYS_mmap2:
		if ((void *)retval == MAP_FAILED)
			return;
		AddSyscallInstance(ts, retval);
		break;
	}
}

VOID SyscallRollback(wlog_entry_t *entry)
{
	Syscall *syscall;
	stringstream ss;

	syscall = (Syscall *)entry->data.ptr;
	ss << "Rollback " << syscall << endl;
	OUTLOG(ss);

	switch (syscall->number()) {
	case SYS_mmap2:
		munmap((void *)syscall->returnValue(), syscall->argument(1));
		break;
	}
	delete syscall;
}

Syscall::Syscall(ADDRINT sysnr, ADDRINT sysret, const ADDRINT args[SYSARGS])
{
	this->sysnr = sysnr;
	this->sysret = sysret;
	memcpy(this->args, args, sizeof(args));
}

ostream & operator<<(ostream &out, Syscall *call)
{
	out << "syscall " << call->sysnr;
	return out;
}
