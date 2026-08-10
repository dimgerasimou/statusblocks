/* See LICENSE file for copyright and license details. */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>

#define MEMORY_C
#define BUF_SIZE 256
#define LEN(a)   (sizeof(a) / sizeof((a)[0]))

#include "colors.h"
#include "toggle.h"
#include "utils.h"
#include "config.h"

/*
 * Returns used memory in KiB (MemTotal - MemAvailable), or -1 if
 * /proc/meminfo could not be read or parsed.
 */
static long
getmemoryusage_kib(void)
{
	char  buffer[BUF_SIZE];
	FILE *fp = NULL;
	long  total = -1;
	long  avail = -1;

	fp = fopen("/proc/meminfo", "r");
	if (!fp) {
		warn("fopen() failed for: \"/proc/meminfo\":");
		return -1;
	}

	while (fgets(buffer, sizeof(buffer), fp)) {
		if (strncmp(buffer, "MemTotal:", 9) == 0) {
			if (sscanf(buffer + 9, "%ld", &total) != 1)
				total = -1;
		} else if (strncmp(buffer, "MemAvailable:", 13) == 0) {
			if (sscanf(buffer + 13, "%ld", &avail) != 1)
				avail = -1;
		}

		if (total >= 0 && avail >= 0)
			break;
	}

	fclose(fp);

	if (total < 0 || avail < 0) {
		warn("Failed to parse MemTotal/MemAvailable");
		return -1;
	}

	if (avail > total)
		return 0;

	return total - avail;
}

static void
on_right(void *ctx)
{
	(void)ctx;
	execute_term((char **)args_task_manager);
}

int
main(void)
{
	static const struct Button buttons[] = {
		{ 3, on_right },
	};

	long used_kib;

	set_name("statusblocks-memory");
	clr_init();
	toggle_init();

	dispatch(buttons, LEN(buttons), NULL);

	used_kib = getmemoryusage_kib();

	printf("%s", clr_get(clr_mem));

	if (toggle_get(show_mem))
		printf("%s", icon_memory);

	if (used_kib < 0)
		printf("--" CLR_NRM "\n");
	else
		printf("%.1fGiB" CLR_NRM "\n", (double)used_kib / 1024.0 / 1024.0);

	return 0;
}
