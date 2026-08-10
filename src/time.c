/* See LICENSE file for copyright and license details. */

#include <stdio.h>
#include <time.h>

#define TIME_C

#include "colors.h"
#include "toggle.h"
#include "utils.h"
#include "config.h"

int
main(void)
{
	struct tm *lt = NULL;
	time_t     ct = 0;

	set_name("statusblocks-time");
	clr_init();
	toggle_init();

	ct = time(NULL);
	lt = localtime(&ct);

	printf("%s", clr_get(clr_tim));

	if (toggle_get(show_tim))
		printf("%s", icon_time);

	/*
	 * A block that exits without printing leaves the bar showing stale
	 * text, so failures still emit a placeholder.
	 */
	if (lt)
		printf("%.2d:%.2d" CLR_NRM "\n", lt->tm_hour, lt->tm_min);
	else {
		warn("localtime:");
		printf("--:--" CLR_NRM "\n");
	}

	return 0;
}
