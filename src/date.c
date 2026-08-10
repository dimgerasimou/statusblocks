/* See LICENSE file for copyright and license details. */

#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define DATE_C
#define HEAD_SIZE 64
#define CAL_SIZE  2048
#define LEN(a)    (sizeof(a) / sizeof((a)[0]))
#define CAL_WIDTH 20
#define CAL_DAY_WIDTH 2

#include "colors.h"
#include "toggle.h"
#include "utils.h"
#include "config.h"

/* Fallbacks used when the locale gives nothing usable. */
static const char *const months_en[] = {
	"January", "February", "March",     "April",
	"May",     "June",     "July",      "August",
	"September", "October", "November", "December"
};

static const char *const days_en[] = {
	"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"
};

static const int days_in_month[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

/*
 * Counts characters rather than bytes, so that centring and truncation
 * stay correct for the accented month names many locales produce.
 * Continuation bytes (10xxxxxx) do not start a character.
 */
static size_t
utf8len(const char *s)
{
	size_t n = 0;

	for (; *s; s++) {
		if ((*s & 0xC0) != 0x80)
			n++;
	}

	return n;
}

/* Copies the first 'want' characters of 's' into 'out', padding to width. */
static void
utf8trunc(const char *s, size_t want, char *out, size_t outsz)
{
	size_t chars = 0;
	size_t i = 0;

	while (s[i] && chars < want) {
		size_t start = i;

		i++;
		while ((s[i] & 0xC0) == 0x80)
			i++;

		if (i - start >= outsz - strlen(out))
			break;

		chars++;
	}

	if (i >= outsz)
		i = outsz - 1;

	memcpy(out, s, i);
	out[i] = '\0';

	/* Short abbreviations are padded so the columns still line up. */
	for (size_t p = chars; p < want && strlen(out) + 1 < outsz; p++)
		strcat(out, " ");
}

static int
cal_firstday(int mday, int wday, const int weekstart)
{
	while (mday > 7)
		mday -= 7;

	while (mday > 1) {
		mday--;
		wday--;
		if (wday == -1)
			wday = 6;
	}

	/* Shift from Sunday-first (tm_wday) to the configured first day. */
	return ((wday - weekstart) % 7 + 7) % 7;
}

static int
cal_monthdays(const int m, const int y)
{
	if (m < 0 || m > 11)
		return 0;

	if (m != 1)
		return days_in_month[m];

	if ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)
		return 29;

	return 28;
}

static const char *
cal_monthname(const int m)
{
	static char buf[64];
	struct tm   tm = {0};

	if (m < 0 || m > 11)
		return NULL;

	/*
	 * %B is the locale's full month name. Fall back to English if the
	 * locale supplies nothing, which happens under the default "C" locale
	 * only for exotic builds.
	 */
	tm.tm_mon  = m;
	tm.tm_mday = 1;
	tm.tm_year = 100;

	if (strftime(buf, sizeof(buf), "%B", &tm) == 0 || buf[0] == '\0')
		return months_en[m];

	return buf;
}

static const char *
cal_dayname(const int wday)
{
	static char buf[16];
	struct tm   tm = {0};
	char        full[64];

	if (wday < 0 || wday > 6)
		return NULL;

	/* 2023-01-01 was a Sunday, so tm_mday 1 + wday lands on the right day. */
	tm.tm_year = 123;
	tm.tm_mon  = 0;
	tm.tm_mday = 1 + wday;
	tm.tm_hour = 12;
	mktime(&tm);

	if (strftime(full, sizeof(full), "%a", &tm) == 0 || full[0] == '\0')
		return days_en[wday];

	buf[0] = '\0';
	utf8trunc(full, CAL_DAY_WIDTH, buf, sizeof(buf));

	return buf;
}

