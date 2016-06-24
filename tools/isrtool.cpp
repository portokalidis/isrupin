#include <string>
#include <iostream>
#include <sstream>
#include <cassert>
#include <pin.H>

extern "C" {
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
}


#include "log.hpp"

// ISR
#include "libisr.hpp"
#include "isr_opts.hpp"


#define SIGNAL_DEBUG

using namespace std;


/**
 * Handle a signal that would cause the application to terminate.
 *
 * @param tid Pin thread id
 * @param sig Signal number 
 * @param ctx CPU state
 * @param has_handler True if the application has a handler installed
 * @param info Additional information 
 * @param v Opaque value passed by the call back
 *
 * @return FALSE if the signal was successfully handled, and should be ignored
 * by the application, TRUE otherwise
 */
static BOOL signal_handler(THREADID tid, INT32 sig, CONTEXT *ctx, 
		BOOL has_handler, const EXCEPTION_INFO *einfo, VOID *v)
{
	string exceptname;
	bool code_injection = false;

	if (einfo) {
		ADDRINT feip;

		feip = PIN_GetContextReg(ctx, REG_INST_PTR);
		exceptname = PIN_ExceptionToString(einfo);
		OUTLOG(string("PIN thread [") + decstr(tid) + "] fault at "
				+ hexstr(feip) + "\n" + exceptname + "\n");

		code_injection = libisr_code_injection(sig, ctx, einfo);
		if (code_injection) {
			OUTLOG("!!!Code-injection detected!!!\n");
			if (isr_cidebug.Value()) {
				DBGLOG(" Pausing execution!\n");
				pause();
			}
			PIN_ExitApplication(EXIT_FAILURE);
		}
	}

        return TRUE;
}

/**
 * Handle a Pin internal fault.
 *
 * @param tid	Pin thread id
 * @param info	Additional information 
 * @param pctx	Physical CPU state
 *
 * @return EHR_HANDLED if the fault was successfully handled, or EHR_UNHANDLED
 * otherwise
 */
EXCEPT_HANDLING_RESULT internal_fault_handler(THREADID tid, 
		EXCEPTION_INFO *info, PHYSICAL_CONTEXT *pctx, VOID *v)
{
	CONTEXT ctx;

	OUTLOG(string("PIN thread [") + decstr(tid) + "] internal fault: " +
			((info)? PIN_ExceptionToString(info) : "<unknown>") +
			"\n");

	return EHR_UNHANDLED;
}


/**
 * Print help message.
 */
static void usage(void)
{
        cerr << "This is the one tool to rule them all." << endl;
        cerr << KNOB_BASE::StringKnobSummary() << endl;
}



int main(int argc, char **argv)
{
	/* initialize symbol processing */
	PIN_InitSymbols();
	
	/* initialize PIN; optimized branch */
	if (PIN_Init(argc, argv)) {
		/* PIN initialization failed */
		usage();
		return EXIT_FAILURE;
	}

	// Intercept signals that can be received due to an attack 
	PIN_UnblockSignal(SIGSEGV, TRUE);
	PIN_InterceptSignal(SIGSEGV, signal_handler, 0);
	PIN_UnblockSignal(SIGILL, TRUE);
	PIN_InterceptSignal(SIGILL, signal_handler, 0);
	PIN_UnblockSignal(SIGABRT, TRUE);
	PIN_InterceptSignal(SIGABRT, signal_handler, 0);
	PIN_UnblockSignal(SIGFPE, TRUE);
	PIN_InterceptSignal(SIGFPE, signal_handler, 0);
	PIN_UnblockSignal(SIGBUS, TRUE);
	PIN_InterceptSignal(SIGBUS, signal_handler, 0);
	PIN_UnblockSignal(SIGSYS, TRUE);
	PIN_InterceptSignal(SIGSYS, signal_handler, 0);
	PIN_UnblockSignal(SIGTRAP, TRUE);
	PIN_InterceptSignal(SIGTRAP, signal_handler, 0);
	PIN_AddInternalExceptionHandler(internal_fault_handler, 0);
	
	/* Enable ISR */
	// Initialize ISR library
	if (libisr_init(isr_keydb.Value().c_str()) != 0)
		/* failed */
		return EXIT_FAILURE;

	/* start Pin */
	PIN_StartProgram();
	/* typically not reached; make the compiler happy */
	return EXIT_SUCCESS;
}
