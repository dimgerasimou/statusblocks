/* See LICENSE file for copyright and license details. */

#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BATTERY_C

#include "colors.h"
#include "toggle.h"
#include "utils.h"
#include "config.h"

#define LEN(a)    (sizeof(a) / sizeof((a)[0]))
#define BODY_SIZE 256


#ifdef POWER_MANAGEMENT
struct Optimus {
	const char *name;
	const char *icon;
};

static const struct Optimus optimus[] = {
	{ "Unmanaged",  "battery" },
	{ "Integrated", "intel" },
	{ "Hybrid",     "deepin-graphics-driver-manager" },
	{ "Nvidia",     "nvidia" }
};

static unsigned int
getmode(void)
{
	char  buf[256];
	FILE *ep;

	ep = popen("optimus-manager --status", "r");
	if (!ep) {
		warn("popen() for: \"optimus-manager --status\":");
		return 0;
	}

	buf[0] = '\0';
	while (fgets(buf, sizeof(buf), ep)) {
		if (strstr(buf, "Current"))
			break;
		buf[0] = '\0';
	}

	pclose(ep);

	if (strstr(buf, "integrated"))
		return 1;
	if (strstr(buf, "hybrid"))
		return 2;
	if (strstr(buf, "nvidia"))
		return 3;

	return 0;
}

static void
send_notification(const char *cap, const char *st)
{
	unsigned int mode = getmode();
	char         body[BODY_SIZE];
	int          n;

	n = snprintf(body, sizeof(body),
	             "Battery capacity: %s%%\n"
	             "Battery status:   %s\n"
	             "Optimus Manager:  %s",
	             cap, st, optimus[mode].name);

	if (n < 0 || (size_t)n >= sizeof(body))
		warn("notification body truncated");

	notify("Power", body, optimus[mode].icon);
}

#else /* POWER_MANAGEMENT */

static void
send_notification(const char *cap, const char *st)
{
	char body[BODY_SIZE];
	int  n;

	n = snprintf(body, sizeof(body),
	             "Battery capacity: %s%%\n"
	             "Battery status:   %s",
	             cap, st);

	if (n < 0 || (size_t)n >= sizeof(body))
		warn("notification body truncated");

	notify("Power", body, "battery");
}

#endif /* POWER_MANAGEMENT */

/*
 * Locates the first battery device in sysfs. On success writes its
 * directory into 'out' and returns 0; returns 1 if none was found.
 */
static int
getbatterypath(char *out, const size_t outsz)
{
	static const char base[] = "/sys/class/power_supply";

	DIR           *d;
	struct dirent *e;
	char           buf[64];
	char           path[PATH_MAX];

	d = opendir(base);
	if (!d)
		return 1;

	while ((e = readdir(d))) {
		FILE *fp;
		int   n;

		if (e->d_name[0] == '.')
			continue;

		if (strncmp(e->d_name, "BAT", strlen("BAT")) != 0)
			continue;

		n = snprintf(path, sizeof(path), "%s/%s/type", base, e->d_name);
		if (n < 0 || (size_t)n >= sizeof(path))
			continue;

		fp = fopen(path, "r");
		if (!fp)
			continue;

		if (fgets(buf, sizeof(buf), fp)) {
			buf[strcspn(buf, "\n")] = '\0';

			if (strcmp(buf, "Battery") == 0) {
				fclose(fp);
				closedir(d);

				n = snprintf(out, outsz, "%s/%s", base, e->d_name);
				return (n < 0 || (size_t)n >= outsz) ? 1 : 0;
			}
		}

		fclose(fp);
	}

	closedir(d);
	return 1;
}

/*
 * Reads the single-line sysfs attribute 'attr' under 'base' into 'out',
 * stripping the trailing newline. Returns 0 on success, 1 otherwise.
 */
static int
readattr(const char *base, const char *attr, char *out, const size_t outsz)
{
	FILE *fp;
	char  path[PATH_MAX];
	int   n;

	n = snprintf(path, sizeof(path), "%s/%s", base, attr);
	if (n < 0 || (size_t)n >= sizeof(path))
		return 1;

	fp = fopen(path, "r");
	if (!fp)
		return 1;

	if (!fgets(out, (int)outsz, fp)) {
		fclose(fp);
		return 1;
	}

	fclose(fp);

	out[strcspn(out, "\n")] = '\0';
	return 0;
}

static unsigned int
getcapacity(const char *base)
{
	char         buf[16];
	unsigned int cap;

	if (readattr(base, "capacity", buf, sizeof(buf)) != 0) {
		warn("failed to read capacity under %s", base);
		return 0;
	}

	if (sscanf(buf, "%u", &cap) != 1)
		return 0;

	return cap > 100 ? 100 : cap;
}

static void
getstatus(const char *base, char *out, const size_t outsz)
{
	if (readattr(base, "status", out, outsz) != 0) {
		warn("failed to read status under %s", base);
		snprintf(out, outsz, "Unknown");
	}
}

struct Status {
	unsigned int cap;
	const char  *st;
};

static void
on_left(void *ctx)
{
	const struct Status *s = ctx;
	char                 c[12];

	snprintf(c, sizeof(c), "%u", s->cap);
	send_notification(c, s->st);
}

static size_t
battery_icon_index(const unsigned int cap)
{
	if (cap < 10)
		return 0;
	if (cap < 30)
		return 1;
	if (cap < 50)
		return 2;
	if (cap < 75)
		return 3;
	return 4;
}

/*
 * Formats the ASCII fallback into 'out': ascii_bat_chg while charging,
 * otherwise ascii_bat_tag followed by the live percentage.
 */
static void
ascii_format(char *out, const size_t outsz, const unsigned int cap, const int charging)
{
	int n;

	if (charging)
		n = snprintf(out, outsz, "%s", ascii_bat_chg);
	else
		n = snprintf(out, outsz, "%s%u%%", ascii_bat_tag, cap);

	if (n < 0 || (size_t)n >= outsz)
		warn("battery ascii string truncated");
}

int
main(void)
{
	/* Colour per icon index, parallel to icons_battery in config.h. */
	static const enum Color icon_colors[] = {
		clr_bat_crt, clr_bat_low, clr_bat_nrm,
		clr_bat_nrm, clr_bat_nrm, clr_bat_chg
	};

	static const struct Button buttons[] = {
		{ 1, on_left },
	};

	struct Status s;
	char          base[PATH_MAX];
	char          st[64];
	char          text[16];
	size_t        i;
	int           ascii;
	int           charging;

	set_name("statusblocks-battery");
	clr_init();
	toggle_init();

	ascii = !toggle_get(show_bat);

	if (getbatterypath(base, sizeof(base)) != 0) {
		/*
		 * No battery is a normal state on a desktop, so render the
		 * empty state rather than leaving the bar blank.
		 */
		printf("%s%s" CLR_NRM "\n", clr_get(icon_colors[0]),
		       ascii ? ascii_bat_none : icons_battery[0]);
		return 0;
	}

	s.cap = getcapacity(base);
	getstatus(base, st, sizeof(st));
	s.st = st;

	dispatch(buttons, LEN(buttons), &s);

	charging = (strcmp(st, "Charging") == 0);
	i = charging ? 5 : battery_icon_index(s.cap);
	if (i >= LEN(icons_battery))
		i = 0;

	if (ascii) {
		ascii_format(text, sizeof(text), s.cap, charging);
		printf("%s%s" CLR_NRM "\n", clr_get(icon_colors[i]), text);
	} else {
		printf("%s%s" CLR_NRM "\n", clr_get(icon_colors[i]), icons_battery[i]);
	}

	return 0;
}
