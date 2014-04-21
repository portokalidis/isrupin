#ifndef FORK_H
#define FORK_H

#include "filter.hpp"

enum FORK_STATE { FORK_UNKNOWN = 0, FORK_COMMIT, FORK_ROLLBACK, FORK_BAIL };

typedef enum FORK_STATE forkstate_t;

struct forklog 
{
	/** 
	 * Filter that marks which addresses were written. Putting this first
	 * makes things slightly faster.
	 */
	filter_t filter;
	// Semaphore used to synchronize between processes
	PIN_SEMAPHORE sem;
	// State of the forked process. Indicated whether it should send data
	// back to the mother process, or simply exit
	forkstate_t state;
	// Pipe used to receive original memory contents from forked process
	int pipefd;
#ifdef TARGET_WINDOWS
	// Handle of file used for shared memory
	void *filemap;
#endif
};

struct forklog *FLogAlloc(void);

void FLogFree(struct forklog *flog);

int CheckpointFork(struct forklog *log);

void CheckpointForkCommit(struct forklog *log);

void CheckpointForkRollback(struct forklog *log);

void CheckpointForkBail(struct forklog *flog);

#endif
