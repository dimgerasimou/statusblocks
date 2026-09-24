/* See LICENSE file for copyright and license details. */

#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <libnotify/notify.h>

#include "utils.h"

static const char *program_name;

void
set_name(const char *name)
{
	program_name = name;
}

const char *
get_name(void)
{
	return program_name;
}

void
die(const char *fmt, ...)
{
	va_list ap;
	int saved_errno = errno;

	fprintf(stderr, "%s: ", program_name);

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (fmt[0] && fmt[strlen(fmt) - 1] == ':')
		fprintf(stderr, " %s", strerror(saved_errno));
	fputc('\n', stderr);

	exit(1);
}

void
warn(const char *fmt, ...)
{
	va_list ap;
	int saved_errno = errno;

	fprintf(stderr, "%s: ", program_name);

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (fmt[0] && fmt[strlen(fmt) - 1] == ':')
		fprintf(stderr, " %s", strerror(saved_errno));
	fputc('\n', stderr);
}

void *
ecalloc(const size_t nmemb, const size_t size)
{
	void *p;

	if ((p = calloc(nmemb, size)) == NULL)
		die("calloc:");
	return p;
}

void *
emalloc(const size_t size)
{
	void *p;

	if ((p = malloc(size)) == NULL)
		die("malloc:");
	return p;
}

void *
erealloc(void *ptr, const size_t size)
{
	void *p;

	if ((p = realloc(ptr, size)) == NULL)
		die("realloc:");
	return p;
}

void
notify(const char *sum, const char *body, const char *icon)
{
	NotifyNotification *n;

	notify_init(get_name());

	n = notify_notification_new(sum, body, icon);
	if (!n) {
		warn("notify_notification_new() failed");
		notify_uninit();
		return;
	}

	notify_notification_set_urgency(n, NOTIFY_URGENCY_NORMAL);
	notify_notification_show(n, NULL);

	g_object_unref(G_OBJECT(n));
	notify_uninit();
}

/*
 * Points a freshly forked child's stdout at /dev/null.
 *
 * A block's stdout is the status bar: anything a spawned program prints
 * would be read as bar text, so a chatty helper corrupts the bar rather
 * than merely being noisy. stderr is deliberately left alone, so the
 * exec warnings below and whatever the program complains about still
 * reach the journal.
 */
static void
silence_stdout(void)
{
	int fd = open("/dev/null", O_WRONLY);

	if (fd < 0) {
		warn("open /dev/null:");
		return;
	}

	if (dup2(fd, STDOUT_FILENO) < 0)
		warn("dup2 for stdout:");

	if (fd != STDOUT_FILENO)
		close(fd);
}

void
execute(char **args)
{
	if (!args || !args[0])
		die("execute: empty argument vector");

	switch (fork()) {
	case -1:
		die("fork:");

	case 0:
		setsid();
		silence_stdout();
		execvp(args[0], args);
		warn("execvp for: %s:", args[0]);
		_exit(127);

	default:
		break;
	}
}

/* Returns 1 if 'cmd' names an executable, searching $PATH when unqualified. */
static int
isexec(const char *cmd)
{
	char        buf[PATH_MAX];
	const char *path, *p;

	if (!cmd || !*cmd)
		return 0;

	if (strchr(cmd, '/'))
		return access(cmd, X_OK) == 0;

	path = getenv("PATH");
	if (!path || !*path)
		path = "/usr/local/bin:/usr/bin:/bin";

	for (p = path; *p;) {
		const char *end = strchr(p, ':');
		size_t      len = end ? (size_t)(end - p) : strlen(p);
		int         n;

		n = snprintf(buf, sizeof(buf), "%.*s/%s", (int)len, p, cmd);

		if (n > 0 && (size_t)n < sizeof(buf) && access(buf, X_OK) == 0)
			return 1;

		if (!end)
			break;
		p = end + 1;
	}

	return 0;
}

