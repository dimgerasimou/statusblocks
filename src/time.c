/* See LICENSE file for copyright and license details. */

#include <stdio.h>
#include <time.h>

#define TIME_C

#include "utils.h"
#include "config.h"

int
main(void)
{
	struct tm *lt = NULL;
	time_t     ct = 0;
	char       text[32];
	char       rich[64];

	set_name("statusblocks-time");

	ct = time(NULL);
	lt = localtime(&ct);

	/*
	 * A block that exits without printing leaves the bar showing stale
	 * text, so failures still emit a placeholder.
	 */
	if (lt) {
		snprintf(text, sizeof(text), "%.2d:%.2d", lt->tm_hour, lt->tm_min);
	} else {
		warn("localtime:");
		snprintf(text, sizeof(text), "--:--");
	}

	snprintf(rich, sizeof(rich), "%s%s", icon_time, text);
	emit("tim", rich, text);

	return 0;
}
