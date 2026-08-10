/* See LICENSE file for copyright and license details. */

#define _POSIX_C_SOURCE 200809L

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XKBrules.h>

#define KEYBOARD_C
#define LEN(a) (sizeof(a) / sizeof((a)[0]))

#include "colors.h"
#include "toggle.h"
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

static void
on_left(void *ctx)
{
	char path[PATH_MAX];

	(void)ctx;

	if (envexpand(path_language_switch, path, sizeof(path)) != 0) {
		warn("failed to expand the language switch path");
		return;
	}

	executepath(path, (char **)args_language_switch);
}

int
main(void)
{
	static const struct Button buttons[] = {
		{ 1, on_left },
	};

	Display *dpy = NULL;
	XkbStateRec state;
	XkbRF_VarDefsRec vd = {0};
	char *layout = NULL;

	set_name("statusblocks-keyboard");
	clr_init();
	toggle_init();

	dispatch(buttons, LEN(buttons), NULL);

	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		warn("XOpenDisplay() failed");
		goto cleanup;
	}

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

	printf("%s", clr_get(clr_kbd));

	if (toggle_get(show_kbd))
		printf(" ");
	printf("%s" CLR_NRM "\n", layout);

cleanup:
	free(layout);
	XkbRF_FreeVarDefs_Local(&vd);
	if (dpy)
		XCloseDisplay(dpy);

	return 0;
}
