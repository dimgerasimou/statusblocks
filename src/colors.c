/* See LICENSE file for copyright and license details. */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "colors.h"
#include "utils.h"

#define LEN(a) (sizeof(a) / sizeof((a)[0]))

_Static_assert(LEN(clr_defaults) == CLR_SIZE, "clr_defaults in config.h must have one entry per enum Color");

#ifdef NO_COLOR

void
clr_init(void)
{
}

const char *
clr_get(enum Color clr)
{
	(void)clr;
	return "";
}

const char *
clr_hex(enum Color clr)
{
	(void)clr;
	return "";
}

#else /* NO_COLOR */

#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xresource.h>

#define CACHE_NAME "statusblocks-colors"

/* Resolved "#RRGGBB" values, and the escapes rendered from them. */
static char hexes[CLR_SIZE][CLR_HEX_LEN] = {0};
static char colors[CLR_SIZE][CLR_LEN] = {0};

static const char *const names[CLR_SIZE] = {
	"clr_bat_crt",
	"clr_bat_low",
	"clr_bat_nrm",
	"clr_bat_chg",
	"clr_bt",
	"clr_date",
	"clr_net_nrm",
	"clr_net_err",
	"clr_sys_pkg",
	"clr_sys_nrm",
	"clr_kbd",
	"clr_mem",
	"clr_pwr",
	"clr_tim",
	"clr_vol_nrm",
	"clr_vol_mut",
	"clr_cal"
};

/* Stores 'hex' for 'clr' if it is a valid "#RRGGBB" string. */
static void
sethex(enum Color clr, const char *hex)
{
	if (!ishexcolor(hex)) {
		hexes[clr][0] = '\0';
		return;
	}

	memcpy(hexes[clr], hex, CLR_HEX_LEN - 1);
	hexes[clr][CLR_HEX_LEN - 1] = '\0';
}

/* Renders every resolved colour through CLR_FMT. */
static void
render(void)
{
	for (size_t i = 0; i < CLR_SIZE; i++) {
		int n;

		if (!hexes[i][0]) {
			colors[i][0] = '\0';
			continue;
		}

		n = snprintf(colors[i], sizeof(colors[i]), CLR_FMT, hexes[i]);

		if (n < 0 || (size_t)n >= sizeof(colors[i])) {
			warn("CLR_FMT produces too long an escape for %s", names[i]);
			colors[i][0] = '\0';
		}
	}
}

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
 * which covers a rebuild after editing clr_defaults. $XDG_RUNTIME_DIR is
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
 * Reads one "#RRGGBB" per colour, in enum order; an empty line means the
 * colour is unset. Any other content means the cache is corrupt and the
 * whole file is rejected. Returns 0 on success.
 */
static int
cacheload(const char *path)
{
	char  line[CLR_HEX_LEN + 2];
	FILE *fp;

	fp = fopen(path, "r");
	if (!fp)
		return 1;

	for (size_t i = 0; i < CLR_SIZE; i++) {
		if (!fgets(line, sizeof(line), fp))
			goto corrupt;

		line[strcspn(line, "\n")] = '\0';

		if (line[0] == '\0') {
			hexes[i][0] = '\0';
			continue;
		}

		if (!ishexcolor(line))
			goto corrupt;

		memcpy(hexes[i], line, CLR_HEX_LEN);
	}

	fclose(fp);
	return 0;

corrupt:
	fclose(fp);
	memset(hexes, 0, sizeof(hexes));
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

	for (size_t i = 0; i < CLR_SIZE; i++)
		fprintf(fp, "%s\n", hexes[i]);

	if (fclose(fp) != 0 || rename(tmp, path) != 0)
		unlink(tmp);
}

/*
 * Looks 'clr' up in 'db' under "statusblocks.<name>" then "*<name>",
 * keeping
 * the first valid hit.
 */
static void
clr_load(XrmDatabase db, enum Color clr)
{
	static const char *const prefixes[] = { "statusblocks.", "*" };

	XrmValue ret;
	char    *type = NULL;
	char     name[256];

	for (size_t i = 0; i < LEN(prefixes); i++) {
		snprintf(name, sizeof(name), "%s%s", prefixes[i], names[clr]);

		if (!XrmGetResource(db, name, "*", &type, &ret) || !ret.addr)
			continue;

		if (!ishexcolor(ret.addr))
			continue;

		sethex(clr, ret.addr);
		return;
	}
}

/* Overrides the defaults with anything found in X. Returns 0 if X answered. */
static int
clr_loadall(void)
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
		for (size_t i = 0; i < CLR_SIZE; i++)
			clr_load(db, (enum Color)i);

		XrmDestroyDatabase(db);
	}

	XCloseDisplay(dpy);
	return 0;
}

void
clr_init(void)
{
	char path[PATH_MAX];
	int  havepath;

	havepath = (cachepath(path, sizeof(path)) == 0);

	if (havepath && !cachestale(path) && cacheload(path) == 0) {
		render();
		return;
	}

	for (size_t i = 0; i < CLR_SIZE; i++)
		sethex((enum Color)i, clr_defaults[i]);

	if (clr_loadall() == 0 && havepath)
		cachestore(path);

	render();
}

const char *
clr_get(enum Color clr)
{
	if ((unsigned int)clr >= (unsigned int)CLR_SIZE)
		return "";

	return colors[clr];
}

const char *
clr_hex(enum Color clr)
{
	if ((unsigned int)clr >= (unsigned int)CLR_SIZE)
		return "";

	return hexes[clr];
}

#endif /* NO_COLOR */
