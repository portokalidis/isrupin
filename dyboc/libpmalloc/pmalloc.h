/******************************************************************************
 * pmalloc.h
 *
 * Author:
 * Stelios Sidiroglou-Douskos <stelios@cs.columbia.edu>
 * Department of Computer Science - Columbia University
 *
 * Copyright (c) 2005 by Columbia University, all rights reserved.
 * ***************************************************************************/


#ifndef _PMALLOC_H_
#define _PMALLOC_H_

/*
 * Datastructure for holding information for every buffer allocated
 * with pmalloc. When an attack occurs, one call look up which 
 * function caused the error by examining the information 
 * present in the siginfo headers.
 */

typedef struct _function_object {
	char	filename[256];
	char	func_name[256];
	char 	buffer_name[256];
	int	address;
}func_object_t;

/*
 * Datastructure for holding information for the stack trace.
 */
typedef struct _stack_object {
	char	filename[256];
	char	func_name[256];
}stack_object_t;

void *pmalloc_2(size_t len, const char *filename, 
			const char *function_name, int line_num, 
			const char *buffer_name);
void *pmalloc(size_t size);
void *pcalloc(size_t nmemb, size_t size);

void pfree_2(void *ptr);
void pfree(void *ptr);

void *prealloc(void *ptr, size_t size);

#if 0
int a2f_find_address(int address, char *buf);
void snake_push_function(char *filename, char *function_name);
void snake_pop_function();
void snake_print_stack();
void snake_find_caller(char *buf);
#endif
//JIKK
//void dyboc_sig_handler(int sig);

#define PMALLOC_MAGIC_NUMBER	0xdeadbeef

#endif /* _PMALLOC_H_ */
