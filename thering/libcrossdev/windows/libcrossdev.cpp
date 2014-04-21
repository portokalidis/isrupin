using namespace std;

#include "../libcrossdev.hpp"
#include <Windows.h>


/**
 * Map memory pages.
 *
 * @param map_p Output pointer to store address of mapped area.
 * @param len	Length of memory map in bytes.
 * @param prot	Protection flag, can be one of MEMORY_PAGE_RW, and 
 * 		MEMORY_PAGE_RO.
 *
 * @return true if the call completed successfully, or false otherwise.
 */
bool memory_map(void **map_p, size_t len, int prot, int flags)
{
	if (flags & MEMORY_MAP_SHARED)
		// XXX: We do not know how to do shared at this point
		return false;

	*map_p = VirtualAlloc(NULL, len, MEM_RESERVE|MEM_COMMIT|flags, prot);
	if (*map_p == NULL)
		return false;
	return true;
}

/**
 * Protect memory pages, so they are not accessible.
 *
 * @param map	Addess of map area to protect.
 * @param len	How many bytes to protect.
 * @param prot	Protection flag, can be one of MEMORY_PAGE_RW, and 
 * 		MEMORY_PAGE_RO.
 *
 * @return true if the call completed successfully, or false otherwise.
 */
bool memory_protect(void *map, size_t len, int prot)
{
	DWORD old_prot;

	if (!VirtualProtect(map, len, prot, &old_prot))
		return false;
	return true;
}

/**
 * Unmap memory pages.
 *
 * @param map	Addess of area to unmap.
 * @param len	Length of area.
 *
 * @return true if the call completed successfully, or false otherwise.
 */
bool memory_unmap(void *buf, size_t len)
{
	if (!VirtualFree(buf, len, MEM_DECOMMIT|MEM_RELEASE))
		return false;
	return true;
}

/**
 * Pause calling thread.
 */
void pause()
{
	Sleep(INFINITE);
}


/**
 * Returns the thread identifier of the calling thread.
 *
 * @return The thread id
 */
uint32_t get_thread_id()
{
	return (uint32_t)GetCurrentThreadId();
}


/**
 * Retrieve system information, like the page size used by the system
 *
 * @param info Pointer to structure that will receive the information
 *
 * @return true on success, or fale on error
 */
bool get_system_info(struct system_info *info)
{
	SYSTEM_INFO win_info;

	GetSystemInfo(&win_info);

	info->page_size = win_info.dwPageSize;
	return true;
}
