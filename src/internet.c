/* See LICENSE file for copyright and license details. */

#define _POSIX_C_SOURCE 200809L

#include <limits.h>

#include <NetworkManager.h>
#include <nm-dbus-interface.h>
#include <stdio.h>
#include <string.h>

#define INTERNET_C
#define LEN(a) (sizeof(a) / sizeof((a)[0]))

#include "utils.h"
#include "config.h"


/* Colour per icon index, parallel to icons_internet in config.h. */
static const char *const icon_colors[] = {
	"net_err",
	"net_nrm",
	"net_nrm",
	"net_nrm",
	"net_nrm",
	"net_nrm",
	"net_nrm",
	"net_err"
};


/* function definitions */
static void comapp(NMDevice *dev, GString *str);
static void ethapp(NMDevice *dev, GString *str);
static unsigned int getactive(NMClient *c, NMDeviceWifi **wdev, NMAccessPoint **wap);
static void nminit(NMClient **c);
static void printinfo(NMClient *c, const int ind);
static void togglewifi(NMClient *client);
static void wifiapp(NMDevice *dev, GString *str);

static inline unsigned int
clampstate(unsigned int s)
{
	const unsigned int n = (unsigned int)LEN(icons_internet);
	if (s >= n)
		return 0;
	return s;
}

static void
comapp(NMDevice *dev, GString *str)
{
	NMActiveConnection *ac;
	const char *mac;

	ac = nm_device_get_active_connection(dev);
	if (!ac)
		return;

	mac = nm_device_get_hw_address(dev);
	if (!mac || !*mac)
		mac = "*unknown*";

	/* IPv4 */
	const char *ip4_addr = NULL;
	const char *ip4_gw   = NULL;
	NMIPConfig *ip4_cfg  = nm_active_connection_get_ip4_config(ac);
	if (ip4_cfg) {
		GPtrArray *a = nm_ip_config_get_addresses(ip4_cfg);
		if (a && a->len >= 1) {
			NMIPAddress *ip = g_ptr_array_index(a, 0);
			if (ip)
				ip4_addr = nm_ip_address_get_address(ip);
		}
		ip4_gw = nm_ip_config_get_gateway(ip4_cfg);
	}

	/* IPv6 */
	const char *ip6_addr = NULL;
	NMIPConfig *ip6_cfg  = nm_active_connection_get_ip6_config(ac);
	if (ip6_cfg) {
		GPtrArray *a = nm_ip_config_get_addresses(ip6_cfg);
		if (a && a->len >= 1) {
			NMIPAddress *ip = g_ptr_array_index(a, 0);
			if (ip)
				ip6_addr = nm_ip_address_get_address(ip);
		}
	}

	if (!ip4_addr || !*ip4_addr) ip4_addr = "*none*";
	if (!ip4_gw   || !*ip4_gw)   ip4_gw   = "*none*";
	if (!ip6_addr || !*ip6_addr) ip6_addr = "*none*";

	g_string_append_printf(str,
		"MAC:  %s\n"
		"IPv4: %s\n"
		"Gate: %s\n"
		"IPv6: %s\n\n",
		mac, ip4_addr, ip4_gw, ip6_addr);
}

static void
ethapp(NMDevice *dev, GString *str)
{
	if (!nm_device_get_active_connection(dev))
		g_string_append(str, "Ethernet: Disconnected\n\n");
	else
		g_string_append(str, "Ethernet\n");
}

struct Context {
	NMClient *client;
	int       icind;
};

static void
on_left(void *ctx)
{
	const struct Context *c = ctx;

	printinfo(c->client, c->icind);
}

static void
on_right(void *ctx)
{
	const struct Context *c = ctx;

	switch (getxmenuopt(menu_internet)) {
	case 0:
		togglewifi(c->client);
		break;

	case 1: {
		char path[PATH_MAX];

		if (envexpand(path_wifi_connect, path, sizeof(path)) == 0)
			executepath(path, (char **)args_wifi_connect);
		break;
	}

	case 2:
		execute_term((char **)args_tui_internet);
		break;

	default:
		break;
	}
}

