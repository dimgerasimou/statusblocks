/* See LICENSE file for copyright and license details. */

#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define POWERPROFILE_C

#include "powerprofile.h"
#include "utils.h"
#include "config.h"

#ifdef POWERPROFILE

#define LEN(a) (sizeof(a) / sizeof((a)[0]))

size_t
powerprofile_state(void)
{
	char        line[256];
	const char *name;
	size_t      len;

	if (cmdfindline(power_profile_status_cmd, power_profile_state_key,
	                line, sizeof(line)) < 0)
		return 0;

	/* The profile name is the word right after the key. */
	name = strstr(line, power_profile_state_key) + strlen(power_profile_state_key);
	for (len = 0; name[len] && !isspace((unsigned char)name[len]); len++)
		;

	/* Whole-word match: "ac" must not select a profile named "acer". */
	for (size_t i = 1; i < LEN(power_profile_states); i++) {
		const char *match = power_profile_states[i].match;

		if (strlen(match) == len && strncmp(name, match, len) == 0)
			return i;
	}

	return 0;
}

const char *
powerprofile_state_label(size_t state)
{
	if (state >= LEN(power_profile_states))
		state = 0;

	return power_profile_states[state].label;
}

int
powerprofile_menu(char *buf, size_t bufsz)
{
	size_t off = 0;

	if (!bufsz)
		return -1;

	buf[0] = '\0';

	for (size_t i = 1; i < LEN(power_profile_modes); i++) {
		int n = snprintf(buf + off, bufsz - off, "%s%s\t%zu",
		                 off ? "\n" : "", power_profile_modes[i].label, i);

		if (n < 0 || (size_t)n >= bufsz - off) {
			warn("power profile menu too long");
			return -1;
		}
		off += (size_t)n;
	}

	return 0;
}

void
powerprofile_switch(size_t mode)
{
	if (mode == 0 || mode >= LEN(power_profile_modes))
		return;

	execute((char **)power_profile_modes[mode].argv);
}

#endif /* POWERPROFILE */
