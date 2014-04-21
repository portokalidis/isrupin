#ifndef SYSCALL_HPP
#define SYSCALL_HPP

class Syscall {
public:
	Syscall(ADDRINT sysnr, ADDRINT sysret, const ADDRINT args[SYSARGS]);
	ADDRINT number(void) { return sysnr; }
	ADDRINT returnValue(void) { return sysret; }
	ADDRINT argument(unsigned idx) { return args[idx]; }
	friend ostream & operator<<(ostream &out, Syscall *call);

protected:
	ADDRINT sysnr, sysret, args[SYSARGS];
};

VOID SyscallRollback(wlog_entry_t *entry);

BOOL HandleCheckpointSysEnter(THREADID tid, struct thread_state *ts, 
		CONTEXT *ctx, SYSCALL_STANDARD std);

VOID HandleCheckpointSysExit(THREADID tid, struct thread_state *ts, 
		CONTEXT *ctx, ADDRINT retval);

#endif
