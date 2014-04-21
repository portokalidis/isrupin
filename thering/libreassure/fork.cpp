#include <iostream>
#include <sstream>
#include <cassert>

#ifdef TARGET_LINUX
extern "C" {
# include <unistd.h>
# include <sys/mman.h>
# include <sys/types.h>
# include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
}
#endif

#include "pin.H"

#ifdef TARGET_WINDOWS
// Include after pin.H and in a separate namespace to avoid conflicts
namespace WND {
#include "windows.h"
}

// Name for the shared fork log
#define SHARED_MAP_NAME "Global\\TheRingForkLog"
#endif

#include "cache.h"
#include "fork.h"
#include "log.hpp"
#include "debug.h"


#ifdef TARGET_WINDOWS
// Need to use the namespace
using namespace WND;
#endif


// Main routine for checkpoint process
// Since this is a fork() off Pin, we only use cerr for logging to interfere the
// least possible with Pin
static void CheckpointChild(struct forklog *flog, int pipe)
{
	//int r;
	//stringstream ss;

#ifdef FLOG_DEBUG
	DBGLOG("Checkpoint process running\n");
#endif

	/*
retry:
	if  ((r = PIN_SemaphoreWait(&flog->sem)) != 0) {
		if (errno == EINTR)
			goto retry;
		ss << "checkpoint child error while waiting: " << 
			strerror(errno) << endl;
		ERRLOG(ss);
	}
	*/
	PIN_SemaphoreWait(&flog->sem);

	switch (flog->state) {
	case FORK_COMMIT:
#ifdef FLOG_DEBUG
		DBGLOG("Checkpoint process committing\n");
#endif
		break;

	case FORK_BAIL:
#ifdef FLOG_DEBUG
		DBGLOG("Checkpoint process bailing out\n");
#endif
		break;

	case FORK_ROLLBACK:
#ifdef FLOG_DEBUG
		DBGLOG("Checkpoint process rolling back\n");
#endif
		filter_child_rollback(&flog->filter, pipe);
		break;

	default:
		ERRLOG("Unknown FORK state\n");
		break;
	}

	close(pipe);

#ifdef FLOG_DEBUG
	ERRLOG("Checkpoint process exiting\n");
#endif
	exit(0);
}

/**
 * Commit changes, the checkpoint process need to do nothing.
 *
 * @param flog Pointer to forklog structure
 */
void CheckpointForkCommit(struct forklog *flog)
{
#ifdef FLOG_DEBUG
	DBGLOG("Signaling checkpoint process to commit\n");
#endif
	flog->state = FORK_COMMIT;
	/*
	if (PIN_SemaphoreSet(&flog->sem) != 0) {
		stringstream ss;

		ss << "checkpoint fork commit error while signaling child: ";
		ss << strerror(errno) << endl;
		ERRLOG(ss);
	}
	*/
	PIN_SemaphoreSet(&flog->sem);
	close(flog->pipefd);
}

/**
 * Abandon checkpoint due to an error.
 * The checkpoint process is signaled to exit.
 *
 * @param flog Pointer to forklog structure
 */
void CheckpointForkBail(struct forklog *flog)
{
#ifdef FLOG_DEBUG
	DBGLOG("Signaling checkpoint process to bail out\n");
#endif
	flog->state = FORK_BAIL;
	/*
	if (PIN_SemaphoreSet(&flog->sem) != 0) {
		stringstream ss;

		ss << "checkpoint fork commit error while signaling child: ";
		ss << strerror(errno) << endl;
		ERRLOG(ss);
	}
	*/
	PIN_SemaphoreSet(&flog->sem);
	close(flog->pipefd);
}

/**
 * Rollback changes, we will receive the original memory contents from the
 * checkpoint process
 *
 * @param flog Pointer to forklog structure
 */
void CheckpointForkRollback(struct forklog *flog)
{

#ifdef FLOG_DEBUG
	DBGLOG("Signaling checkpoint process to rollback\n");
#endif

	flog->state = FORK_ROLLBACK;
	/*
	if (PIN_SemaphoreSet(&flog->sem) != 0) {
		stringstream ss;
		ss << "checkpoint fork rollback error while signaling child: ";
		ss << strerror(errno) << endl;
		ERRLOG(ss);
	}*/
	PIN_SemaphoreSet(&flog->sem);

	if (!filter_parent_rollback(flog->pipefd)) {
		ERRLOG("checkpoint fork rollback failed while recovering "
				"memory contents\n"); 
		PIN_ExitProcess(1);
	}
	close(flog->pipefd);

	// XXX: Is it safe here?
	//PIN_SemaphoreFini(&flog->sem);

#ifdef FLOG_DEBUG
	DBGLOG("Rollback through checkpoint process completed \n");
#endif
}

/**
 * Perform a checkpoint by forking a process.
 * A filter shared between the real process and the checkpoint (assistant)
 * process is used to mark the memory areas that were written and need to be
 * rolled back in case of an error later on.
 *
 * @param flog Pointer to forklog structure.
 * @return 0 on success, or -1 on error.
 */
int CheckpointFork(struct forklog *flog)
{
	int fds[2];
	pid_t p;
	stringstream ss;

	if (pipe(fds) != 0) {
		ss << "checkpoint fork() could not create pipe: ";
err:
		ss << strerror(errno) << endl;
		ERRLOG(ss);
		PIN_ExitProcess(EXIT_FAILURE);
		return -1;
	}

	// Initialize shared semaphore
	// XXX: Needs to be destroyed
	if (!PIN_SemaphoreInit(&flog->sem) != 0) {
		ss << "checkpoint fork() could not initialize semaphore: ";
		goto err;
	}

	p = fork();
	if (p < 0) {
		ss << "checkpoint fork() could not create process: ";
		goto err;
	} else if (p == 0) { // Child
		close(fds[0]);
		CheckpointChild(flog, fds[1]);
		return 0; // Never return
	}

	// Parent
	close(fds[1]);
	flog->pipefd = fds[0];
	return 0;
}

static struct forklog *FLogMap(void)
{
	struct forklog *map;

#ifdef TARGET_WINDOWS
	HANDLE filemap;

	filemap = WND::CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
		0, sizeof(struct forklog), SHARED_MAP_NAME);
	assert(filemap);

	map = (struct forklog *)MapViewOfFile(filemap, FILE_MAP_ALL_ACCESS,
		0, 0, sizeof(struct forklog));
	assert(map);

	map->filemap = (void *)filemap;
#endif

#ifdef TARGET_LINUX
	map = (struct forklog *)mmap(NULL, sizeof(struct forklog), 
			PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	assert(map != MAP_FAILED);
#endif
	return map;
}

void FLogUnmap(struct forklog *flog)
{
#ifdef TARGET_WINDOWS
	HANDLE filemap;

	filemap = (HANDLE)flog->filemap;
	UnmapViewOfFile(flog);
	WND::CloseHandle(filemap);
#endif

#ifdef TARGET_LINUX
	munmap(flog, sizeof(struct forklog));
#endif
}

struct forklog *FLogAlloc(void)
{
	struct forklog *flog;

	flog = FLogMap();
	filter_init(&flog->filter);

	return flog;
}

void FLogFree(struct forklog *flog)
{
	filter_cleanup(&flog->filter);
	FLogUnmap(flog);
}

