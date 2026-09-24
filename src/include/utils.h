/* See LICENSE file for copyright and license details. */

#ifndef UTILS_H
#define UTILS_H

#include <limits.h>
#include <stddef.h>
#include <sys/types.h>

/*
 * POSIX allows PATH_MAX to be absent when the limit is indeterminate.
 * Every path buffer here is bounded and checked, so a fixed cap is safe.
 */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

void set_name(const char *name);
const char *get_name(void);

/*
 * Prints formated message to stderr and exits.
 * If last char is ':', prints strerror with set errno.
 */
_Noreturn void die(const char *fmt, ...);

/*
 * Prints formated message to stderr and returns.
 * If last char is ':', prints strerror with set errno.
 */
void warn(const char *fmt, ...);

void *ecalloc(const size_t nmemb, const size_t size);
void *emalloc(const size_t size);
void *erealloc(void *ptr, const size_t size);

/* Sends a single desktop notification with normal urgency. */
void notify(const char *sum, const char *body, const char *icon);

/* One entry of a block's BLOCK_BUTTON dispatch table. */
struct Button {
	int   button;
	void (*handler)(void *ctx);
};

/*
 * Reads BLOCK_BUTTON and runs the matching handler, passing 'ctx' through.
 * Does nothing if the variable is unset, empty, or matches no entry.
 */
void dispatch(const struct Button *buttons, const size_t n, void *ctx);

/* Returns 1 if 's' is exactly "#RRGGBB". */
int ishexcolor(const char *s);

/*
 * Forks and execs 'args' in a new session, searching PATH for args[0].
 * The child never returns.
 */
void execute(char **args);

/*
 * As execute(), but runs the exact binary at 'path'.
 */
void executepath(const char *path, char **args);

/*
 * As execute(), but replaces args[0] with $TERM or $TERMINAL when either
 * names a real executable, falling back to the configured terminal.
 * Use for any command whose first element is term_cmd.
 */
void execute_term(char **args);

/*
 * Expands a path into 'out'. A leading "~" or "~/" becomes $HOME, and
 * "$VAR" or "${VAR}" anywhere in the string is replaced by that variable.
 * A '$' not followed by a name is copied through literally.
 * Returns 0 on success, 1 if a variable was unset or 'out' was too small.
 */
int envexpand(const char *path, char *out, const size_t outsz);

/*
 * Presents 'menu' via xmenu(1) and returns the selected option's value,
 * or -1 if nothing was selected or xmenu could not be run.
 */
int getxmenuopt(const char *menu);

/*
 * Returns the pid of the process whose /proc/<pid>/cmdline starts with
 * 'process', or -1 if no such process exists.
 */
pid_t getpidof(const char *process);

/*
 * Runs 'cmd' through the shell and copies the first line of its output
 * containing 'key' into 'buf'.
 *
 * Returns 0 on success, -1 if the command could not be run or no line
 * contained the key.
 */
int cmdfindline(const char *cmd, const char *key, char *buf, const size_t bufsz);

#endif /* UTILS_H */
