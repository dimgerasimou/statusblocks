/* See LICENSE file for copyright and license details. */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>

#define GPUPROFILE_C

#include "gpuprofile.h"
#include "utils.h"
#include "config.h"

#ifdef GPU_PROFILE

#define LEN(a) (sizeof(a) / sizeof((a)[0]))

size_t
gpuprofile_state(void)
{
	char line[256];

	if (cmdfindline(gpu_profile_status_cmd, gpu_profile_state_key,
	                line, sizeof(line)) < 0)
		return 0;

	/* Matched in table order: an entry's text may appear inside another
	 * backend's phrasing, so the more specific entries come first. */
	for (size_t i = 1; i < LEN(gpu_profile_states); i++) {
		if (strstr(line, gpu_profile_states[i].match))
			return i;
	}

	return 0;
}

const char *
gpuprofile_state_label(size_t state)
{
	if (state >= LEN(gpu_profile_states))
		state = 0;

	return gpu_profile_states[state].label;
}

size_t
gpuprofile_mode_count(void)
{
	return LEN(gpu_profile_modes);
}

const char *
gpuprofile_mode_label(size_t mode)
{
	if (mode >= LEN(gpu_profile_modes))
		mode = 0;

	return gpu_profile_modes[mode].label;
}

int
gpuprofile_menu(char *buf, size_t bufsz)
{
	size_t off = 0;

	if (!bufsz)
		return -1;

	buf[0] = '\0';

	for (size_t i = 1; i < LEN(gpu_profile_modes); i++) {
		int n = snprintf(buf + off, bufsz - off, "%s%s\t%zu",
		                 off ? "\n" : "", gpu_profile_modes[i].label, i);

		if (n < 0 || (size_t)n >= bufsz - off) {
			warn("GPU profile menu too long");
			return -1;
		}
		off += (size_t)n;
	}

	return 0;
}

void
gpuprofile_switch(size_t mode)
{
	if (mode == 0 || mode >= LEN(gpu_profile_modes))
		return;

	execute((char **)gpu_profile_modes[mode].argv);
}

#endif /* GPU_PROFILE */
