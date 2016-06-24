#include <iostream>
#include <vector>

#include "pin.H"

extern "C" {
#include <syscall.h>
#include <string.h>
#include <linux/net.h>
}

typedef struct syscall_data_struct {
	ADDRINT sysnr;
	ADDRINT arg[6];
} syscall_data_t;

#define VECTOR_BLOCK_SIZE 16

static vector<syscall_data_t> syscall_tracker;
static bool app_fds[65536];


static VOID Fini(INT32 code, VOID *v)
{
	cerr << "Process exiting" << endl; 
}

static VOID SysEntry(THREADID tid, CONTEXT *ctx, SYSCALL_STANDARD std, VOID *v)
{
	int fd;
	syscall_data_t *sysdata;
	ADDRINT *args;

	sysdata = &(syscall_tracker[tid]);
	sysdata->sysnr = PIN_GetSyscallNumber(ctx, std);

	// XXX: fcntl and others?
	switch (sysdata->sysnr) {
	case SYS_open:
		sysdata->arg[0] = PIN_GetSyscallArgument(ctx, std, 0);
		break;

	case SYS_close:
		fd = sysdata->arg[0] = PIN_GetSyscallArgument(ctx, std, 0);
do_close:
#if 0
		//cerr << '[' << PIN_GetPid() << "][" << tid << "] close(" << fd << ')' << endl;
		if (!app_fds[fd]) {
			cerr << "Process does not seem to own this descriptor" << endl;
			PIN_SetSyscallNumber(ctx, std, SYS_sync);
		}
#endif
		PIN_SetSyscallNumber(ctx, std, SYS_sync);
		break;

	case SYS_socketcall:
		sysdata->arg[0] = PIN_GetSyscallArgument(ctx, std, 0);
		sysdata->arg[1] = PIN_GetSyscallArgument(ctx, std, 1);
		args = (ADDRINT *)sysdata->arg[1];

		switch (sysdata->arg[0]) {
		case SYS_SHUTDOWN:
			fd = args[0];
			goto do_close;
		}
		break;

	case SYS_pipe:
	case SYS_pipe2:
		sysdata->arg[0] = PIN_GetSyscallArgument(ctx, std, 0);
		break;

	case SYS_setresuid32:
	case SYS_setresgid32:
	case SYS_setsid:
	case SYS_setgroups32:
	case SYS_setregid32:
	case SYS_setuid32:
	case SYS_chroot:
		cerr << "Disable syscall" << endl;
		PIN_SetSyscallNumber(ctx, std, SYS_sync);
		break;
	}
}

static VOID SetFd(int fd)
{
	//cerr << "Opening " << fd << endl;
	app_fds[fd] = true;
}

static VOID ClearFd(int fd)
{
	//cerr << "Closing " << fd << endl;
	app_fds[fd] = false;
}

static VOID SysExit(THREADID tid, CONTEXT *ctx, SYSCALL_STANDARD std, VOID *v)
{
	int ret, *sv, fd;
	syscall_data_t *sysdata;
	ADDRINT *args;

	sysdata = &(syscall_tracker[tid]);
	ret = (int)PIN_GetSyscallReturn(ctx, std);

	switch (sysdata->sysnr) {
	case SYS_close:
		fd = sysdata->arg[0];
do_close:
		if (ret == 0)
			ClearFd(fd);
		break;

	case SYS_open:
		//cerr << ret << " = open(" << (char *)sysdata->arg[0] << ')' << endl;
		if (ret >= 0)
			SetFd(ret);
		break;

	case SYS_creat:
	case SYS_dup:
	case SYS_dup2:
	case SYS_dup3:
		if (ret >= 0)
			SetFd(ret);
		break;

	case SYS_socketcall:
		args = (ADDRINT *)sysdata->arg[1];
		switch (sysdata->arg[0]) {
		case SYS_SOCKET:
		case SYS_ACCEPT:
			if (ret >= 0)
				SetFd(ret);
			break;

		case SYS_SHUTDOWN:
			fd = args[0];
			goto do_close;

		case SYS_SOCKETPAIR:
			if (ret == 0) {
				sv = (int *)args[3];
				SetFd(sv[0]);
				SetFd(sv[1]);
			}
			break;
		} // SYS_SOCKETCALL
		break;

	case SYS_pipe:
	case SYS_pipe2:
		if (ret == 0) {
			sv = (int *)sysdata->arg[0];
			SetFd(sv[0]);
			SetFd(sv[1]);
		}
		break;
	} // syscall

}

static VOID Fork(THREADID tid, const CONTEXT *ctx, VOID *v)
{
	cerr << '[' << PIN_GetPid() << "][" << tid << "] fork!" << endl;
	syscall_tracker.clear();
	syscall_tracker.reserve(tid + VECTOR_BLOCK_SIZE);
}

static VOID ThreadStart(THREADID tid, CONTEXT *ctx, INT32 flags, VOID *v)
{
	cerr << '[' << PIN_GetPid() << "][" << tid << 
		"] thread starting!" << endl;
	if (syscall_tracker.capacity() <= tid) {
		syscall_tracker.reserve(tid + VECTOR_BLOCK_SIZE);
	}
}

static BOOL ChildStarts(CHILD_PROCESS child, VOID *v)
{
	INT argc, i;
	const CHAR **argv;

	cerr << "Pin child starts " << CHILD_PROCESS_GetId(child) << endl;
	CHILD_PROCESS_GetCommandLine(child, &argc,
			(const char *const **)&argv);

	for (i = 0; i < argc; i++) {
		cerr << "\targ[" << i << "] = " << argv[i] << endl;
	}

	return FALSE;
}


int main(int argc, char *argv[])
{
	// Initialize pin
	PIN_Init(argc, argv);

	cerr << "Pin starting [" << PIN_GetPid() << ']' << endl;
	memset(app_fds, 0, sizeof(bool) * 65536);
	app_fds[0] = true;
	app_fds[1] = true;
	app_fds[2] = true;

	PIN_AddFiniFunction(Fini, 0);
	//PIN_AddSyscallEntryFunction(SysEntry, NULL);
	//PIN_AddSyscallExitFunction(SysExit, NULL);
	PIN_AddForkFunction(FPOINT_AFTER_IN_CHILD, Fork, NULL);
	PIN_AddThreadStartFunction(ThreadStart, NULL);
	PIN_AddFollowChildProcessFunction(ChildStarts, NULL);
	
	// Start the program, never returns
	PIN_StartProgram();

	return 0;
}
