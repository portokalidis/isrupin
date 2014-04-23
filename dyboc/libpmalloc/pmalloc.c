/******************************************************************************
 * pmalloc.c
 *
 * Author:
 * Stelios Sidiroglou-Douskos <stelios@cs.columbia.edu>
 * Department of Computer Science - Columbia University
 *
 * Copyright (c) 2005 by Columbia University, all rights reserved.
 * ***************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/user.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <signal.h>
#include <errno.h>

//#include <glib.h> //For datastructures
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <stdint.h>

#include "pmalloc.h"

//#define PMALLOC_DEBUG
#define PMALLOC_WHITELIST

#ifdef PMALLOC_WHITELIST
#define WLIST_SIZE 16

static size_t *wlist = NULL;
#endif


#if 0

/*
 * Doubly linked list for keeping address to buffer translations
 */
GList *address_to_function;

/*
 * init flag for address_to_function list
 */
int init_flag = 0;

/*
 * Doubly linked list for implementing our stack
 */
GList *stack_list;

/*
 * Init flag for stack_init_flag list
 */
int stack_init_flag = 0;


/*
 * address_to_function_init()
 * 
 * Initialize the list. When using the glib datastructures, one has to 
 * initiate them to NULL before populating them.
 *
 */
void address_to_function_init()
{	
	if (init_flag == 0) {
		address_to_function = NULL;
		init_flag = 1;
	} else if (init_flag == 1) {
		return;
	} else {
		fprintf(stderr, "Undefined value for init_flag\n");
	}
}

/*
 * a2f_append_object()
 * Inserts object to linked list
 */
#if 0
static void a2f_append_object(func_object_t *object)
{
	address_to_function = g_list_append(address_to_function, object);
}

/*
 * a2f_object_init()
 * 
 * Initialize object
 */
static void a2f_object_init(func_object_t *object) 
{
	bzero(object->filename, sizeof(object->filename));
	bzero(object->func_name, sizeof(object->func_name));
	bzero(object->buffer_name, sizeof(object->buffer_name));
	object->address = 0;
}

/*
 * a2f_create_object()
 * Create and initialize object
 *
 * Returns object on success, NULL on failure
 */
static func_object_t *a2f_create_object()
{
	func_object_t *object = malloc(sizeof(func_object_t));

	if (!object) {
		fprintf(stderr, "a2f_create_object: No mem for object\n");
		return NULL;
	} else {
		a2f_object_init(object);
	}
	return object;
}
#endif

/*
 * a2f_show_objects()
 * Walks through allocated buffers and prints information relative to 
 * their creation (function name, buffer name, address returned from pmalloc)
 */
void a2f_show_objects()
{
	GList *iterator;
	func_object_t *tmp;

	fprintf(stderr, "\n *** Dumping function object list *** \n");
	for (iterator = address_to_function; iterator != NULL; 
						iterator = iterator->next) {
		tmp = (func_object_t *) iterator->data;
		fprintf(stderr, "Filename %s\n FunctionName %s\n " 
						"BufferName %s\n address 0x%x\n",
						tmp->filename,
						tmp->func_name,
						tmp->buffer_name,
						tmp->address);
	}
}

/*
 * a2f_find_address()
 * 
 * Searches throught linked list to find the information given
 * the address of the function.
 */
int a2f_find_address(int address, char *buf)
{
	GList *iterator;
	func_object_t *tmp;
	int found = 0;
	
	fprintf(stderr, "\n*** SEARCHING FOR THE BUFFER THAT CAUSED SIGSEGV ***\n");
	for (iterator = address_to_function; iterator != NULL; 
									iterator = iterator->next) {
		tmp = (func_object_t *) iterator->data;
		if (tmp->address == address) {
			fprintf(stderr, "\n*** FOUND OFFENDING ADDRESS ***\n");
			fprintf(stderr, "buffer name = %s\n", tmp->buffer_name);
			fprintf(stderr, "function name = %s\n", tmp->func_name);
			fprintf(stderr, "in file = %s\n", tmp->filename);
			fprintf(stderr, "At mem location = 0x%x\n", tmp->address);
		
			snprintf(buf, 
						(sizeof(tmp->func_name) + 
						 sizeof(tmp->filename) + 
						 sizeof(tmp->buffer_name)), 
						"%s:%s:%s", 
						tmp->buffer_name, tmp->func_name, tmp->filename);
			
			found = 1;
		}
	}
	if (!found) {
		fprintf(stderr, "\n*** COUND NOT FIND OFFENDING ADDRESS ***\n");
		a2f_show_objects();
	}
	return found;
}

