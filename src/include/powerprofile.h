/* See LICENSE file for copyright and license details. */

#ifndef STATUSBLOCKS_POWERPROFILE_H
#define STATUSBLOCKS_POWERPROFILE_H

#include <stddef.h>

/*
 * System power profile switching through powerprofile(1), the tool that
 * applies a named group of CPU, EC, Wi-Fi and brightness settings. Enabled
 * with POWERPROFILE in config.h.
 *
 * It mirrors gpuprofile.h, and the same two things are kept apart:
 *
 *   State  - which profile was applied last. This is what a status block
 *            should show. Under `--auto` it follows the AC adapter, so it
 *            is not something the user necessarily asked for.
 *
 *   Mode   - what can be asked for: automatic selection, or one named
 *            profile. Unlike a GPU switch, it takes effect immediately.
 *
 * Index 0 is the unknown entry in the state list, so a failed query still
 * returns something printable. It is not a mode: index 0 of the mode list
 * is a placeholder that is never offered or run.
 */

/* Profile applied last. 0 if it could not be determined or is not listed. */
size_t powerprofile_state(void);
const char *powerprofile_state_label(size_t state);

/*
 * Writes an xmenu prompt listing the requestable modes into `buf`. Entry
 * values are mode indices, so getxmenuopt()'s result can be passed
 * straight to powerprofile_switch().
 *
 * Returns 0 on success, -1 if the menu would not fit.
 */
int powerprofile_menu(char *buf, size_t bufsz);

/* Applies a mode now. Index 0 is ignored. */
void powerprofile_switch(size_t mode);

#endif /* STATUSBLOCKS_POWERPROFILE_H */
