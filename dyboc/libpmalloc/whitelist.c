#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "whitelist.h"

#define WLIST_SIZE 16

static size_t *wlist = NULL;

#define WLIST_CONFS 1

struct whitelist {
	const char name[128];
	size_t wlist[WLIST_SIZE];
};

static struct whitelist wlist_conf[WLIST_CONFS] = {
	{ "grep", { 36865, 0, } }
};


/**
 * Initialize whitelist
 */
void whitelist_init()
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

/**
 * Is the allocation length in the whitelist
 */
int whitelist_has_length(size_t len)
{
	if (wlist) {
		size_t *wl;
		for (wl = wlist; *wl > 0; wl++)
			if (*wl == len) {
				//len++;
				//fprintf(stderr, "Increase size of allocation %d\n", len);
				return 1;
			}
	}
	/*
	if (len == 36865) {
		len++;
	}
	*/
	return 0;
}