/*------------------------------STACK----------------------------------------*/
/*
 * stack_init()
 * Initialize the list in the first invocation.
 */
void stack_init()
{	
	if (init_flag == 0) {
		stack_list = NULL;
		stack_init_flag = 1;
	} else if (stack_init_flag == 1) {
		return;
	} else {
		fprintf(stderr, "Undefined value for init_flag\n");
	}
}

/*
 * stack_object_init()
 * 
 * Initialize object the passed object.
 */
static void stack_object_init(stack_object_t *object) 
{
	bzero(object->filename, sizeof(object->filename));
	bzero(object->func_name, sizeof(object->func_name));
}

/*
 * stack_create_object()
 * Create and initialize object
 *
 * Returns object on success, NULL on failure
 */
static stack_object_t *stack_create_object()
{
	stack_object_t *object = malloc(sizeof(stack_object_t));

	if (!object) {
		fprintf(stderr, "a2f_create_object: No mem for object\n");
		return NULL;
	} else {
		stack_object_init(object);
	}
	return object;
}

/*
 * snake_push_function()
 * 
 * push function name
 *
 * This will be used to help create our own program trace. The
 * stack object passed is appended at the end of the list.
 */
void snake_push_function(char *filename, char *function_name)
{
	stack_object_t *object;
	object = stack_create_object();
	if (!object) {
		fprintf(stderr, "Error:snake_push_function: No mem for object\n");
		return;
	}
	
	/*
	 * Stack will initialize only the first time this is invoked
	 */
	stack_init();
		
	/*
	 * Populate datastructure
	 */
	memcpy(object->filename, filename, sizeof(object->filename));
	memcpy(object->func_name, function_name, sizeof(object->func_name));

	/* Insert object on stack */
	stack_list = g_list_append(stack_list, object);
}

/*
 * pop function name
 * This will be used to help create our own program trace
* 
 */
void snake_pop_function()
{
	GList *last;
	last = g_list_last(stack_list);
	if (!last) {
		/*
		fprintf(stderr, "snake_pop_function(): " 
				"tried to pop empty stack\n");
		
		*/		
		return;
	}
	stack_list = g_list_remove(stack_list, last->data);
}

/*
 * snake_find_caller()
 *
 * This function is responsible for returning a string that contains
 * the caller and callee of the vulnerable function. It essentially
 * prints the second and third to last elements in the call stack.
 * The last element is the signal handling code that we insert so 
 * we can safely ommit that.
 *
 */
void snake_find_caller(char *buf)
{
	stack_object_t *callee, *caller;
	
	guint length = g_list_length(stack_list);

	if (length < 3) {
		/* If the length is 2 there might be something strange here */
		
		fprintf(stderr, "snake_find_caller(): " 
						"Length should be greater than 2. It is %d\n",
						length);
		return;
	}

	callee = (stack_object_t *) g_list_nth_data(stack_list, length - 2);
	fprintf(stderr, "The callee that caused overflow is believed to be "
					"Function:%s in %s\n",
						callee->func_name,
						callee->filename);

	caller = (stack_object_t *) g_list_nth_data(stack_list, length - 3);
	fprintf(stderr, "The caller that caused the overflow is believed to be "
					"Function:%s in %s\n",
						caller->func_name,
						caller->filename);

	snprintf(buf, (sizeof(callee->func_name) + sizeof(callee->filename) +
				sizeof(callee->func_name) + sizeof(callee->filename)), 
						"%s:%s:%s:%s:", 
						callee->func_name, callee->filename, 
						caller->func_name, caller->filename);
	

}


/*
 * snake_print_stack()
 * Walks through objects and prints information
 */
void snake_print_stack()
{
	GList *iterator;
	stack_object_t *tmp;

	fprintf(stderr, "\n *** Printing stack trace *** \n");
	for (iterator = stack_list; iterator != NULL; 
									iterator = iterator->next) {
		tmp = (stack_object_t *) iterator->data;
		fprintf(stderr, "Filename %s\n FunctionName %s\n",
						tmp->filename,
						tmp->func_name);

	}

}
#endif

/*----------------------------END STACK----------------------------------------*/

/* Store the magic number, as the means to
 * differentiate dyboc related segfaults.
 */