void
execute_term(char **args)
{
	static const char *const vars[] = { "TERM", "TERMINAL" };

	if (!args || !args[0])
		die("execute_term: empty argument vector");

	/*
	 * $TERM usually holds a terminfo entry name such as "st-256color"
	 * rather than a program, so each candidate is only taken when it
	 * actually resolves to an executable. Otherwise the configured
	 * terminal is used.
	 */
	for (size_t i = 0; i < sizeof(vars) / sizeof(vars[0]); i++) {
		const char *v = getenv(vars[i]);

		if (isexec(v)) {
			args[0] = (char *)v;
			break;
		}
	}

	execute(args);
}

void
executepath(const char *path, char **args)
{
	if (!path || !*path || !args || !args[0])
		die("executepath: empty path or argument vector");

	switch (fork()) {
	case -1:
		die("fork:");

	case 0:
		setsid();
		silence_stdout();
		execv(path, args);
		warn("execv for: %s:", path);
		_exit(127);

	default:
		break;
	}
}

/* Appends 'len' bytes of 's' to 'out'. Returns 1 if it would not fit. */
static int
strappend(char *out, const size_t outsz, size_t *off, const char *s,
          const size_t len)
{
	if (*off >= outsz || len >= outsz - *off)
		return 1;

	memcpy(out + *off, s, len);
	*off += len;
	out[*off] = '\0';

	return 0;
}

/* Returns the length of the variable name at 's', or 0 if there is none. */
static size_t
varnamelen(const char *s)
{
	size_t n = 0;

	while (s[n] == '_' || (s[n] >= 'a' && s[n] <= 'z') ||
	       (s[n] >= 'A' && s[n] <= 'Z') || (n > 0 && s[n] >= '0' && s[n] <= '9'))
		n++;

	return n;
}

/*
 * Looks up the variable named by the 'len' bytes at 'name'. Returns NULL
 * and warns if it is unset or the name is unreasonably long.
 */
static const char *
lookupvar(const char *name, const size_t len)
{
	char        buf[128];
	const char *env;

	if (len == 0 || len >= sizeof(buf))
		return NULL;

	memcpy(buf, name, len);
	buf[len] = '\0';

	env = getenv(buf);
	if (!env)
		warn("getenv() for: %s", buf);

	return env;
}

int
envexpand(const char *path, char *out, const size_t outsz)
{
	const char *p = path;
	size_t      off = 0;

	if (!path || !out || outsz == 0)
		return 1;

	out[0] = '\0';

	/* A leading "~" or "~/" is shorthand for $HOME. */
	if (p[0] == '~' && (p[1] == '\0' || p[1] == '/')) {
		const char *home = getenv("HOME");

		if (!home) {
			warn("getenv() for: HOME");
			return 1;
		}

		if (strappend(out, outsz, &off, home, strlen(home)) != 0)
			goto toolong;

		p++;
	}

	while (*p) {
		const char *name, *val;
		size_t      len;

		if (*p != '$') {
			const char *next = strchr(p, '$');
			size_t      run  = next ? (size_t)(next - p) : strlen(p);

			if (strappend(out, outsz, &off, p, run) != 0)
				goto toolong;

			p += run;
			continue;
		}

		/* "${VAR}" */
		if (p[1] == '{') {
			const char *close = strchr(p + 2, '}');

			if (!close) {
				warn("unterminated ${ in path: %s", path);
				return 1;
			}

			name = p + 2;
			len  = (size_t)(close - name);
			val  = lookupvar(name, len);

			if (!val)
				return 1;

			p = close + 1;
		} else {
			name = p + 1;
			len  = varnamelen(name);

			/* A bare '$' is not a variable; copy it through. */
			if (len == 0) {
				if (strappend(out, outsz, &off, p, 1) != 0)
					goto toolong;
				p++;
				continue;
			}

			val = lookupvar(name, len);

			if (!val)
				return 1;

			p = name + len;
		}

		if (strappend(out, outsz, &off, val, strlen(val)) != 0)
			goto toolong;
	}

	return 0;

toolong:
	warn("envexpand: expanded path too long: %s", path);
	return 1;
}

