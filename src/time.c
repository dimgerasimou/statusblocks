/* See LICENSE file for copyright and license details. */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <time.h>

#define TIME_C

#include "utils.h"
#include "config.h"

static void
show(void)
{
	struct timespec ts;
	struct tm      *lt = NULL;
	time_t          ct = 0;
	char            text[32];
	char            rich[64];

	/* time() reads a coarse clock that can still be in the previous
	 * minute right after main() wakes on the boundary. */
	clock_gettime(CLOCK_REALTIME, &ts);
	ct = ts.tv_sec;
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

	snprintf(rich, sizeof(rich), "%s %s", icon_time, text);
	emit("tim", rich, text);
}

int
main(void)
{
	struct timespec next;

	set_name("statusblocks-time");

	if (!persistent()) {
		show();
		return 0;
	}

	/*
	 * An absolute CLOCK_REALTIME sleep also wakes after suspend and when
	 * the clock is set, unlike a relative one.
	 */
	for (;;) {
		show();
		clock_gettime(CLOCK_REALTIME, &next);
		next.tv_sec += 60 - next.tv_sec % 60;
		next.tv_nsec = 0;
		while (clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next, NULL) == EINTR)
			;
	}
}