static void mark_magic_number(void *ptr, size_t len)
{
	void *start, *end;
	uint32_t magic;

	for (start = ptr, end = ptr + len; (start + 3) < end; start += 4) {
		*(uint32_t *)start = PMALLOC_MAGIC_NUMBER;
	}
	for (magic = PMALLOC_MAGIC_NUMBER; start < end; start++, magic >>= 8) {
		*(uint8_t *)start = (uint8_t)magic;
	}

}

static void check_magic_number(void *ptr, size_t len)
{
	void *start, *end;
	uint32_t magic;

	/* Check that the magic values (canaries) are there */
	for (start = ptr, end = ptr + len; (start + 3) < end; start += 4) {
		if (*(uint32_t *)start != PMALLOC_MAGIC_NUMBER) {
#ifdef PMALLOC_DEBUG
			fprintf(stderr, "unprotect_area: magic number does "
					"not match\n");
#endif
			raise(SIGSEGV);
		}
	}
	for (magic = PMALLOC_MAGIC_NUMBER; start < end; start++, magic >>= 8) {
		if (*(uint8_t *)start != (uint8_t)magic) {
#ifdef PMALLOC_DEBUG
			fprintf(stderr, "unprotect_area: magic number "
					"does not match\n");
#endif
			raise(SIGSEGV);
		}
	}


}

/**
 * 
 * This is the new version of the pmalloc implementation. The goal here is 
 * to include additional information such as where the buffer was defined
 * so that we can use this information once an attack has been caught.
 */
void *pmalloc_2(size_t len, const char *filename, const char *function_name, 
		int line_num, const char *buffer_name)
{
	return pmalloc(len);
#if 0
	func_object_t *object;
	address_to_function_init();
	void *buf;
	/* 
	* We're going to need 2 extra pages of memory - one for any offset
	* carryover over a page boundry, and one to protect. 
	*/
	size_t pages = (len & PAGE_MASK) + 2;
	/* The offset into our region where the user's memory starts */
	size_t offset = PAGE_SIZE - (len & ~PAGE_MASK);


	if(offset < sizeof(size_t))
	{
		pages++;
		offset += PAGE_SIZE;
	}

	//printf("size is %d, page is %d , my is %d\n", pages << PAGE_SHIFT, pages, len + (2 * PAGE_SIZE));

	if((buf = mmap(NULL, len + (2 * PAGE_SIZE), PROT_READ | PROT_WRITE, 
		    MAP_PRIVATE | MAP_ANONYMOUS, 0, 0)) == (void *)-1)
	{
		perror("pmalloc/mmap");
		exit(1);
	}
	mark_magic_number(buf, PAGE_SIZE);

	/* We store the length twice, because we need it in the beginning of the
	* segment, and we can keep it in the protected region as well to double 
	* check for heap corruption (ie: the user wrote to addresses BEFORE his
	* memory)
	*/
	*(size_t *)buf = len;
	*(size_t *)(buf+offset+len) = len;

	mark_magic_number(buf + offset + len, PAGE_SIZE);
	if(mprotect(buf+offset+len, PAGE_SIZE, PROT_NONE) == -1)
	{
		perror("mymalloc/mprotect");
		exit(1);
	}

	/*
	 * Create object that will store the buffer infomration
	 */
	object = (func_object_t *) a2f_create_object();
	if (!object) {
		fprintf(stderr, "Error->pmalloc_2: couldn't create object\n");
		exit(1);
	}
	
	/*
	 * Populate datastructure
	 */
	memcpy(object->filename, filename, sizeof(object->filename));
	memcpy(object->func_name, function_name, sizeof(object->func_name));
	memcpy(object->buffer_name, buffer_name, sizeof(object->buffer_name));
	object->address = (int) buf+offset+len;
	
	a2f_append_object(object);
	//a2f_show_objects();    
	
	return buf+offset;
#endif
}

/*
 * This version of pfree will provide the same functionality as the 
 * standard version but will also make sure to clear the structures
 * associated with pmalloc_2
 */
void pfree_2(void *buf)
{
	pfree(buf);
}


/*---------------------------------------------------------------------------*/


void *malloc(size_t size)
{
	return pmalloc(size);
}

int posix_memalign(void **memptr, size_t alignment, size_t size)
{
	void *ptr;

#ifdef PMALLOC_DEBUG
	fprintf(stderr, "posix_memalign: %lu %lu\n", alignment, size);
#endif

	ptr = pmalloc(size);
	if (!ptr)
		return -1;
	*memptr = ptr;
	return 0;
}

