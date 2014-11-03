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

// DYBOC
#ifdef USE_DYBOC
# include "dyboc.hpp"
#endif

#include "log.hpp"

// ISR
#ifdef USE_ISR
# include "libisr.hpp"
# include "isr_opts.hpp"
#endif

// REASSURE
#ifdef USE_REASSURE
# include "libreassure.hpp"
# include "reassure_opts.hpp"
#endif

// DTA
#ifdef USE_DTA
# include "dta_tool.hpp"
# include "tagmap.h"
// Options are included in dta_tool.cpp
#endif

// MINESTRONE
#ifdef MINESTRONE
# include "libminestrone.hpp"
#else
# define minestrone_notify(a, b) do { } while (0)
# define minestrone_log_status(a, b) do { } while (0)
#endif

#define SIGNAL_DEBUG

using namespace std;

// Define options like which tools to use
#include "thering_opts.hpp"

#ifdef USE_DTA

//! Flag used to signal that a DTA alert for flow alteration was issued
static BOOL dta_alert_issued = FALSE;

/* 
 * DTA/DFT alert
 *
 * @param ctx		libdft syscall context
 */
static void dta_data_written(const syscall_ctx_t *ctx)
{
	ADDRINT buf, len;

	OUTLOG("!!!Important data being written out!!!\n");
	buf = ctx->arg[SYSCALL_ARG1];
	len = ctx->arg[SYSCALL_ARG2];

	for (ADDRINT i = 0; i < len; i++) {
		if (tagmap_getb(buf + i) != 0) {
			*((char *)buf + i) = 'X';
		}
	}
}

/**
 * Print some information about the flow alteration attack.
 *
 * @param insaddr    Address of the instruction where the alert originated
 * @param targetaddr Address where program flow is redirected
 */
static void print_alter_flow_info(THREADID tid, 
		const CONTEXT *ctx, ADDRINT targetaddr)
{
	ADDRINT pc;

	// Print alert
	TIDMLOG(tid, "ALERT: Control-flow alteration detected\n");

	// Print PC
	pc = PIN_GetContextReg(ctx, REG_INST_PTR);
	TIDMLOG(tid, "PC: " + hexstr(pc) + '\n');

	// Print location of PC in image
	PIN_LockClient();
	IMG img = IMG_FindByAddress(pc);
	if (IMG_Valid(img))
		OUTLOG("LOC: " + IMG_Name(img) + ':' +
				hexstr(pc + IMG_LoadOffset(img)) + '\n');
	PIN_UnlockClient();

	// Print registers
#ifdef TARGET_IA32
	TIDMLOG(tid, 
		"EAX: " + hexstr(PIN_GetContextReg(ctx, LEVEL_BASE::REG_EAX)) +
	       " ECX: " + hexstr(PIN_GetContextReg(ctx, LEVEL_BASE::REG_ECX)) +
	       " EDX: " + hexstr(PIN_GetContextReg(ctx, LEVEL_BASE::REG_EDX)) +
	       " EBX: " + hexstr(PIN_GetContextReg(ctx, LEVEL_BASE::REG_EBX)) +
	       " ESP: " + hexstr(PIN_GetContextReg(ctx, LEVEL_BASE::REG_ESP)) +
	       " EBP: " + hexstr(PIN_GetContextReg(ctx, LEVEL_BASE::REG_EBP)) +
	       " ESI: " + hexstr(PIN_GetContextReg(ctx, LEVEL_BASE::REG_ESI)) +
	       " EDI: " + hexstr(PIN_GetContextReg(ctx, LEVEL_BASE::REG_EDI)) + 
	       '\n');
#endif

	// Target where control is transfered
	TIDMLOG(tid, "TARGET PC: " + hexstr(targetaddr) + '\n');

	PIN_LockClient();
	IMG target_img = IMG_FindByAddress(targetaddr);
	if (IMG_Valid(target_img)) {
		ADDRINT locaddr = targetaddr + IMG_LoadOffset(target_img);
		OUTLOG("LOC: " + IMG_Name(target_img) + ':' +
				hexstr(locaddr) + '\n');
	}
	PIN_UnlockClient();
}

/* 
 * DTA/DFT alert.
 *
 * @param tid	Thread ID
 * @param ctx	CPU context
 * @param bt	address of the branch target
 */
static void dta_flow_altered(THREADID tid, const CONTEXT *ctx, ADDRINT bt)
{
	EXCEPTION_INFO exception;

	print_alter_flow_info(tid, ctx, bt);

	PIN_InitAccessFaultInfo(&exception, EXCEPTCODE_ACCESS_DENIED, 
			PIN_GetContextReg(ctx, REG_INST_PTR), bt, 
			FAULTY_ACCESS_EXECUTE);
	dta_alert_issued = TRUE;
	PIN_RaiseException(ctx, tid, &exception);
}

#endif

/**
 * Handle faults by either rescuing or exiting
 *
 * @param tid Pin thread id
 * @param ctx CPU state
 * @param internal Internal fault?
 *
 * @return TRUE if the fault was handled, exit otherwise
 */
