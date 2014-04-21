#ifndef THREADSTATE
#define THREADSTATE

// Type of checkpoint used for rolling back memory contents
enum CHECKPOINT_TYPES { UNKNOWN_CHECKP = 0, WLOG_CHECKP, FORK_CHECKP };
typedef enum CHECKPOINT_TYPES checkp_t;

// Can be used as bit masks
#define NORMAL 		0
#define CHECKPOINTING 	1
#define ROLLINGBACK	2

// Maximum number of system call arguments
#define SYSARGS		6

// Magic number that can by assigned to wlog_entry_t->len to store a pointer
// to a reversible system call
#define SYSCALL_ENTRY 128


class RescuePoint;
struct writeslog;
struct forklog;


// XXX: Re-arrange for performance
struct thread_state {
	unsigned int state; //!< Thread state

	//! Structures to log necessary information to recover memory state
	union {
		struct writeslog *wlog; //!< For writes log
		struct forklog *flog; //!< For filter using fork()
	} memcheckp;

	CONTEXT *checkpoint; //!< CPU state on rescue point entry
	RescuePoint *rp; //!< Active rescue point

	// Thread blocking stuff
	bool blocked; // Is thread blocked
	// The real id of a thread. Pin uses INT for a cross-platform pid
	OS_THREAD_ID real_tid;

	// Syscall stuff
	ADDRINT in_syscall; // Syscall thread is in, or 0 if not in a syscall
	ADDRINT sysargs[SYSARGS]; // System call arguments

	// Pin thread id
	THREADID tid;
};


void CheckpointAlloc(struct thread_state *ts, checkp_t type);
void CheckpointFree(struct thread_state *ts, checkp_t type);
void CheckpointRollback(struct thread_state *ts, checkp_t type, CONTEXT *ctx);

void ThreadstateInit(struct thread_state *ts, THREADID tid);
void ThreadstateCleanup(struct thread_state *ts, checkp_t type);

#endif