/* 
 * pmalloc
 *
 */
void *pmalloc(size_t len)
{
	void *buf, *ptr;
	size_t real_len, offset;

#ifdef PMALLOC_WHITELIST
	if (wlist) {
		size_t *wl;
		for (wl = wlist; *wl > 0; wl++)
			if (*wl == len) {
				len++;
				//fprintf(stderr, "Increase size of allocation %d\n", len);
				break;
			}
	}
	/*
	if (len == 36865) {
		len++;
	}
	*/
#endif

#ifdef PMALLOC_DEFAULT
	/* We need to allocate 2 pages + all the pages necessary to store len
	 * bytes */
	real_len = len + 2 * PAGE_SIZE;

	/* Offset of buffer in page 1 */
	offset = PAGE_SIZE - (len & ~PAGE_MASK);
#elif defined(PMALLOC_UNDER)
	/* We need to allocate 1 page + all the pages necessary to store len
	 * bytes */
	real_len = len + PAGE_SIZE;

	/* Offset of buffer in page 1 */
	offset = 0;
#elif defined(PMALLOC_OVER)
	/* We need to allocate all the pages necessary to store len+size_t
	 * bytes + 1 page */
	real_len = len + sizeof(size_t) + PAGE_SIZE;

	/* Offset of buffer in page 1 */
	offset = PAGE_SIZE - ((len + sizeof(size_t)) & ~PAGE_MASK);
#else
# error "Unspecified pmalloc behavior"
#endif

	// Align to page size
	if (real_len & ~PAGE_MASK)
		real_len = (real_len & PAGE_MASK) + PAGE_SIZE;
	// If page was aligned, offset = 0
	if (offset == PAGE_SIZE)
		offset = 0;

#ifdef PMALLOC_DEBUG
	fprintf(stderr, "pmalloc: len=%lu, real_len=%lu\n", len, real_len);
#endif

	if((buf = mmap(NULL, real_len, PROT_READ | PROT_WRITE, 
			MAP_PRIVATE | MAP_ANONYMOUS, 0, 0)) == (void *)-1) {
		perror("pmalloc/mmap");
		exit(1);
	}

#ifdef PMALLOC_DEFAULT
	// Mark pages as magic
	// Mark first pages + unused offset in 2nd page
	mark_magic_number(buf, PAGE_SIZE + offset); 
	// Mark last page
	mark_magic_number(buf + PAGE_SIZE + len + offset, PAGE_SIZE); 

	/* We store the length twice, because we need it in the beginning of the
	 * segment, and we can keep it in the protected region as well to double
	 * check for heap corruption (ie: the user wrote to addresses BEFORE his
	 * memory)
	*/
	*(size_t *)buf = len;
	*(size_t *)(buf + PAGE_SIZE + offset + len) = len;

	// Protect first page
	if(mprotect(buf, PAGE_SIZE, PROT_NONE) == -1) {
		perror("pmalloc/mprotect");
		exit(1);
	}

	// Protect last page
	if(mprotect(buf + PAGE_SIZE + offset + len, PAGE_SIZE, 
				PROT_NONE) == -1) {
		perror("pmalloc/mprotect");
		exit(1);
	}

	ptr = buf + PAGE_SIZE + offset;
#elif defined(PMALLOC_UNDER)
	// Mark page guard page with magic number
	mark_magic_number(buf, PAGE_SIZE);

	/* Store the length of the allocated area in the guard page */
	*(size_t *)(buf) = len;

	// Protect first page
	if(mprotect(buf, PAGE_SIZE, PROT_NONE) == -1) {
		perror("pmalloc/mprotect");
		exit(1);
	}

	ptr = buf + PAGE_SIZE;
#elif defined(PMALLOC_OVER)
	// Mark guard page with magic number
	mark_magic_number(buf + len + offset + sizeof(size_t), PAGE_SIZE);

	/* We store the length twice, because we need it in the beginning of the
	* segment, and we can keep it in the protected region as well to double 
	* check for heap corruption (ie: the user wrote to addresses BEFORE his
	* memory)
	*/
	*(size_t *)(buf + offset) = len;
	*(size_t *)(buf + offset + sizeof(size_t) + len) = len;

	// Protect last page
	if(mprotect(buf + len + offset + sizeof(size_t), 
				PAGE_SIZE, PROT_NONE) == -1) {
		perror("pmalloc/mprotect");
		exit(1);
	}

	ptr = buf + offset + sizeof(size_t);
#else
# error "Unspecified pmalloc behavior"
#endif

#ifdef PMALLOC_DEBUG
	fprintf(stderr, "         start=%8p end(excl)=%8p ptr=%8p\n", buf, 
			buf + real_len, ptr);
#endif
#if 0
	if (buf == (void *)0xb60ab000) 
		asm ("int3\n\t");
#endif
	return ptr;
}

