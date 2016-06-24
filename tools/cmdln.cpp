#include <iostream>

extern "C" {
#include <stdlib.h>
#include <string.h>
}

static struct command_line {
	int argc;
	char **argv;
} cmdln;


/**
 * Patch the command line arguments, replacing the name of the tool with a new
 * one, and removing the first argument after -- (i.e., the execution wrapper).
 * It returns a pointer to private static array with the command line arguments.
 * Not thread safe.
 *
 * @param orig_name The original name of the tool that is going to be replaced
 * @param new_name  The new name of the tool to replace orig_name with
 * @param argvp A pointer to a strings array to return the new argument list,
 * which is stored in a private static array
 *
 * @return -1 on error. On success it returns the number of command line
 * arguments *argvp points to
 */
int cmdln_patch(const char *orig_name, const char *new_name, char ***argvp)
{
	int i;
	char *tname;

	// Search for tool option -t
	for (i = 0; i < cmdln.argc; i++)
		if (strcmp(cmdln.argv[i], "-t") == 0) {
			if (++i >= cmdln.argc)
				return -1;
			break;
		}

	// Search where in the argument is the tool name
	if ((tname = strstr(cmdln.argv[i], orig_name)) == NULL)
		return -1;

	// Check that we can modify the tool name in place
	if (strlen(new_name) > strlen(orig_name))
		return -1;

	// Replace the tool name
	strcpy(tname, new_name);

	// Search for -- separator
	for (i = 0; i < cmdln.argc; i++)
		if (strcmp(cmdln.argv[i], "--") == 0) {
			if (++i >= cmdln.argc)
				return false;
			break;
		}

	// Skip the execution wrapper
	for (; i < cmdln.argc; i++)
		cmdln.argv[i] = cmdln.argv[i + 1];
	cmdln.argc--;
	*argvp = cmdln.argv;
	return cmdln.argc;
}

/**
 * Save the command line arguments in a static private array.
 * Not thread safe.
 *
 * @param argc Number of arguments in argv
 * @param argv Array of strings containing the arguments. Last slot should be
 * NULL.
 *
 * @return false if not enough memory was available, and true on success
 */
bool cmdln_save(int argc, char **argv)
{
	int i;
	char *argv_copy;

	// Copy command line arguments
	cmdln.argc = argc;
	cmdln.argv = (char **)malloc((argc + 1) * sizeof(char **));
	if (!cmdln.argv)
		return false;

	for (i = 0; i < argc; i++) {
		argv_copy = strdup(argv[i]);
		if (!argv_copy)
			goto cleanup;

		//cout << "ARG[" << i << "]=" << argv_copy << endl;
		cmdln.argv[i] = argv_copy;
	}
	cmdln.argv[i] = NULL;
	return true;

cleanup:
	while (--i >= 0)
		free(argv[i]);
	return false;
}
