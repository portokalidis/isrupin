#ifndef LIBREASSURE_HPP
#define LIBREASSURE_HPP

#include "threadstate.hpp"


/**
 * Enum for returning results of reassure handling a particular exception
 */
typedef enum REASSURE_EHANDLING_RESULT { 
	//! Exception handled
	RHR_HANDLED,
	//! Exception handled and proccess rescued
	RHR_RESCUED,
	//! Exception handling error, or exception cannot be handled
	RHR_ERROR
} reassure_ehandling_result_t;


int reassure_init(const char *conf_fn, BOOL rb, BOOL usefork);

reassure_ehandling_result_t libreassure_handle_fault(THREADID tid, 
		CONTEXT *ctx);

reassure_ehandling_result_t libreassure_handle_internal_fault(THREADID tid, 
		CONTEXT *ctx, EXCEPTION_INFO *info);

#endif