/*
 * Unprotect an area returned by malloc, and return the begin of the mapped area
 * and its real length
 */
static void *unprotect_area(void *ptr, size_t *real_len, size_t *lenp)
{
	void *start;
	size_t offset, len;

#ifdef PMALLOC_DEFAULT
	// Calculate the beginning of the area we allocated
	start = (void *)((long)ptr & PAGE_MASK) - PAGE_SIZE;

	/* Unprotect 1st guard page */
	if (mprotect(start, PAGE_SIZE, PROT_READ) == -1) {
		perror("unprotect_area/mprotect");
		raise(SIGSEGV);
	}

	// Get the length of the area
	len = *(size_t *)start;
	/* Offset of ptrfer in page 1 */
	offset = (len & ~PAGE_MASK);
	if (offset != 0) // len not aligned
		offset = PAGE_SIZE - offset;

	*real_len = 2 * PAGE_SIZE + len + offset;
	*lenp = len;

# ifdef PMALLOC_DEBUG
	fprintf(stderr, "unprotect_area: start=%p, ptr=%p, len=%lu, "
			"real_len=%lu\n", start, ptr, len, *real_len);
# endif

	/* Unprotect last guard page */
	if (mprotect(ptr + len, PAGE_SIZE, PROT_READ) == -1) {
		perror("unprotect_area/mprotect");
		raise(SIGSEGV);
	}

	/* Check to make sure lengths are consistant. If not, advise user to 
	 * try using the (not yet written) underflow version of this library */
	if(*(size_t *)(ptr + len) != len) {
		fprintf(stderr, "Error: heap corruption. Inconsistant "
				"allocation sizes.\n");
		raise(SIGSEGV);
	}

	check_magic_number(start + sizeof(size_t),
			PAGE_SIZE + offset - sizeof(size_t));

#elif defined(PMALLOC_UNDER)
	// Calculate the beginning of the area we allocated
	start = ptr - PAGE_SIZE;

	/* Unprotect guard page */
	if (mprotect(start, PAGE_SIZE, PROT_READ) == -1) {
		perror("unprotect_area/mprotect");
		raise(SIGSEGV);
	}

	// Get the length of the area
	len = *(size_t *)start;
	/* Offset of end of ptrfer */
	offset = (len & ~PAGE_MASK);
	if (offset != 0) // len not aligned
		offset = PAGE_SIZE - offset;

	*real_len = len + offset + PAGE_SIZE;
	*lenp = len;

	check_magic_number(start + sizeof(size_t), 
			PAGE_SIZE - sizeof(size_t));

# ifdef PMALLOC_DEBUG
	fprintf(stderr, "unprotect_area: start=%p, ptr=%p, len=%lu, "
			"real_len=%lu\n", start, ptr, len, *real_len);
# endif

#elif defined(PMALLOC_OVER)
	// Get the length of the allocated area
	len = *(size_t *)(ptr - sizeof(size_t));

	// Calculate the beginning of the area we allocated
	start = (void *)((long)(ptr - sizeof(size_t)) & PAGE_MASK);

	// Offset of ptr from beginning of allocated area
	offset = ptr - start;

# ifdef PMALLOC_DEBUG
	fprintf(stderr, "unprotect_area: start=%p, ptr=%p, len=%lu\n", 
			start, ptr, len);
# endif

	/* Unprotect guard page */
	if (mprotect(ptr + len, PAGE_SIZE, PROT_READ) == -1) {
		perror("unprotect_area/mprotect");
		raise(SIGSEGV);
	}

	*real_len = len + offset + PAGE_SIZE;
	*lenp = len;

	check_magic_number(start + len + offset + sizeof(size_t), 
			PAGE_SIZE - sizeof(size_t));

# ifdef PMALLOC_DEBUG
	fprintf(stderr, "start=%p, ptr=%p, len=%lu, real_len=%lu\n", 
			start, ptr, len, *real_len);
# endif

	// Check that the two locations were we stored the length of the ptrfer
	// agree
	if (len != *(size_t *)(ptr + len)) {
		fprintf(stderr, "Error: heap corruption. Inconsistant "
				"allocation sizes.\n");
		raise(SIGSEGV);
	}
#else
# error "Unspecified unprotect_area behavior"
#endif

	return start;
}

