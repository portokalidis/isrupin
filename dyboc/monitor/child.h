#ifndef CHILD_H
#define CHILD_H

//! Exit status enum for reporting minestrone style
typedef enum ACTION_MESSAGE_TYPE_ENUM { 
	ACTIONMSG_NONE, ACTIONMSG_CONTROLLED_EXIT, ACTIONMSG_CONTINUED_EXIT, 
	ACTIONMSG_OTHER } actionmsg_type_t;

int child_execute(int cmdline_idx, char **argv);

int child_monitor();

#endif
