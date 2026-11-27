/* See LICENSE file for copyright and license details. */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XKBrules.h>

#define KEYBOARD_C
#define LEN(a) (sizeof(a) / sizeof((a)[0]))

#include "utils.h"
#include "config.h"

static void
XkbRF_FreeVarDefs_Local(XkbRF_VarDefsRec *var_defs)
{
	if (!var_defs)
		return;

	free(var_defs->model);
	free(var_defs->layout);
	free(var_defs->variant);
	free(var_defs->options);
	free(var_defs->extra_names);
	free(var_defs->extra_values);

	/* optional: make double-free impossible if called again */
	memset(var_defs, 0, sizeof(*var_defs));
}

/*
 * Return a newly allocated copy of the n-th (0-based) token in a comma-separated list.
 * Returns NULL if not found. Caller must free().
 */
static char *
nth_csv_token_dup(const char *s, unsigned int n)
{
	const char *p, *start;
	unsigned int i = 0;

	if (!s || !*s)
		return NULL;

	p = s;
	start = s;

	while (1) {
		if (*p == ',' || *p == '\0') {
			if (i == n) {
				size_t len = (size_t)(p - start);
				char *out = malloc(len + 1);
				if (!out)
					return NULL;
				memcpy(out, start, len);
				out[len] = '\0';
				return out;
			}
			if (*p == '\0')
				break;
			i++;
			start = p + 1;
		}
		p++;
	}

	return NULL;
}

/* Locks the next group, wrapping, as a grp:*_toggle option would. */
static void
on_left(void *ctx)
{
	XkbRF_VarDefsRec vd = {0};
	XkbStateRec      state;
	Display         *dpy;
	unsigned int     n = 1;

	(void)ctx;

	if (!(dpy = XOpenDisplay(NULL))) {
		warn("XOpenDisplay() failed");
		return;
	}

	if (XkbGetState(dpy, XkbUseCoreKbd, &state) == Success
	    && XkbRF_GetNamesProp(dpy, NULL, &vd) && vd.layout) {
		for (const char *p = vd.layout; *p; p++)
			n += *p == ',';
		XkbLockGroup(dpy, XkbUseCoreKbd, ((unsigned int)state.locked_group + 1) % n);
	} else {
		warn("failed to read the keyboard state");
	}

	XkbRF_FreeVarDefs_Local(&vd);
	XCloseDisplay(dpy);
}

static void
show(Display *dpy)
{
	XkbStateRec      state;
	XkbRF_VarDefsRec vd = {0};
	char            *layout = NULL;
	char             rich[64];

	if (XkbGetState(dpy, XkbUseCoreKbd, &state) != Success) {
		warn("XkbGetState() failed");
		goto cleanup;
	}

	if (!XkbRF_GetNamesProp(dpy, NULL, &vd) || !vd.layout || !*vd.layout) {
		warn("XkbRF_GetNamesProp() failed");
		goto cleanup;
	}

	layout = nth_csv_token_dup(vd.layout, (unsigned int)state.group);
	if (!layout || !*layout) {
		warn("Invalid layout for given group");
		goto cleanup;
	}

	snprintf(rich, sizeof(rich), "%s %s", icon_keyboard, layout);
	emit("kbd", rich, layout);

cleanup:
	free(layout);
	XkbRF_FreeVarDefs_Local(&vd);
}

/*
 * Group switches arrive as XkbStateNotify; setxkbmap rewrites the layout
 * list in _XKB_RULES_NAMES and sends a new keymap. A lost X connection
 * exits through Xlib's IO error handler, and dwmblocks restarts the block.
 */
static int
watch(Display *dpy)
{
	Atom   names;
	XEvent ev;
	int    xkbevent;

	if (!XkbQueryExtension(dpy, NULL, &xkbevent, NULL, NULL, NULL)) {
		warn("XkbQueryExtension() failed");
		return 1;
	}

	names = XInternAtom(dpy, "_XKB_RULES_NAMES", False);
	XSelectInput(dpy, DefaultRootWindow(dpy), PropertyChangeMask);
	XkbSelectEventDetails(dpy, XkbUseCoreKbd, XkbStateNotify,
	                      XkbGroupStateMask, XkbGroupStateMask);
	XkbSelectEvents(dpy, XkbUseCoreKbd, XkbNewKeyboardNotifyMask,
	                XkbNewKeyboardNotifyMask);

	for (;;) {
		show(dpy);
		do {
			XNextEvent(dpy, &ev);
		} while (ev.type != xkbevent
		         && !(ev.type == PropertyNotify && ev.xproperty.atom == names));
	}
}

int
main(void)
{
	static const struct Button buttons[] = {
		{ 1, on_left },
	};

	Display *dpy;
	int      ret = 0;

	set_name("statusblocks-keyboard");

	if (persistent()) {
		if (!(dpy = XOpenDisplay(NULL))) {
			warn("XOpenDisplay() failed");
			return 1;
		}
		ret = watch(dpy);
		XCloseDisplay(dpy);
		return ret;
	}

	dispatch(buttons, LEN(buttons), NULL);

	if (!(dpy = XOpenDisplay(NULL))) {
		warn("XOpenDisplay() failed");
		return 0;
	}
	show(dpy);
	XCloseDisplay(dpy);

	return 0;
}