void *calloc(size_t nmemb, size_t size)
{
	return pcalloc(nmemb, size);
}

void *pcalloc(size_t nmemb, size_t size)
{
	void *ptr;

	ptr = pmalloc(nmemb * size);
	if (ptr)
		memset(ptr, 0, nmemb * size);
	return ptr;
}

void free(void *ptr)
{
	pfree(ptr);
}

/* 
 * pfree
 * Will unmap memory, then remap that address to anonymous with no
 * permissions. This has two effects: 
 * 1. Using free'd addresses will cause a segfault
 * 2. Freeing already free'd memory will cause a segfault (no error printed)
 */ 
void pfree(void *buf)
{
	void *start;
	size_t len, real_len;

	if (!buf) {
#ifdef PMALLOC_DEBUG
		fprintf(stderr, "pfree: NULL pointer\n");
#endif
		return;
	}

#ifdef PMALLOC_DEBUG
	fprintf(stderr, "pfree: ptr=%p\n", buf);
#endif

	start = unprotect_area(buf, &real_len, &len);

	if (munmap(start, real_len) == -1) {
		perror("pfree/munmap");
		raise(SIGSEGV);
	}
}

void *realloc(void *ptr, size_t size)
{
	return prealloc(ptr, size);
}

/*
 * prealloc
 */
void *prealloc(void *ptr, size_t size)
{
	void *new_ptr, *old_area;
	size_t old_len, old_arealen;

#ifdef PMALLOC_DEBUG
	fprintf(stderr, "prealloc: old_ptr=%p new size=%lu\n", ptr, size);
#endif

	if (!ptr)
		return pmalloc(size);
	else if (size == 0) {
		pfree(ptr);
		return NULL;
	}

	/* XXX: Dummy version.
	 * Allocate new area, find information about older area,
	 * copy data, free old area.
	 */
	new_ptr = pmalloc(size);
	old_area = unprotect_area(ptr, &old_arealen, &old_len);
	if (old_len < size)
		size = old_len;
	memcpy(new_ptr, ptr, size);
	if (munmap(old_area, old_arealen) == -1) {
		perror("prealloc/munmap");
		raise(SIGSEGV);
	}
	return new_ptr;
}


/* JIKK: signal handler
 * to gracefully exits the application
 */

#if 0
#define XML_MSG

extern const char *__progname;
void dyboc_sig_handler(int sig) {
	fprintf (stdout, "<structured_message><message_type>found_cwe</message_type> " 
					 "<cwe_entry_id>120</cwe_entry_id>\n </structured_message><structured_message>"
					 "<message_type>controlled_exit</message_type><test_case>%s</test_case>\n"
					 " </structured_message><structured_message><message_type>technical_impact"
					 "</message_type><test_case>name</test_case>\n <impact>ALTER_EXECUTION_LOGIC"
					 "</impact></structured_message>\n"
	, __progname);
	exit (0);
}
#endif

#ifdef PMALLOC_WHITELIST
#define WLIST_CONFS 1

struct whitelist {
	const char name[128];
	size_t wlist[WLIST_SIZE];
};

static struct whitelist wlist_conf[WLIST_CONFS] = {
	{ "grep", { 36865, 0, } }
};


__attribute__((constructor)) void __init(void)
{
	int fd, r, i;
	char cmdline[4096];

	//fprintf(stderr, "library loaded!\n");
	
	fd = open("/proc/self/cmdline", O_RDONLY);
	if (fd < 0)
		return;
	r = read(fd, cmdline, sizeof(cmdline));
	if (r < 0)
		goto ret;
	cmdline[sizeof(cmdline) - 1] = '\0';

	for (i = 0; i < WLIST_CONFS; i++) {
		//fprintf(stderr, "wlist: %s\n", wlist_conf[i].name);
		if (strstr(cmdline, wlist_conf[i].name) != NULL) {
			wlist = wlist_conf[i].wlist;
			//fprintf(stderr, "Found whitelist\n");
			break;
		}
	}

ret:
	close(fd);
}
#endif
