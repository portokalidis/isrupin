#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "monitor.h"
#include "child.h"
#include "pmalloc.h"

//! Message header
#define MSG_HEADER "<ActionMessageType>\n"
//! Message footer
#define MSG_FOOTER  "</ActionMessageType>\n"
//! Page size (4096)
#define PAGE_SIZE (4096)
//! Page mask
#define PAGE_MASK (PAGE_SIZE-1)


/**
 * Macro that logs a message in the log file and to stderr if it has been 
 * log_stderr is set
 */
#define LOG_MESSAGE(...) \
	do {\
		fprintf(log_fp, __VA_ARGS__);\
		if (log_stderr)\
			fprintf(stderr, __VA_ARGS__);\
	} while (0)

#ifdef MONITOR_DEBUG
# define DBG_PRINT(...) do { printf(__VA_ARGS__); } while (0)
#else
# define DBG_PRINT(...) do { } while (0)
#endif


//! Flag determining if an action message has already been issued
static int action_message_issued = 0;


/**
 * Return minestrone message for execution status.
 *
 * @param status	Exit status
 * @param code		Exit code
 *
 * @return string containing message
 */
static void action_message(actionmsg_type_t msgtype)
{
	if (action_message_issued)
		return;

	LOG_MESSAGE("<ActionMessageType>\n");

	LOG_MESSAGE("\t<ActionEnumType>");
	switch (msgtype) {
	case ACTIONMSG_NONE:
		LOG_MESSAGE("NONE");
		break;

	case ACTIONMSG_CONTROLLED_EXIT:
		LOG_MESSAGE("CONTROLLED_EXIT");
		break;

	case ACTIONMSG_CONTINUED_EXECUTION:
		LOG_MESSAGE("CONTINED_EXIT");
		break;
	
	case ACTIONMSG_OTHER:
		LOG_MESSAGE("OTHER");
		break;

	default:
		abort();
	}
	LOG_MESSAGE("</ActionEnumType>\n");

	LOG_MESSAGE("</ActionMessageType>\n");

	action_message_issued = 1;
}

/**
 * Analyze access error to determine if it was cause by DYBOC defenses.
 *
 * @param info Signal information structure
 */
static int analyze_access_error(pid_t pid, siginfo_t *info)
{
	long val, magic;
	// Aligning fault address to page boundary
	long addr = (long)info->si_addr & ~PAGE_MASK;
	long start, end;

#ifdef MONITOR_DEBUG
	errno = 0;
	if ((val = ptrace(PTRACE_PEEKDATA, pid, (void *)addr, 0)) == -1) {
		if (errno != 0) {
			perror("ptrace(PEEKDATA)");
			return -1;
		}
	}
	DBG_PRINT("data read from protected page %lu\n", val);
#endif

	// The magic number is 4 bytes
	magic = PMALLOC_MAGIC_NUMBER;
	if (sizeof(magic) > 4)
		magic |= (long)PMALLOC_MAGIC_NUMBER << 32;
	//DBG_PRINT("magic: %lx\n", magic);

	for (start = addr + sizeof(size_t), end = addr + PAGE_SIZE; 
			start < end; start += sizeof(size_t)) {
		errno = 0;
		val = ptrace(PTRACE_PEEKDATA, pid, (void *)start, 0);
		if (val == -1 && errno != 0) {
			perror("ptrace(PEEKDATA)");
			return -1;
		}
		//DBG_PRINT("page: %lx\n", val);
		if (val != magic)
			return 0;
	}

	LOG_MESSAGE("Fault in DYBOC guard page!\n");
	action_message(ACTIONMSG_CONTROLLED_EXIT);
	return 1;
}

/**
 * Process signal and return signal number to be actually delivered to process.
 *
 * @param pid	Process id that received signal
 * @param signo Signal number
 *
 * @return 0 on success, or -1 on error
 */
static int process_signal(pid_t pid, int signo)
{
	siginfo_t info;

	switch (signo) {
	case SIGSTOP:
	case SIGTSTP:
	case SIGTTIN:
	case SIGTTOU:
		signo = 0;
		break;

	case SIGTRAP:
		signo = 0;
		break;

	case SIGSEGV:
		/* Process below */
		break;

	default:
		return signo;
	}

	if (ptrace(PTRACE_GETSIGINFO, pid, 0, &info) != 0) {
		if (errno == EINVAL)
			DBG_PRINT("ptrace failed with EINVAL\n");
		else 
			perror("ptrace");
	}

	DBG_PRINT("signo=%d, errno=%d, code=%d\n", info.si_signo, info.si_errno,
			info.si_code);
	if (info.si_code == SEGV_ACCERR) {
		DBG_PRINT("Illegal access rights\n");
		DBG_PRINT("error address %p\n", info.si_addr);
		analyze_access_error(pid, &info);
	}

	return signo;
}

/**
 * Process an event received from the child.
 *
 * @param pid		PID of the process that generated the event
 * @param status	Event status code
 *
 * @return 0 on success, or -1 on error
 */
static int process_event(pid_t pid, int status)
{
	int signo = 0;

	if (WIFEXITED(status)) {
		DBG_PRINT("Exited normally!\n");
		return 0;
	} else if (WIFSIGNALED(status)) {
		signo = WTERMSIG(status);
		DBG_PRINT("Terminated by signal %d!\n", WTERMSIG(status));
		return 0;
	} else if (WIFSTOPPED(status)) {
		signo = WSTOPSIG(status);
		DBG_PRINT("Stopped by signal %d!\n", signo);
		signo = process_signal(pid, signo);
	}

	if (ptrace(PTRACE_CONT, pid, 0, signo) != 0)
		return -1;
	return 0;
}

/**
 * Execute the child.
 *
 * @param cmdline_idx	Index of target command line in argv 
 * @param argv 		String array of arguments
 *
 * @return 0 on success, or -1 on error
 */
int child_execute(int cmdline_idx, char **argv)
{
	if (ptrace(PTRACE_TRACEME, 0, 0, NULL) != 0) {
		perror(argv[0]);
		return -1;
	}

	execvp(argv[cmdline_idx], argv + cmdline_idx);

	fprintf(stderr, "%s: error executing target program\n", argv[0]);
	fprintf(stderr, "%s: %s\n", argv[0], strerror(errno));
	exit(EXIT_FAILURE);
	return -1;
}

/**
 * Monitor child.
 *
 * @return 0 on success, or -1 on error
 */
int child_monitor()
{
	pid_t wpid;
	int wstatus;

	while ((wpid = waitpid(-1, &wstatus, __WALL)) >= 0) {
		assert(wpid > 0);
		fprintf(stderr, "EVENT: from %d\n", wpid);
		if (process_event(wpid, wstatus) != 0)
			break;
	}

	if (errno == ECHILD) {
		action_message(ACTIONMSG_NONE);
		return 0;
	}
	perror("child_monitor");
	return -1;
}