/* 'wdev' and 'wap', when non-NULL, receive the Wi-Fi device and access
 * point behind a wireless primary connection, or NULL. */
static unsigned int
getactive(NMClient *c, NMDeviceWifi **wdev, NMAccessPoint **wap)
{
	NMActiveConnection *ac;
	const GPtrArray *acdev;
	NMAccessPoint *ap;
	unsigned int state;
	NMDeviceWifi *dev;
	const char *type;

	if (wdev)
		*wdev = NULL;
	if (wap)
		*wap = NULL;

	ac = nm_client_get_primary_connection(c);
	if (!ac)
		return 0;

	type = nm_active_connection_get_connection_type(ac);
	if (!type)
		return 0;

	if (!strcmp(type, "802-3-ethernet"))
		return 1;

	if (!strcmp(type, "802-11-wireless")) {
		acdev = nm_active_connection_get_devices(ac);
		if (!acdev || acdev->len < 1)
			return 7;

		dev = NM_DEVICE_WIFI(g_ptr_array_index(acdev, 0));
		if (!dev)
			return 7;
		if (wdev)
			*wdev = dev;

		ap = nm_device_wifi_get_active_access_point(dev);
		if (!ap)
			return 7;
		if (wap)
			*wap = ap;

		/* map 0..100 -> 0..4 */
		state = (nm_access_point_get_strength(ap) / 20);
		state = state > 4 ? 4 : state;
		return (2 + state); /* 2..6 */
	}

	return 0;
}

static void
nminit(NMClient **c)
{
	GError *error = NULL;

	if (!c)
		return;

	*c = nm_client_new(NULL, &error);

	if (error) {
		warn("Error initializing NetworkManager client: %s", error->message);
		g_error_free(error);
	}

	/* If nm_client_new failed, *c will be NULL. Caller already checks. */
}

static void
printinfo(NMClient *c, const int ind)
{
	const GPtrArray *devarr;
	unsigned int icind;
	NMDevice *dev;
	GString *str;

	devarr = nm_client_get_devices(c);
	icind = (unsigned int)(ind > 2 ? 2 : ind);

	if (!devarr) {
		notify("Network Devices Info", "No network devices detected", icons_internet_notif[0]);
		return;
	}

	str = g_string_new("");

	for (int i = 0; i < (int)devarr->len; i++) {
		dev = g_ptr_array_index(devarr, i);
		if (!dev)
			continue;

		int type = nm_device_get_device_type(dev);
		switch (type) {
		case NM_DEVICE_TYPE_ETHERNET:
			ethapp(dev, str);
			comapp(dev, str);
			break;

		case NM_DEVICE_TYPE_WIFI:
			wifiapp(dev, str);
			comapp(dev, str);
			break;

		default:
			break;
		}
	}

	if (str->len <= 1)
		notify("Network Devices Info", "No network devices detected", icons_internet_notif[icind]);
	else
		notify("Network Devices Info", str->str, icons_internet_notif[icind]);

	g_string_free(str, TRUE);
}

static void
togglewifi(NMClient *client)
{
	/* May fail due to permissions/polkit; we at least report what we observe after the call. */
	gboolean current = nm_client_wireless_get_enabled(client);
	nm_client_wireless_set_enabled(client, !current);

	const char *msg = current ? "WiFi Disabled" : "WiFi Enabled";
	notify("WiFi Status", msg, icons_internet_notif[2]);
}

