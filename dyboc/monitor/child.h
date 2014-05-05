#ifndef CHILD_H
#define CHILD_H

//! Exit status enum for reporting minestrone style
typedef enum ACTION_MESSAGE_TYPE_ENUM { 
	ACTIONMSG_NONE = 0, ACTIONMSG_CONTROLLED_EXIT, 
	ACTIONMSG_CONTINUED_EXECUTION, ACTIONMSG_OTHER } actionmsg_type_t;

typedef enum STATUS_ENUM { STATUS_SUCCESS = 0, STATUS_SKIP, 
	STATUS_TIMEOUT, STATUS_OTHER } status_type_t;


int child_execute(int cmdline_idx, char **argv);

int child_monitor();

#endif
