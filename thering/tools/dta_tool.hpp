#ifndef DTA_TOOL_HPP
#define DTA_TOOL_HPP

#include "libdft_api.h"

/**
 * Structure with alert callback functions.
 */
struct alert_callbacks {
	/** 
	 * Function called when tainted data attempt to alter control flow.
	 * @param tid		Pin thread id 
	 * @param ctx		Pin CPU context
	 * @param bt		Address of the branch target
	 */
	void (*alter_flow)(THREADID tid, const CONTEXT *ctx, ADDRINT bt);
	/** 
	 * Function called when tainted data are written to a descriptor
	 * @param sctx		System call context
	 */
	void (*written)(const syscall_ctx_t *sctx);
};


int dta_tool_init(struct alert_callbacks *callbacks);

#endif