static BOOL fault_handler(THREADID tid, CONTEXT *ctx, 
		const EXCEPTION_INFO *einfo, bool internal = false,
		bool code_injection = false, bool alter_flow = false)
{
#ifdef USE_REASSURE
	if (reassure.Value()) {
		reassure_ehandling_result_t res;

		res = libreassure_handle_fault(tid, ctx);
		if (res == RHR_RESCUED) {
# ifdef MINESTRONE
			if (reassure_exception_nullpointer(einfo)) {
				// CWE-476 NULL Pointer Dereference
				minestrone_notify(CWE_NULLPOINTER,
						"CONTINUED_EXECUTION",
						"DOS_INSTABILITY");
			} else {
				minestrone_notify(0,
						"CONTINUED_EXECUTION",
						"DOS_INSTABILITY");
			}
# endif
			if (internal)
				PIN_ExecuteAt(ctx);
			return TRUE;
		}
	}
#endif
	return FALSE;
}

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
	bool hijacking = false;

	if (einfo) {
		ADDRINT feip;

		feip = PIN_GetContextReg(ctx, REG_INST_PTR);
		exceptname = PIN_ExceptionToString(einfo);
		OUTLOG(string("PIN thread [") + decstr(tid) + "] fault at "
				+ hexstr(feip) + "\n" + exceptname + "\n");

#ifdef USE_ISR
		if (isr.Value()) {
			code_injection = libisr_code_injection(sig, ctx, einfo);
			if (code_injection) {
				OUTLOG("!!!Code-injection detected!!!\n");
				if (isr_cidebug.Value()) {
					DBGLOG(" Pausing execution!\n");
					pause();
				}
			}
		}
#endif
#ifdef USE_DTA
		if (dta.Value() && !code_injection) {
			if (dta_alert_issued) {
				OUTLOG("!!!Control-flow alteration "
						"detected!!!\n");
				dta_alert_issued = FALSE;
				hijacking = true;
			}
		}
#endif
	}

	if (fault_handler(tid, ctx, einfo, false, code_injection, hijacking))
		return FALSE; // Supress signal

#ifdef MINESTRONE
	if (hijacking || code_injection) {
		OUTLOG("!!!Exiting to safety!!!\n");
		if (hijacking) {
			// CWE-691 Insufficient Control Flow Management
			minestrone_notify(CWE_CF, "CONTROLLED_EXIT",
					"ALTER_EXECUTION_LOGIC");
		} else {
			// CWE-94 Failure to Control Generation of Code
			minestrone_notify(CWE_CI, "CONTROLLED_EXIT",
					"EXECUTE_UNAUTHORIZED_CODE");
		}
	} else {
		// Log a controlled exit message
		minestrone_notify(0, "CONTROLLED_EXIT", "DOS_INSTABILITY");
	}
	minestrone_log_status(ES_SUCCESS, EXIT_SUCCESS);         
	PIN_ExitProcess(EXIT_FAILURE);
#endif
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

#ifdef USE_REASSURE
	if (reassure.Value()) {
		reassure_ehandling_result_t res;

		// Handle intentional faults
		res = libreassure_handle_internal_fault(tid, &ctx, info);
		if (res == RHR_HANDLED) {
			OUTLOG("internal fault handled by REASSURE\n");
			return EHR_HANDLED;
		}
	}
#endif

	fault_handler(tid, &ctx, info, true);
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
	
#ifdef USE_DTA
	/* Enable DTA */
	if (dta.Value()) { 
		struct alert_callbacks dft_callbacks;

		dft_callbacks.written = dta_data_written;
		dft_callbacks.alter_flow = dta_flow_altered;
		/* initialize the core tagging engine */
		if (dta_tool_init(&dft_callbacks) != 0) 
			/* failed */
			return EXIT_FAILURE;
	}
#endif
	
#ifdef USE_ISR
	/* Enable ISR */
	if (isr.Value()) {
		// Initialize ISR library
		if (libisr_init(isr_keydb.Value().c_str()) != 0)
			/* failed */
			return EXIT_FAILURE;
	}
#endif

#ifdef USE_REASSURE
	/* Enable REASSURE */
	if (reassure.Value()) {
		if (reassure_init(config_file.Value().c_str(), 
					runtime_block.Value(),
					fork_checkpoint.Value()) != 0)
			return EXIT_FAILURE;

		PIN_UnblockSignal(SIGPIPE, TRUE);
		PIN_InterceptSignal(SIGPIPE, signal_handler, 0);
	}
#endif

#ifdef MINESTRONE
	// If a timeout has been specified, setup and start the watchdog
	if (exec_timeout.Value() > 0) {
		minestrone_watchdog_init(argc, argv, exec_timeout.Value());
		if (!minestrone_watchdog_start())
			return EXIT_FAILURE;
	}
        PIN_AddFiniFunction(minestrone_fini_success, 0);
#endif

	/* start Pin */
	PIN_StartProgram();
	/* typically not reached; make the compiler happy */
	return EXIT_SUCCESS;
}
