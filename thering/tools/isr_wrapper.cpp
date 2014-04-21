#include <iostream>
#include <sstream>
#include "pin.H"

extern "C" {
#include <stdlib.h>
#include <string.h>
}

#include "thering_opts.hpp"
#ifdef USE_ISR
# include "isr_opts.hpp"
#endif
#ifdef USE_REASSURE
# include "reassure_opts.hpp"
#endif
#ifdef USE_DTA
# include "dta_tool_opts.hpp"
#endif

#include "cmdln.hpp"
#include "log.hpp"


static BOOL child_starts(CHILD_PROCESS child, VOID *v)
{
	int argc;
	char **argv;

	argc = cmdln_patch(ISR_WRAPPER_NAME, ISR_NAME, &argv);
	DBGLOG("wrapper detected " + decstr(argc) + 
			" command line arguments\n");
	if (argc < 0) {
		ERRLOG("Failed to patch command line arguments\n");
		PIN_ExitApplication(EXIT_SUCCESS);
	}

#ifdef DEBUG
	DBGLOG("corrected command line\n");
	for (int i = 0; i < argc; i++) 
		DBGLOG("Child arg[" + decstr(i) + "] = " + argv[i] +'\n');
#endif

	CHILD_PROCESS_SetPinCommandLine(child, argc, argv);
	return TRUE;
}

static VOID usage(void)
{
	cerr << "This is the ISR wrapper PIN tool, to be called with "
		"the execution wrapper. Executed children will be run with the "
		"proper PIN tool and use randomized libraries." << endl;
	cerr << KNOB_BASE::StringKnobSummary() << endl;

}


int main(int argc, char *argv[])
{
	// Initialize pin
	if (PIN_Init(argc, argv)) {
		usage();
		return -1;
	}

	if (!cmdln_save(argc, argv)) {
		cerr << "Not enough memory" << endl;
		return -1;
	}

	// Capture the wrapper's exec()
	PIN_AddFollowChildProcessFunction(child_starts, 0);

	// Start the program, never returns
	PIN_StartProgram();

	return 0;
}