static int
cal_render(char *buf, const size_t bufsz, const char *accent,
           const int mday, const int wday, const int m, const int y,
           const int weekstart)
{
	size_t off   = 0;
	int    fday  = 0;
	int    daysm = 0;
	int    n     = 0;

	if (!buf || bufsz == 0 || weekstart < 0 || weekstart > 6)
		return 1;

	daysm = cal_monthdays(m, y);
	if (daysm == 0)
		return 1;

	/* An unset colour renders the calendar without markup. */
	if (accent && !*accent)
		accent = NULL;

	fday = cal_firstday(mday, wday, weekstart);

	/* Header: day abbreviations, weekends accented. */
	for (int i = 0; i < 7; i++) {
		int         day = (weekstart + i) % 7;
		int         weekend = (day == 0 || day == 6);
		const char *name = cal_dayname(day);

		if (accent && weekend)
			n = snprintf(buf + off, bufsz - off, "<span color='%s'>%s</span>%s",
			             accent, name, i == 6 ? "" : " ");
		else
			n = snprintf(buf + off, bufsz - off, "%s%s",
			             name, i == 6 ? "" : " ");

		if (n < 0 || (size_t)n >= bufsz - off)
			return 1;
		off += (size_t)n;
	}

	n = snprintf(buf + off, bufsz - off, "\n");
	if (n < 0 || (size_t)n >= bufsz - off)
		return 1;
	off += (size_t)n;

	for (int i = 0; i < fday; i++) {
		n = snprintf(buf + off, bufsz - off, "   ");
		if (n < 0 || (size_t)n >= bufsz - off)
			return 1;
		off += (size_t)n;
	}

	for (int i = 1; i <= daysm; i++) {
		int day = (weekstart + fday) % 7;

		if (fday == 7) {
			fday = 0;
			day  = weekstart;

			n = snprintf(buf + off, bufsz - off, "\n");
			if (n < 0 || (size_t)n >= bufsz - off)
				return 1;
			off += (size_t)n;
		}

		if (i == mday && accent)
			n = snprintf(buf + off, bufsz - off,
			             "<span color='black' bgcolor='%s'>%2d</span> ",
			             accent, i);
		else if (i == mday)
			n = snprintf(buf + off, bufsz - off, "<b>%2d</b> ", i);
		else if (accent && (day == 0 || day == 6))
			n = snprintf(buf + off, bufsz - off,
			             "<span color='%s'>%2d</span> ", accent, i);
		else
			n = snprintf(buf + off, bufsz - off, "%2d ", i);

		if (n < 0 || (size_t)n >= bufsz - off)
			return 1;
		off += (size_t)n;

		fday++;
	}

	return 0;
}

static int
cal_heading(char *buf, const size_t bufsz, const int m, const int y)
{
	const char *name = cal_monthname(m);
	char        year[16];
	size_t      len, pad;
	int         n;

	if (!buf || bufsz == 0 || !name)
		return 1;

	n = snprintf(year, sizeof(year), "%d", y);
	if (n < 0 || (size_t)n >= sizeof(year))
		return 1;

	/*
	 * Centre "<Month> <year>" over a CAL_WIDTH body. Character counts are
	 * used rather than byte counts so that accented month names, which
	 * several locales produce, still line up.
	 */
	len = utf8len(name) + 1 + strlen(year);
	pad = (len < CAL_WIDTH) ? (CAL_WIDTH - len) / 2 : 0;

	if (pad + strlen(name) + 1 + strlen(year) >= bufsz)
		return 1;

	memset(buf, ' ', pad);
	n = snprintf(buf + pad, bufsz - pad, "%s %s", name, year);

	if (n < 0 || (size_t)n >= bufsz - pad)
		return 1;

	return 0;
}

static void
on_left(void *ctx)
{
	struct tm *lt = NULL;
	char       body[CAL_SIZE];
	char       head[HEAD_SIZE];
	time_t     ct = 0;

	(void)ctx;

	ct = time(NULL);
	lt = localtime(&ct);
	if (!lt) {
		warn("localtime:");
		return;
	}

	if (cal_render(body, sizeof(body), clr_hex(clr_cal), lt->tm_mday,
	               lt->tm_wday, lt->tm_mon, lt->tm_year + 1900,
	               calendar_week_start) != 0 ||
	    cal_heading(head, sizeof(head), lt->tm_mon, lt->tm_year + 1900) != 0) {
		warn("failed to render calendar");
		return;
	}

	notify(head, body, "calendar");
}

int
main(void)
{
	static const struct Button buttons[] = {
		{ 1, on_left },
	};

	struct tm *lt = NULL;
	time_t     ct = 0;

	set_name("statusblocks-date");

	/* Month and weekday names in the calendar follow the user's locale. */
	setlocale(LC_TIME, "");

	clr_init();
	toggle_init();

	dispatch(buttons, LEN(buttons), NULL);

	ct = time(NULL);
	lt = localtime(&ct);

	printf("%s", clr_get(clr_date));

	if (toggle_get(show_date))
		printf("%s", icon_date);

	if (lt)
		printf("%02d/%02d" CLR_NRM "\n", lt->tm_mday, lt->tm_mon + 1);
	else {
		warn("localtime:");
		printf("--/--" CLR_NRM "\n");
	}

	return 0;
}
