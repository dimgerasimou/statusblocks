/* See LICENSE file for copyright and license details. */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

#define UPDATES_C
#define BUF_SIZE   64
#define BODY_SIZE  128
#define CACHE_NAME "statusblocks-updates"
#define LEN(a)     (sizeof(a) / sizeof((a)[0]))

/* A transaction touches the database many times; recount once it settles. */
#define SETTLE_MS  2000

#include "utils.h"
#include "config.h"

/* Pending update counts, one per configured source. */
struct Updates {
	int primary;
	int secondary;
};

/*
 * Counts the lines produced by 'cmd', or 0 if 'cmd' is empty. These
 * commands typically query the network, which is why the result is cached
 * rather than recomputed on every refresh.
 */
static int
getupdates(const char *cmd)
{
	FILE *ep;
	char  buffer[BUF_SIZE];
	int   counter = 0;

	if (!cmd || !*cmd)
		return 0;

	ep = popen(cmd, "r");
	if (!ep) {
		warn("popen() for: %s", cmd);
		return -1;
	}

	while (fgets(buffer, sizeof(buffer), ep))
		counter++;

	/*
	 * The exit status is deliberately ignored: checkupdates exits 2 and
	 * paru exits 1 when there is simply nothing to update, and other
	 * package managers behave similarly, so a non-zero status is the
	 * normal case rather than a failure.
	 */
	pclose(ep);

	return counter;
}

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
 * The cache is stale once the TTL has passed, or as soon as the watched
 * package-manager path is newer than it. The second test is what makes a
 * post-transaction hook work: the block cannot observe the signal that
 * dwmblocks sends (it is simply re-executed), but installing or removing
 * a package always rewrites that path, so the count refreshes on the very
 * next run instead of waiting out the TTL.
 */
static int
cachestale(const struct stat *cst)
{
	struct stat dbst;

	if (time(NULL) - cst->st_mtime > update_cache_ttl)
		return 1;

	if (update_watch_path[0] && stat(update_watch_path, &dbst) == 0 &&
	    dbst.st_mtime > cst->st_mtime)
		return 1;

	return 0;
}

/* Returns 0 and fills 'u' if a usable, fresh cache entry exists. */
static int
cacheload(const char *path, struct Updates *u)
{
	struct stat st;
	FILE       *fp;
	int         ok;

	/* A non-positive TTL disables caching entirely. */
	if (update_cache_ttl <= 0)
		return 1;

	if (stat(path, &st) != 0)
		return 1;

	if (cachestale(&st))
		return 1;

	fp = fopen(path, "r");
	if (!fp)
		return 1;

	ok = (fscanf(fp, "%d %d", &u->primary, &u->secondary) == 2);
	fclose(fp);

	return ok ? 0 : 1;
}

/* Written via a temporary file so concurrent blocks never see a partial cache. */
static void
cachestore(const char *path, const struct Updates *u)
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

	fprintf(fp, "%d %d\n", u->primary, u->secondary);

	if (fclose(fp) != 0 || rename(tmp, path) != 0)
		unlink(tmp);
}

/* Runs the update commands and refreshes the cache. */
static void
refresh(struct Updates *u)
{
	char path[PATH_MAX];

	u->primary   = getupdates(cmd_updates_primary);
	u->secondary = getupdates(cmd_updates_secondary);

	if (u->primary < 0 || u->secondary < 0)
		return;

	if (update_cache_ttl > 0 && cachepath(path, sizeof(path)) == 0)
		cachestore(path, u);
}

/*
 * Returns the update counts, preferring a fresh cache entry. Negative
 * counts mean the query failed and nothing should be displayed.
 */
static void
getcounts(struct Updates *u)
{
	char path[PATH_MAX];

	if (cachepath(path, sizeof(path)) == 0 && cacheload(path, u) == 0)
		return;

	refresh(u);
}

static void
on_left(void *ctx)
{
	struct Updates *u = ctx;
	char            body[BODY_SIZE];
	int             n;

	/* An explicit click is a request for current numbers, so bypass the cache. */
	refresh(u);

	if (u->primary < 0 || u->secondary < 0) {
		notify("Packages", "Failed to query package updates.", "tux");
		return;
	}

	n = snprintf(body, sizeof(body),
	             "%s %s: %d\n"
	             "%s %s: %d",
	             icon_updates_primary, label_updates_primary, u->primary,
	             icon_updates_secondary, label_updates_secondary, u->secondary);

	if (n < 0 || (size_t)n >= sizeof(body))
		warn("notification body truncated");

	notify("Packages", body, "tux");
}