int
getxmenuopt(const char *menu)
{
	char   buf[16];
	int    towrite[2], toread[2];
	int    option = -1;
	pid_t  pid;
	size_t len;

	if (!menu || !*menu)
		return -1;

	if (pipe(towrite) < 0)
		die("pipe:");

	if (pipe(toread) < 0)
		die("pipe:");

	switch ((pid = fork())) {
	case -1:
		die("fork:");

	case 0:
		close(towrite[1]);
		close(toread[0]);

		if (dup2(towrite[0], STDIN_FILENO) < 0 ||
		    dup2(toread[1], STDOUT_FILENO) < 0)
			_exit(127);

		close(towrite[0]);
		close(toread[1]);

		execlp("xmenu", "xmenu", (char *)NULL);
		_exit(127);

	default:
		break;
	}

	close(towrite[0]);
	close(toread[1]);

	len = strlen(menu);
	if (write(towrite[1], menu, len) != (ssize_t)len)
		warn("write() to xmenu:");
	close(towrite[1]);

	{
		ssize_t got = read(toread[0], buf, sizeof(buf) - 1);

		if (got > 0) {
			buf[got] = '\0';
			if (sscanf(buf, "%d", &option) != 1)
				option = -1;
		}
	}

	close(toread[0]);

	while (waitpid(pid, NULL, 0) < 0 && errno == EINTR)
		;

	return option;
}

pid_t
getpidof(const char *process)
{
	char           buf[PATH_MAX];
	struct dirent *ent;
	DIR           *dir;
	pid_t          ret = -1;

	if (!process || !*process)
		return -1;

	dir = opendir("/proc");
	if (!dir) {
		warn("opendir() for \"/proc\":");
		return -1;
	}

	while ((ent = readdir(dir))) {
		FILE *fp;
		char *end;
		long  pid;

		pid = strtol(ent->d_name, &end, 10);
		if (end == ent->d_name || *end != '\0' || pid <= 0)
			continue;

		snprintf(buf, sizeof(buf), "/proc/%s/cmdline", ent->d_name);

		fp = fopen(buf, "r");
		if (!fp)
			continue;

		buf[0] = '\0';
		if (fgets(buf, sizeof(buf), fp) && strcmp(buf, process) == 0) {
			fclose(fp);
			ret = (pid_t)pid;
			break;
		}

		fclose(fp);
	}

	closedir(dir);

	if (ret < 0)
		errno = ESRCH;

	return ret;
}

void
dispatch(const struct Button *buttons, const size_t n, void *ctx)
{
	const char *env;
	int         want;

	if (!buttons || n == 0)
		return;

	env = getenv("BLOCK_BUTTON");
	if (!env || !*env)
		return;

	want = atoi(env);

	for (size_t i = 0; i < n; i++) {
		if (buttons[i].button == want) {
			if (buttons[i].handler)
				buttons[i].handler(ctx);
			return;
		}
	}
}

static int
ishexdigit(char c)
{
	if (c >= 'a' && c <= 'f')
		return 1;
	if (c >= 'A' && c <= 'F')
		return 1;
	if (c >= '0' && c <= '9')
		return 1;
	return 0;
}

int
ishexcolor(const char *s)
{
	if (!s || strlen(s) != 7 || s[0] != '#')
		return 0;

	for (size_t i = 1; i < 7; i++) {
		if (!ishexdigit(s[i]))
			return 0;
	}

	return 1;
}

int
cmdfindline(const char *cmd, const char *key, char *buf, const size_t bufsz)
{
	char  line[256];
	FILE *ep;
	int   found = 0;

	if (!(ep = popen(cmd, "r"))) {
		warn("popen() for \"%s\":", cmd);
		return -1;
	}

	while (fgets(line, sizeof(line), ep)) {
		if (strstr(line, key)) {
			snprintf(buf, bufsz, "%s", line);
			found = 1;
			break;
		}
	}

	pclose(ep);

	return found ? 0 : -1;
}
