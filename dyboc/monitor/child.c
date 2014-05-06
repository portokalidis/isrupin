#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>

#include "monitor.h"
#include "child.h"
#include "pmalloc.h"

//! Message header
#define MSG_HEADER "<ActionMessageType>\n"
//! Message footer
#define MSG_FOOTER  "</ActionMessageType>\n"


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


//! Last exit code by monitored processes
static int last_exit_status = EXIT_SUCCESS;


/*
 * Return code
 */
static inline void minestrone_write_return_code(int code)
{
	LOG_MESSAGE("<return_code>%d</return_code>\n", code);
}

/*
 Possible status codes:
  <xs:enumeration value="SUCCESS"/>
  <xs:enumeration value="SKIP"/>
  <xs:enumeration value="TIMEOUT"/>
  <xs:enumeration value="OTHER"/>
*/
static inline void minestrone_write_status(const char *status)
{
	LOG_MESSAGE("<status>%s</status>\n", status);
}

/*
 CWE code 

 Possible behavior values:
  <xs:enumeration value="NONE"/>
  <xs:enumeration value="CONTROLLED_EXIT"/>
  <xs:enumeration value="CONTINUED_EXECUTION"/>
  <xs:enumeration value="OTHER"/>


 Possible impact values:
   <xs:enumeration value="NONE"/>
   <xs:enumeration value="UNSPECIFIED"/>
   <xs:enumeration value="READ_FILE"/>
   <xs:enumeration value="READ_APPLICATION_DATA"/>
   <xs:enumeration value="GAIN_PRIVILEGES"/>
   <xs:enumeration value="HIDE_ACTIVITIES"/>
   <xs:enumeration value="EXECUTE_UNAUTHORIZED_CODE"/>
   <xs:enumeration value="MODIFY_FILES"/>
   <xs:enumeration value="MODIFY_APPLICATION_DATA"/>
   <xs:enumeration value="BYPASS_PROTECTION_MECHANISM"/>
   <xs:enumeration value="ALTER_EXECUTION_LOGIC"/>
   <xs:enumeration value="UNEXPECTED_STATE"/>
   <xs:enumeration value="DOS_UNCONTROLLED_EXIT"/>
   <xs:enumeration value="DOS_AMPLIFICATION"/>
   <xs:enumeration value="DOS_INSTABILITY"/>
   <xs:enumeration value="DOS_BLOCKING"/>
   <xs:enumeration value="DOS_RESOURCE_CONSUMPTION"/>
*/
static void minestrone_write_action(int cwe, const char *behavior, 
		const char *impact)
{
	LOG_MESSAGE("<action>\n");
	LOG_MESSAGE("<behavior>%s</behavior>\n", behavior);
	if (cwe > 0) {
		LOG_MESSAGE("<weakness>\n");
		LOG_MESSAGE("<cwe>CWE-%d</cwe>\n", cwe);
		LOG_MESSAGE("</weakness>\n");
	}
	LOG_MESSAGE("<impact>\n");
	LOG_MESSAGE("<effect>%s</effect>\n", impact);
	LOG_MESSAGE("</impact>\n");
	LOG_MESSAGE("</action>\n");
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
	long addr = (long)info->si_addr & PAGE_MASK;
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

	minestrone_write_action(119, "CONTROLLED_EXIT", "UNEXPECTED_STATE");
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

static int first_process = 0;

static inline void set_ptrace_options(pid_t pid)
{
	long options;

	if (first_process)
		return;

	options = PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK | 
		PTRACE_O_TRACECLONE;

	if (ptrace(PTRACE_SETOPTIONS, pid, 0, options) != 0)
		perror("PTRACE_SETOPTIONS");

	first_process = 1;
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
		last_exit_status = WEXITSTATUS(status);
		return 0;
	} else if (WIFSIGNALED(status)) {
		signo = WTERMSIG(status);
		fprintf(stderr, "Terminated by signal %d\n", WTERMSIG(status));
		last_exit_status = signo;
		return 0;
	} else if (WIFSTOPPED(status)) {
		signo = WSTOPSIG(status);
		DBG_PRINT("Stopped by signal %d!\n", signo);
		set_ptrace_options(pid);
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
int child_monitor(pid_t pid)
{
	pid_t wpid;
	int wstatus, r = 0;


	while ((wpid = waitpid(-1, &wstatus, __WALL)) >= 0) {
		assert(wpid > 0);
		DBG_PRINT("EVENT: from %d\n", wpid);
		if (process_event(wpid, wstatus) != 0)
			break;
	}

	minestrone_write_return_code(last_exit_status);

	if (errno != ECHILD) {
		perror("child_monitor");
		r = -1;
		minestrone_write_status("OTHER");
	} else {
		minestrone_write_status("SUCCESS");
	}
	return r;
}
