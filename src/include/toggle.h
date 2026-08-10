/* See LICENSE file for copyright and license details. */

#ifndef TOGGLE_H
#define TOGGLE_H

/*
 * Which icon style each block renders: its compiled-in glyph (typically a
 * Nerd Font icon) when set, or that block's plain-ASCII fallback when
 * unset. Values come from icon_defaults in config.h, overridden by any
 * matching entry in the X resource database. Resolved values are cached
 * for the session, so only the first block to run in a session pays for
 * an X connection.
 */
enum Toggle {
	show_bat = 0,
	show_bt,
	show_date,
	show_kbd,
	show_mem,
	show_net,
	show_pwr,
	show_sys,
	show_tim,
	show_vol,
	TOGGLE_SIZE
};

/*
 * Makes the toggle table available to toggle_get(). Never fails: if the
 * cache is unusable and X is unreachable, the configured defaults are used
 * on their own.
 */
void toggle_init(void);

/* Returns nonzero if 't' should render as its compiled-in icon. */
unsigned int toggle_get(enum Toggle t);

#endif /* TOGGLE_H */
