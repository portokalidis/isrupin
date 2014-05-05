#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>

#include "child.h"

//! getopt command-line options
#define CMDLINE_OPTIONS "ho:"

//! Filename to store logging information
static char *logfile_name = NULL;

//! Target program command-line index in argv
static int target_cmdline_idx = -1;


#ifdef MONITOR_DEBUG
/**
 * Print command line that will be executed.
 *
 * @param argc	Number of arguments
 * @param argv	String array of arguments
 */
static void dbg_print_cmdline(int argc, char **argv)
{
	int i;
	for (i = target_cmdline_idx; i < argc; i++)
		printf("cmdline: %s\n", argv[i]);
}
#else
#define dbg_print_cmdline(c, v) do { } while (0)
#endif


/**
 * Execute target program.
 *
 * @param cmdline_idx	Index of target command line in argv 
 * @param argv 		String array of arguments
 *
 * @return 0 on success, or -1 on error
 */
static int execute_program(int cmdline_idx, char **argv)
{
	pid_t pid;

	pid = fork();
	if (pid == 0) {
		child_execute(cmdline_idx, argv);
		return -1;
	} else if (pid > 0) {
		return child_monitor();
	} else
		return -1;
}

/**
 * Print program usage.
 *
 * @param argc	Number of arguments
 * @param argv	String array of arguments
 */
static void print_usage(int argc, char **argv)
{
	printf("Usage: %s [options] -- <program name> [program arguments] ...\n", 
			argv[0]);
	printf("\t-h              Print this message\n");
	printf("\t-o <filename>   Write log messages to <filename>\n");
}

/**
 * Parse command line arguments.
 *
 * @param argc	Number of arguments
 * @param argv	String array of arguments
 *
 * @return 0 on success, or -1 on error
 */
static int parse_arguments(int argc, char **argv)
{
	int iarg;

	// Loop over arguments
	while ((iarg = getopt(argc, argv, CMDLINE_OPTIONS)) != -1) {
		switch (iarg) {
		case 'h':
			print_usage(argc, argv);
			exit(EXIT_SUCCESS);
		case 'o':
			logfile_name = optarg;
			break;
		default:
			print_usage(argc, argv);
			exit(EXIT_FAILURE);
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "%s: missing program name\n", argv[0]);
		print_usage(argc, argv);
		return -1;
	}

	target_cmdline_idx = optind;

	return 0;
}

int main(int argc, char **argv)
{
	if (parse_arguments(argc, argv) != 0)
		return EXIT_FAILURE;

	dbg_print_cmdline(argc, argv);

	if (execute_program(target_cmdline_idx, argv) != 0)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
