/* See LICENSE file for copyright and license details. */

#define _POSIX_C_SOURCE 200809L

#include <pulse/pulseaudio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VOLUME_C
#define LEN(a) (sizeof(a) / sizeof((a)[0]))

#include "utils.h"
#include "config.h"

typedef struct {
	unsigned int volume;
	unsigned int mute;
	unsigned int done;
} AudioInfo;

/* Indices into the AudioInfo array; AI_COUNT is its length. */
enum AudioIndex {
	AI_SINK = 0,
	AI_SOURCE,
	AI_COUNT
};

enum { OP_MUTE, OP_UP, OP_DOWN };


static pa_mainloop *ml = NULL;
static pa_context *pactx = NULL;
static pa_mainloop_api *mlapi = NULL;
static int persist = 0;

static void contextcb(pa_context *c, void *userdata);
static void freepa(void);
static void getaudioinfo(AudioInfo *a);
static int  initpa(void);
static void propnotify(const AudioInfo *a);
static void servercb(pa_context *c, const pa_server_info *i, void *userdata);
static void sinkcb(pa_context *c, const pa_sink_info *i, int eol, void *userdata);
static void sourcecb(pa_context *c, const pa_source_info *i, int eol, void *userdata);
static void quitml(AudioInfo *a);
static void show(const AudioInfo *a);
static void subscribecb(pa_context *c, pa_subscription_event_type_t t, uint32_t idx, void *userdata);
static int  watch(AudioInfo *a);
static void changesink(int op);
static void changesinkcb(pa_context *c, const pa_sink_info *i, int eol, void *userdata);
static void changectxcb(pa_context *c, void *userdata);
static void donecb(pa_context *c, int success, void *userdata);

static void
changectxcb(pa_context *c, void *userdata)
{
	pa_operation *o;

	switch (pa_context_get_state(c)) {
	case PA_CONTEXT_READY:
		o = pa_context_get_sink_info_by_name(c, "@DEFAULT_SINK@", changesinkcb, userdata);
		if (o)
			pa_operation_unref(o);
		else
			pa_mainloop_quit(ml, 1);
		break;

	case PA_CONTEXT_FAILED:
	case PA_CONTEXT_TERMINATED:
		pa_mainloop_quit(ml, 1);
		break;

	default:
		break;
	}
}

static void
changesink(int op)
{
	if (initpa() != 0) {
		freepa();
		return;
	}

	pa_context_set_state_callback(pactx, changectxcb, &op);
	if (pa_context_connect(pactx, NULL, PA_CONTEXT_NOFLAGS, NULL) < 0)
		warn("pa_context_connect() failed");
	else
		pa_mainloop_run(ml, NULL);

	freepa();
}

static void
changesinkcb(pa_context *c, const pa_sink_info *i, int eol, void *userdata)
{
	const pa_volume_t step = (pa_volume_t)((uint64_t)PA_VOLUME_NORM * volume_step / 100);
	const pa_volume_t max  = (pa_volume_t)((uint64_t)PA_VOLUME_NORM * volume_max / 100);
	pa_operation     *o = NULL;
	pa_cvolume        cv;

	if (eol > 0)
		return;

	if (!i) {
		warn("no default sink");
		pa_mainloop_quit(ml, 1);
		return;
	}

	cv = i->volume;

	switch (*(int *)userdata) {
	case OP_MUTE:
		o = pa_context_set_sink_mute_by_index(c, i->index, !i->mute, donecb, NULL);
		break;
	case OP_UP:
		if (pa_cvolume_max(&cv) < max)
			pa_cvolume_inc_clamp(&cv, step, max);
		o = pa_context_set_sink_volume_by_index(c, i->index, &cv, donecb, NULL);
		break;
	case OP_DOWN:
		pa_cvolume_dec(&cv, step);
		o = pa_context_set_sink_volume_by_index(c, i->index, &cv, donecb, NULL);
		break;
	}

	if (o)
		pa_operation_unref(o);
	else
		pa_mainloop_quit(ml, 1);
}

