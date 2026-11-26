/* See LICENSE file for copyright and license details. */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>

#define KERNEL_C

#include "utils.h"
#include "config.h"

int
main(void)
{
	struct utsname un;
	char           rich[sizeof(icon_kernel) + sizeof(un.release) + 1];

	set_name("statusblocks-kernel");

	if (uname(&un) != 0) {
		warn("uname():");
		snprintf(un.release, sizeof(un.release), "--");
	}

	/* strip a suffix like "-arch1-1" */
	un.release[strcspn(un.release, "-")] = '\0';

	snprintf(rich, sizeof(rich), "%s %s", icon_kernel, un.release);
	emit("sys_nrm", rich, un.release);

	return 0;
}
