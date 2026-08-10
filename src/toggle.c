/* See LICENSE file for copyright and license details. */

#define _POSIX_C_SOURCE 200809L

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xresource.h>

#include "config.h"
#include "toggle.h"
#include "utils.h"

#define LEN(a) (sizeof(a) / sizeof((a)[0]))
#define CACHE_NAME "statusblocks-icons"

_Static_assert(LEN(icon_defaults) == TOGGLE_SIZE, "icon_defaults in config.h must have one entry per enum Toggle");

static unsigned int values[TOGGLE_SIZE] = {0};

static const char *const names[TOGGLE_SIZE] = {
	"icon_bat",
	"icon_bt",
	"icon_date",
	"icon_kbd",
	"icon_mem",
	"icon_net",
	"icon_pwr",
	"icon_sys",
	"icon_tim",
	"icon_vol"
};

/*
 * Builds the cache path in $XDG_RUNTIME_DIR, falling back to /tmp keyed by
 * uid. Returns 0 on success.
 */
static int
cachepath(char *out, const size_t outsz)
{
	const char *dir = getenv("XDG_RUNTIME_DIR");
	int         n;

	if (dir && *dir)
		n = snprintf(out, outsz, "%s/%s", dir, CACHE_NAME);
	else
		n = snprintf(out, outsz, "/tmp/%s-%u", CACHE_NAME,
		             (unsigned int)getuid());

	return (n < 0 || (size_t)n >= outsz) ? 1 : 0;
}

/*
 * The cache is stale if xresources_path is newer than it, which covers the
 * usual "edit the file, run xrdb" workflow, or if this executable is newer,
 * which covers a rebuild after editing icon_defaults. $XDG_RUNTIME_DIR is
 * cleared at logout, so a fresh session always rebuilds.
 */
static int
cachestale(const char *cache)
{
	struct stat cst, other;
	char        xres[PATH_MAX];

	if (stat(cache, &cst) != 0)
		return 1;

	if (stat("/proc/self/exe", &other) == 0 && other.st_mtime > cst.st_mtime)
		return 1;

	if (envexpand(xresources_path, xres, sizeof(xres)) != 0)
		return 0;

	if (stat(xres, &other) != 0)
		return 0;

	return other.st_mtime > cst.st_mtime;
}

/*
 * Reads one "0" or "1" per toggle, in enum order. Any other content means
 * the cache is corrupt and the whole file is rejected. Returns 0 on
 * success.
 */
static int
cacheload(const char *path)
{
	char  line[16];
	FILE *fp;

	fp = fopen(path, "r");
	if (!fp)
		return 1;

	for (size_t i = 0; i < TOGGLE_SIZE; i++) {
		unsigned int v;

		if (!fgets(line, sizeof(line), fp))
			goto corrupt;

		if (sscanf(line, "%u", &v) != 1 || (v != 0 && v != 1))
			goto corrupt;

		values[i] = v;
	}

	fclose(fp);
	return 0;

corrupt:
	fclose(fp);
	return 1;
}

/* Written via a temporary file so concurrent blocks never see a partial cache. */
static void
cachestore(const char *path)
{
	char  tmp[PATH_MAX];
	FILE *fp;
	int   fd, n;

	n = snprintf(tmp, sizeof(tmp), "%s.XXXXXX", path);
	if (n < 0 || (size_t)n >= sizeof(tmp))
		return;

	fd = mkstemp(tmp);
	if (fd < 0)
		return;

	fp = fdopen(fd, "w");
	if (!fp) {
		close(fd);
		unlink(tmp);
		return;
	}

	for (size_t i = 0; i < TOGGLE_SIZE; i++)
		fprintf(fp, "%u\n", values[i]);

	if (fclose(fp) != 0 || rename(tmp, path) != 0)
		unlink(tmp);
}

/*
 * Looks 't' up in 'db' under "statusblocks.<name>" then "*<name>", keeping
 * the first valid hit.
 */
static void
toggle_load(XrmDatabase db, enum Toggle t)
{
	static const char *const prefixes[] = { "statusblocks.", "*" };

	XrmValue     ret;
	char        *type = NULL;
	char         name[256];
	unsigned int v;

	for (size_t i = 0; i < LEN(prefixes); i++) {
		snprintf(name, sizeof(name), "%s%s", prefixes[i], names[t]);

		if (!XrmGetResource(db, name, "*", &type, &ret) || !ret.addr)
			continue;

		if (sscanf(ret.addr, "%u", &v) != 1 || (v != 0 && v != 1))
			continue;

		values[t] = v;
		return;
	}
}

/* Overrides the defaults with anything found in X. Returns 0 if X answered. */
static int
toggle_loadall(void)
{
	Display     *dpy;
	XrmDatabase  db;
	char        *resm;

	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		warn("XOpenDisplay() failed, using the configured defaults");
		return 1;
	}

	XrmInitialize();

	resm = XResourceManagerString(dpy);
	db   = resm ? XrmGetStringDatabase(resm) : NULL;

	if (db) {
		for (size_t i = 0; i < TOGGLE_SIZE; i++)
			toggle_load(db, (enum Toggle)i);

		XrmDestroyDatabase(db);
	}

	XCloseDisplay(dpy);
	return 0;
}

void
toggle_init(void)
{
	char path[PATH_MAX];
	int  havepath;

	havepath = (cachepath(path, sizeof(path)) == 0);

	if (havepath && !cachestale(path) && cacheload(path) == 0)
		return;

	for (size_t i = 0; i < TOGGLE_SIZE; i++)
		values[i] = icon_defaults[i];

	if (toggle_loadall() == 0 && havepath)
		cachestore(path);
}

unsigned int
toggle_get(enum Toggle t)
{
	if ((unsigned int)t >= (unsigned int)TOGGLE_SIZE)
		return 1;

	return values[t];
}
