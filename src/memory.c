/* See LICENSE file for copyright and license details. */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>

#define MEMORY_C
#define BUF_SIZE 256
#define LEN(a)   (sizeof(a) / sizeof((a)[0]))

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
	char text[32];
	char rich[64];

	set_name("statusblocks-memory");

	dispatch(buttons, LEN(buttons), NULL);

	used_kib = getmemoryusage_kib();

	if (used_kib < 0)
		snprintf(text, sizeof(text), "--");
	else
		snprintf(text, sizeof(text), "%.1fGiB", (double)used_kib / 1024.0 / 1024.0);

	snprintf(rich, sizeof(rich), "%s %s", icon_memory, text);
	emit("mem", rich, text);

	return 0;
}