static void
wifiapp(NMDevice *dev, GString *str)
{
	NMActiveConnection *ac;
	NMAccessPoint *ap;
	GBytes *ssid;
	guint32 freq_mhz;
	guint8 stren;

	ac = nm_device_get_active_connection(dev);
	if (!ac) {
		g_string_append(str, "Wifi: Disconnected\n\n");
		return;
	}

	ap = nm_device_wifi_get_active_access_point(NM_DEVICE_WIFI(dev));
	if (!ap) {
		g_string_append(str, "Wifi: Connected (AP unknown)\n\n");
		return;
	}

	ssid     = nm_access_point_get_ssid(ap);
	freq_mhz = nm_access_point_get_frequency(ap); /* MHz */
	stren    = nm_access_point_get_strength(ap);

	const char *ssid_disp = "*hidden_network*";
	char *ssid_utf8 = NULL;

	if (ssid) {
		ssid_utf8 = nm_utils_ssid_to_utf8(g_bytes_get_data(ssid, NULL),
		                                 g_bytes_get_size(ssid));
		if (ssid_utf8 && *ssid_utf8)
			ssid_disp = ssid_utf8;
	}

	/* Convert MHz -> GHz for display */
	double freq_ghz = freq_mhz > 0 ? (freq_mhz / 1000.0) : 0.0;

	g_string_append_printf(str,
		"Wifi\n"
		"SSID: %s\n"
		"Strn: %d%% | Freq: %.1f GHz\n",
		ssid_disp, (int)stren, freq_ghz);

	if (ssid_utf8)
		g_free(ssid_utf8);
}

static void
show(NMClient *c)
{
	unsigned int state = c ? clampstate(getactive(c, NULL, NULL)) : 7;

	emit(icon_colors[state], icons_internet[state], ascii_internet[state]);
}

struct Watch {
	gpointer obj;
	gulong   id;
};

static void changed(GObject *obj, GParamSpec *pspec, gpointer client);

/* Moves 'w' to 'obj' ('signal' on it), dropping the previous one. */
static void
rebind(struct Watch *w, gpointer obj, const char *signal, NMClient *client)
{
	if (w->obj == obj)
		return;

	if (w->obj) {
		g_signal_handler_disconnect(w->obj, w->id);
		g_object_unref(w->obj);
	}

	w->obj = obj ? g_object_ref(obj) : NULL;
	w->id  = obj ? g_signal_connect(obj, signal, G_CALLBACK(changed), client) : 0;
}

/*
 * Client-level notifications cover connecting, disconnecting, switching
 * networks and NetworkManager restarting; roaming and signal strength live
 * on the current Wi-Fi device and access point, so those are re-bound on
 * every change.
 */
static void
changed(GObject *obj, GParamSpec *pspec, gpointer client)
{
	static struct Watch wdev, wap;
	NMDeviceWifi       *dev;
	NMAccessPoint      *ap;

	(void)obj;
	(void)pspec;

	getactive(client, &dev, &ap);
	rebind(&wdev, dev, "notify::" NM_DEVICE_WIFI_ACTIVE_ACCESS_POINT, client);
	rebind(&wap, ap, "notify::" NM_ACCESS_POINT_STRENGTH, client);

	show(client);
}

static int
watch(void)
{
	static const char *const props[] = {
		NM_CLIENT_PRIMARY_CONNECTION,
		NM_CLIENT_NM_RUNNING,
		NM_CLIENT_ACTIVE_CONNECTIONS,
	};

	NMClient *client;
	char      signal[64];

	nminit(&client);
	if (!client) {
		show(NULL);
		return 1;
	}

	for (size_t i = 0; i < LEN(props); i++) {
		snprintf(signal, sizeof(signal), "notify::%s", props[i]);
		g_signal_connect(client, signal, G_CALLBACK(changed), client);
	}

	changed(NULL, NULL, client);
	g_main_loop_run(g_main_loop_new(NULL, FALSE));

	return 1;
}

int
main(void)
{
	static const struct Button buttons[] = {
		{ 1, on_left },
		{ 3, on_right },
	};

	struct Context c = { NULL, 0 };

	set_name("statusblocks-internet");

	if (persistent())
		return watch();

	nminit(&c.client);

	if (c.client) {
		c.icind = (int)clampstate(getactive(c.client, NULL, NULL));
		dispatch(buttons, LEN(buttons), &c);
	}

	show(c.client);

	if (c.client)
		g_object_unref(c.client);

	return 0;
}
