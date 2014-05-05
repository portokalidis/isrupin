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
	LOG_MESSAGE("<ActionMessageType>\n");

	LOG_MESSAGE("\t<ActionEnumType>");
	switch (msgtype) {
	case ACTIONMSG_NONE:
		LOG_MESSAGE("NONE");
		break;

	case ACTIONMSG_CONTROLLED_EXIT:
		LOG_MESSAGE("CONTROLLED_EXIT");
		break;

	case ACTIONMSG_CONTINUED_EXIT:
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
		printf("Exited normally!\n");
		return 0;
	} else if (WIFSIGNALED(status)) {
		signo = WTERMSIG(status);
		printf("Terminated by signaled!\n");
		return 0;
	} else if (WIFSTOPPED(status)) {
		signo = WSTOPSIG(status);
		printf("Stopped by signal %d!\n", signo);
	}

	if (ptrace(PTRACE_CONT, pid, signo, 0) != 0)
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
