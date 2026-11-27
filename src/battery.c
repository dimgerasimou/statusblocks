/* See LICENSE file for copyright and license details. */

#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <linux/netlink.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BATTERY_C

#include "gpuprofile.h"
#include "powerprofile.h"
#include "utils.h"
#include "config.h"

#define LEN(a)    (sizeof(a) / sizeof((a)[0]))
#define BODY_SIZE 256

/* Not every battery reports capacity steps as uevents. */
#ifndef BATTERY_POLL_MS
#define BATTERY_POLL_MS 60000
#endif


#if defined(POWERPROFILE) || defined(GPU_PROFILE)
/* Appends "<label><value>" to 'body' on a new line. */
static void
add_line(char *body, const size_t bodysz, const char *label, const char *value)
{
	const size_t len = strlen(body);
	const int    n   = snprintf(body + len, bodysz - len, "\n%-18s%s", label, value);

	if (n < 0 || (size_t)n >= bodysz - len)
		warn("notification body truncated");
}
#endif

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

#ifdef POWERPROFILE
	add_line(body, sizeof(body), "Power profile:",
	         powerprofile_state_label(powerprofile_state()));
#endif

#ifdef GPU_PROFILE
	add_line(body, sizeof(body), "GPU in use:",
	         gpuprofile_state_label(gpuprofile_state()));
#endif

	notify("Power", body, icon_battery_notif);
}

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

static void
show(struct Status *s)
{
	/* Colour per icon index, parallel to icons_battery in config.h. */
	static const char *const icon_colors[] = {
		"bat_crt", "bat_low", "bat_nrm",
		"bat_nrm", "bat_nrm", "bat_chg"
	};

	static char base[PATH_MAX];
	static char st[64];
	char        text[16];
	size_t      i;
	int         charging;

	if (getbatterypath(base, sizeof(base)) != 0) {
		/*
		 * No battery is a normal state on a desktop, so render the
		 * empty state rather than leaving the bar blank.
		 */
		emit(icon_colors[0], icons_battery[0], ascii_bat_none);
		s->st = NULL;
		return;
	}

	s->cap = getcapacity(base);
	getstatus(base, st, sizeof(st));
	s->st = st;

	charging = (strcmp(st, "Charging") == 0);
	i = charging ? 5 : battery_icon_index(s->cap);
	if (i >= LEN(icons_battery))
		i = 0;

	ascii_format(text, sizeof(text), s->cap, charging);
	emit(icon_colors[i], icons_battery[i], text);
}

/* Kernel uevents on the netlink socket; only power_supply ones matter. */
static int
ispowersupply(const char *msg, size_t len)
{
	for (size_t i = 0; i < len; i += strlen(msg + i) + 1)
		if (strcmp(msg + i, "SUBSYSTEM=power_supply") == 0)
			return 1;
	return 0;
}

static int
openuevents(void)
{
	struct sockaddr_nl sa = { .nl_family = AF_NETLINK, .nl_groups = 1 };
	int                fd;

	fd = socket(AF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC, NETLINK_KOBJECT_UEVENT);
	if (fd < 0 || bind(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		warn("uevent socket:");
		if (fd >= 0)
			close(fd);
		return -1;
	}
	return fd;
}

static int
watch(void)
{
	struct Status s;
	struct pollfd pfd = { .events = POLLIN };
	char          msg[4096];

	/* Without uevents the poll timeout alone keeps the block current. */
	pfd.fd = openuevents();

	for (;;) {
		show(&s);

		for (;;) {
			int n = poll(&pfd, 1, BATTERY_POLL_MS);
			ssize_t len;

			if (n < 0 && errno != EINTR)
				return 1;
			if (n <= 0)
				break;

			len = recv(pfd.fd, msg, sizeof(msg) - 1, 0);
			if (len <= 0)
				continue;
			msg[len] = '\0';
			if (ispowersupply(msg, (size_t)len))
				break;
		}
	}
}

int
main(void)
{
	static const struct Button buttons[] = {
		{ 1, on_left },
	};

	struct Status s;

	set_name("statusblocks-battery");

	if (persistent())
		return watch();

	show(&s);
	if (s.st)
		dispatch(buttons, LEN(buttons), &s);

	return 0;
}