static void
on_right(void *ctx)
{
	(void)ctx;
	execute_term((char **)args_update_cmd);
}

static void
show(const struct Updates *u)
{
	char rich[64] = "";
	char plain[64] = "";

	if (u->primary > 0 || u->secondary > 0) {
		int total = u->primary + u->secondary;

		if (show_update_count) {
			snprintf(rich, sizeof(rich), "%s %d", icon_updates, total);
			snprintf(plain, sizeof(plain), "%s%d", ascii_updates, total);
		} else {
			snprintf(rich, sizeof(rich), "%s", icon_updates);
			snprintf(plain, sizeof(plain), "%s", ascii_updates);
		}
	}

	emit("sys_pkg", rich, plain);
}

/*
 * Recounts when the package database settles after a change and every
 * update_cache_ttl seconds; CLOCK_BOOTTIME so time suspended counts. The
 * cache file is watched too, so a click's refresh reaches the bar.
 */
static int
watch(void)
{
	const long        period = update_cache_ttl > 0 ? update_cache_ttl : 3600;
	struct itimerspec its = { { period, 0 }, { period, 0 } };
	struct Updates    u = { -1, -1 };
	struct pollfd     pfd[2] = { { .events = POLLIN }, { .events = POLLIN } };
	char              cache[PATH_MAX];
	_Alignas(struct inotify_event) char buf[4096];
	const char       *cachename = NULL;
	int               dbwd = -1, cachewd = -1, settling = 0;

	pfd[0].fd = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
	pfd[1].fd = timerfd_create(CLOCK_BOOTTIME, TFD_CLOEXEC | TFD_NONBLOCK);
	if (pfd[0].fd < 0 || pfd[1].fd < 0) {
		warn("inotify/timerfd:");
		return 1;
	}
	timerfd_settime(pfd[1].fd, 0, &its, NULL);

	if (update_watch_path[0]) {
		dbwd = inotify_add_watch(pfd[0].fd, update_watch_path,
		                         IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
		if (dbwd < 0)
			warn("inotify_add_watch(%s):", update_watch_path);
	}

	if (update_cache_ttl > 0 && cachepath(cache, sizeof(cache)) == 0) {
		char *slash = strrchr(cache, '/');

		*slash = '\0';
		cachename = slash + 1;
		cachewd = inotify_add_watch(pfd[0].fd, cache, IN_MOVED_TO);
		*slash = '/';
	}

	getcounts(&u);
	show(&u);

	for (;;) {
		int     n = poll(pfd, LEN(pfd), settling ? SETTLE_MS : -1);
		ssize_t len;

		if (n < 0) {
			if (errno == EINTR)
				continue;
			warn("poll:");
			return 1;
		}

		if (n == 0 || (pfd[1].revents & POLLIN)) {
			uint64_t expirations;

			if (read(pfd[1].fd, &expirations, sizeof(expirations)) < 0 && errno != EAGAIN)
				warn("timerfd read:");
			settling = 0;
			refresh(&u);
			show(&u);
			continue;
		}

		while ((len = read(pfd[0].fd, buf, sizeof(buf))) > 0) {
			for (char *p = buf; p < buf + len;) {
				const struct inotify_event *ev = (const struct inotify_event *)p;

				if (ev->wd == dbwd) {
					settling = 1;
				} else if (ev->wd == cachewd && ev->len && !strcmp(ev->name, cachename)
				           && cacheload(cache, &u) == 0) {
					show(&u);
				}
				p += sizeof(*ev) + ev->len;
			}
		}
	}
}

int
main(void)
{
	static const struct Button buttons[] = {
		{ 1, on_left },
		{ 3, on_right },
	};

	struct Updates u = { -1, -1 };

	set_name("statusblocks-updates");

	if (persistent())
		return watch();

	dispatch(buttons, LEN(buttons), &u);

	if (u.primary < 0 || u.secondary < 0)
		getcounts(&u);

	show(&u);

	return 0;
}