static void
contextcb(pa_context *c, void *userdata)
{
	pa_operation *o;

	switch (pa_context_get_state(c)) {
	case PA_CONTEXT_READY:
		if (persist) {
			pa_context_set_subscribe_callback(c, subscribecb, userdata);
			if ((o = pa_context_subscribe(c, PA_SUBSCRIPTION_MASK_SINK
			                              | PA_SUBSCRIPTION_MASK_SERVER, NULL, NULL)))
				pa_operation_unref(o);
		}
		if ((o = pa_context_get_server_info(c, servercb, userdata)))
			pa_operation_unref(o);
		break;

	case PA_CONTEXT_FAILED:
	case PA_CONTEXT_TERMINATED:
		pa_mainloop_quit(ml, 1);
		break;

	default:
		break;
	}
}

static void
on_left(void *ctx)
{
	propnotify(ctx);
}

static void
on_middle(void *ctx)
{
	(void)ctx;
	execute((char **)args_eqalizer);
}


static void
on_mute(void *ctx)
{
	changesink(OP_MUTE);
	getaudioinfo(ctx);
}

static void
on_up(void *ctx)
{
	changesink(OP_UP);
	getaudioinfo(ctx);
}

static void
on_down(void *ctx)
{
	changesink(OP_DOWN);
	getaudioinfo(ctx);
}

static void
donecb(pa_context *c, int success, void *userdata)
{
	(void)c;
	(void)userdata;

	if (!success)
		warn("changing the default sink failed");
	pa_mainloop_quit(ml, 0);
}

static void
freepa(void)
{
	if (pactx) {
		pa_context_disconnect(pactx);
		pa_context_unref(pactx);
		pactx = NULL;
	}
	if (ml) {
		pa_mainloop_free(ml);
		ml = NULL;
	}
}

static void
getaudioinfo(AudioInfo *a)
{
	if (initpa() != 0) {
		freepa();
		return;
	}

	pa_context_set_state_callback(pactx, contextcb, a);
	pa_context_connect(pactx, NULL, PA_CONTEXT_NOFLAGS, NULL);

	pa_mainloop_run(ml, NULL);

	freepa();
}

/* Returns 0 on success. PulseAudio may simply not be running. */
static int
initpa(void)
{
	if (!(ml = pa_mainloop_new())) {
		warn("pa_mainloop_new() failed to initialize");
		return 1;
	}

	if (!(mlapi = pa_mainloop_get_api(ml))) {
		warn("pa_mainloop_get_api() failed to initialize");
		return 1;
	}

	if (!(pactx = pa_context_new(mlapi, "statusblocks-volume"))) {
		warn("pa_context_new() failed to initialize");
		return 1;
	}

	return 0;
}

static void
propnotify(const AudioInfo *a)
{
	char   body[256];
	size_t off = 0;
	int    n;

	if (a[AI_SINK].done == 1)
		n = snprintf(body, sizeof(body), "%s Volume: %3u%%, Muted: %s\n",
		             icon_vol_sink, a[AI_SINK].volume, a[AI_SINK].mute ? "Yes" : "No");
	else
		n = snprintf(body, sizeof(body), "No audio sink detected.\n");

	if (n < 0 || (size_t)n >= sizeof(body)) {
		warn("notification body truncated");
		return;
	}
	off = (size_t)n;

	if (a[AI_SOURCE].done == 1)
		n = snprintf(body + off, sizeof(body) - off, "%s Volume: %3u%%, Muted: %s\n",
		             icon_vol_source, a[AI_SOURCE].volume, a[AI_SOURCE].mute ? "Yes" : "No");
	else
		n = snprintf(body + off, sizeof(body) - off, "No audio source detected.\n");

	if (n < 0 || (size_t)n >= sizeof(body) - off)
		warn("notification body truncated");

	notify("Audio Properties", body, "audio-headphones");
}

static void
servercb(pa_context *c, const pa_server_info *i, void *userdata)
{
	pa_operation *o;

	if (!i)
		return;

	if ((o = pa_context_get_sink_info_by_name(c, i->default_sink_name, sinkcb, userdata)))
		pa_operation_unref(o);
	if ((o = pa_context_get_source_info_by_name(c, i->default_source_name, sourcecb, userdata)))
		pa_operation_unref(o);
}

/* Converts a PulseAudio volume to a 0..100 percentage. */
static unsigned int
volpercent(const pa_cvolume *cv)
{
	double pct = (pa_cvolume_avg(cv) * 100.0) / PA_VOLUME_NORM;

	if (pct < 0.0)
		return 0;
	if (pct > 150.0)
		return 150;

	return (unsigned int)(pct + 0.5);
}

