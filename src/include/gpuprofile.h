/* See LICENSE file for copyright and license details. */

#ifndef STATUSBLOCKS_GPUPROFILE_H
#define STATUSBLOCKS_GPUPROFILE_H

#include <stddef.h>

/*
 * Hybrid-graphics profile switching, independent of the tool that
 * implements it. The backend is a compile-time choice; see
 * GPU_PROFILE_BACKEND in config.h.
 *
 * Two things are deliberately kept apart:
 *
 *   State  - which GPU is driving the display right now. This is what a
 *            status block should show. A backend may decide it from a
 *            rule rather than a fixed setting, in which case the saved
 *            mode does not tell you the answer.
 *
 *   Mode   - what can be asked for. Selecting one affects the next
 *            session, not the current one, since neither backend can
 *            move a running X server between GPUs.
 *
 * They are separate lists because they need not match: a backend can
 * offer an "automatic" mode that is never itself a state.
 *
 * Index 0 is the unknown entry in both, so a failed query still returns
 * something printable.
 */

/* Which GPU is driving the display now. 0 if it could not be determined. */
size_t gpuprofile_state(void);
const char *gpuprofile_state_label(size_t state);

/* Modes that can be requested. Count includes the unknown entry. */
size_t gpuprofile_mode_count(void);
const char *gpuprofile_mode_label(size_t mode);

/*
 * Writes an xmenu prompt listing the requestable modes into `buf`. Entry
 * values are mode indices, so getxmenuopt()'s result can be passed
 * straight to gpuprofile_switch().
 *
 * Returns 0 on success, -1 if the menu would not fit.
 */
int gpuprofile_menu(char *buf, size_t bufsz);

/* Requests a mode for the next session. Index 0 is ignored. */
void gpuprofile_switch(size_t mode);

#endif /* STATUSBLOCKS_GPUPROFILE_H */