static void
sinkcb(pa_context *c, const pa_sink_info *i, int eol, void *userdata)
{
	AudioInfo *a = userdata;

	(void)c;

	if (eol > 0)
		return;

	if (!i) {
		a[AI_SINK].done = 2;
		quitml(a);
		return;
	}

	a[AI_SINK].volume = volpercent(&i->volume);
	a[AI_SINK].mute   = i->mute ? 1u : 0u;
	a[AI_SINK].done   = 1;

	quitml(a);
}

static void
sourcecb(pa_context *c, const pa_source_info *i, int eol, void *userdata)
{
	AudioInfo *a = userdata;

	(void)c;

	if (eol > 0)
		return;

	if (!i) {
		a[AI_SOURCE].done = 2;
		quitml(a);
		return;
	}

	a[AI_SOURCE].volume = volpercent(&i->volume);
	a[AI_SOURCE].mute   = i->mute ? 1u : 0u;
	a[AI_SOURCE].done   = 1;

	quitml(a);
}

static void
quitml(AudioInfo *a)
{
	if (!a[AI_SINK].done || !a[AI_SOURCE].done)
		return;

	if (!persist) {
		pa_mainloop_quit(ml, 0);
		return;
	}

	show(a);
	a[AI_SINK].done = a[AI_SOURCE].done = 0;
}

static void
show(const AudioInfo *a)
{
	char          v[16] = "";
	char          rich[64];
	char          plain[64];
	const char   *ricon = icons_volume[4];
	const char   *picon = "";
	unsigned int  volume;
	unsigned int  mute;
	int           pad = volume_padding ? 3 : 0;
	int           n;

	if (a[AI_SINK].done == 1) {
		mute   = a[AI_SINK].mute;
		volume = a[AI_SINK].volume;
	} else {
		mute   = 1;
		volume = 0;
	}

	if (display_type != 2) {
		picon = mute ? ascii_vol_mute_tag : ascii_vol_tag;

		if (mute)
			ricon = icons_volume[0];
		else if (volume > 66)
			ricon = icons_volume[3];
		else if (volume > 33)
			ricon = icons_volume[2];
		else
			ricon = icons_volume[1];
	}

	n = snprintf(v, sizeof(v), "%*u%%", pad, volume);
	if (n < 0 || (size_t)n >= sizeof(v))
		warn("volume string truncated");

	/*
	 * display_type == 1 hides the percentage, which a glyph can carry by
	 * shape; the ASCII tags cannot, so the plain text keeps it. While
	 * muted, ascii_vol_mute_tag alone already conveys the state.
	 */
	snprintf(rich, sizeof(rich), "%s%s", ricon, display_type != 1 ? v : "");
	snprintf(plain, sizeof(plain), "%s%s", picon, mute ? "" : v);
	emit(mute ? "vol_mut" : "vol_nrm", rich, plain);
}

static void
subscribecb(pa_context *c, pa_subscription_event_type_t t, uint32_t idx, void *userdata)
{
	pa_operation *o;

	(void)t;
	(void)idx;

	/* The default sink may have changed too, so start from the server. */
	if ((o = pa_context_get_server_info(c, servercb, userdata)))
		pa_operation_unref(o);
}

/* Returns only when the server connection is lost; dwmblocks restarts us. */
static int
watch(AudioInfo *a)
{
	int ret = 1;

	persist = 1;
	if (initpa() != 0) {
		freepa();
		return 1;
	}

	pa_context_set_state_callback(pactx, contextcb, a);
	if (pa_context_connect(pactx, NULL, PA_CONTEXT_NOFLAGS, NULL) < 0) {
		warn("pa_context_connect() failed");
		freepa();
		return 1;
	}

	pa_mainloop_run(ml, &ret);
	freepa();

	return ret ? ret : 1;
}

int
main(void)
{
	static const struct Button buttons[] = {
		{ 1, on_left },
		{ 2, on_middle },
		{ 3, on_mute },
		{ 4, on_up },
		{ 5, on_down },
	};

	AudioInfo a[AI_COUNT] = {{0, 0, 0}, {0, 0, 0}};

	set_name("statusblocks-volume");

	if (persistent())
		return watch(a);

	getaudioinfo(a);

	dispatch(buttons, LEN(buttons), a);

	show(a);

	return 0;
}
