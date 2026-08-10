/* See LICENSE file for copyright and license details. */

#include <stdio.h>
#include <dbus/dbus.h>

#define BLUETOOTH_C

#include "colors.h"
#include "toggle.h"
#include "utils.h"
#include "config.h"

#define DBUS_TIMEOUT_MS 1000
#define LEN(a)          (sizeof(a) / sizeof((a)[0]))

static const char *adapter_paths[] = { "/org/bluez/hci0", "/org/bluez/hci1", NULL };

static int
getbtadapterstate(DBusConnection *conn, DBusError *err, const char *objpath)
{
	DBusMessage     *msg, *reply;
	DBusMessageIter  args, replyArgs;

	const char  *iface   = "org.bluez.Adapter1";
	const char  *prop    = "Powered";
	dbus_bool_t  powered = FALSE;

	msg = dbus_message_new_method_call("org.bluez", objpath, "org.freedesktop.DBus.Properties", "Get");
	if (!msg) {
		warn("Failed to create DBus message");
		return -1;
	}

	dbus_message_iter_init_append(msg, &args);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &iface);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &prop);

	reply = dbus_connection_send_with_reply_and_block(conn, msg, DBUS_TIMEOUT_MS, err);
	dbus_message_unref(msg);

	if (dbus_error_is_set(err)) {
		warn("D-Bus error: %s", err->message);
		dbus_error_free(err);
		return -1;
	}

	if (!reply)
		return -1;

	if (dbus_message_iter_init(reply, &replyArgs) &&
	    dbus_message_iter_get_arg_type(&replyArgs) == DBUS_TYPE_VARIANT) {
		DBusMessageIter variant;

		dbus_message_iter_recurse(&replyArgs, &variant);

		/*
		 * get_basic() writes sizeof(contained type) bytes through the
		 * pointer, so the type must be confirmed before the call.
		 */
		if (dbus_message_iter_get_arg_type(&variant) == DBUS_TYPE_BOOLEAN)
			dbus_message_iter_get_basic(&variant, &powered);
		else
			warn("Unexpected type for Powered property");
	}

	dbus_message_unref(reply);
	return powered ? 1 : 0;
}

/*
 * Walks the known adapter paths and returns the powered state of the first
 * one that answers. If 'out' is non-NULL it receives that adapter's path.
 * Returns -1 if no adapter responded.
 */
static int
findadapter(DBusConnection *conn, const char **out)
{
	for (int i = 0; adapter_paths[i]; i++) {
		DBusError err;
		int       state;

		dbus_error_init(&err);
		state = getbtadapterstate(conn, &err, adapter_paths[i]);
		dbus_error_free(&err);

		if (state >= 0) {
			if (out)
				*out = adapter_paths[i];
			return state;
		}
	}

	return -1;
}

static int
getbtstate(void)
{
	DBusConnection *conn = NULL;
	DBusError err;
	int state = -1;

	dbus_error_init(&err);

	conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
	if (!conn || dbus_error_is_set(&err)) {
		warn("Failed to connect to the DBus system bus: %s",
		     dbus_error_is_set(&err) ? err.message : "unknown error");
		dbus_error_free(&err);
		return -1;
	}

	state = findadapter(conn, NULL);

	dbus_connection_unref(conn);
	return state;
}

static int
setbtpowered(DBusConnection *conn, DBusError *err, const char *objpath, dbus_bool_t powered)
{
	DBusMessage     *msg, *reply;
	DBusMessageIter  args, valueIter;

	const char *iface = "org.bluez.Adapter1";
	const char *prop  = "Powered";

	msg = dbus_message_new_method_call("org.bluez", objpath, "org.freedesktop.DBus.Properties", "Set");
	if (!msg) {
		warn("Failed to create DBus message");
		return -1;
	}

	dbus_message_iter_init_append(msg, &args);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &iface);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &prop);

	dbus_message_iter_open_container(&args, DBUS_TYPE_VARIANT, "b", &valueIter);
	dbus_message_iter_append_basic(&valueIter, DBUS_TYPE_BOOLEAN, &powered);
	dbus_message_iter_close_container(&args, &valueIter);

	reply = dbus_connection_send_with_reply_and_block(conn, msg, DBUS_TIMEOUT_MS, err);
	dbus_message_unref(msg);

	if (dbus_error_is_set(err)) {
		warn("D-Bus error: %s", err->message);
		dbus_error_free(err);
		return -1;
	}

	if (!reply)
		return -1;

	dbus_message_unref(reply);
	return 0;
}

static void
togglebt(void)
{
	DBusConnection  *conn = NULL;
	DBusError        err;
	int              state = -1;
	const char      *objpath = NULL;

	dbus_error_init(&err);

	conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
	if (!conn || dbus_error_is_set(&err)) {
		warn("Failed to connect to the DBus system bus: %s",
		     dbus_error_is_set(&err) ? err.message : "unknown error");
		dbus_error_free(&err);
		return;
	}

	state = findadapter(conn, &objpath);

	if (state < 0 || !objpath) {
		warn("No Bluetooth adapter found (hci0/hci1)");
		dbus_connection_unref(conn);
		return;
	}

	dbus_error_init(&err);
	if (setbtpowered(conn, &err, objpath, state ? FALSE : TRUE) < 0) {
		dbus_connection_unref(conn);
		return;
	}

	dbus_connection_unref(conn);
}

static void
on_left(void *ctx)
{
	(void)ctx;
	execute_term((char **)bt_tui_cmd);
}

static void
on_middle(void *ctx)
{
	(void)ctx;
	togglebt();
}

int
main(void)
{
	static const struct Button buttons[] = {
		{ 1, on_left },
		{ 2, on_middle },
	};

	int                 state;
	const char *const  *icons;

	set_name("statusblocks-bluetooth");
	clr_init();
	toggle_init();

	icons = toggle_get(show_bt) ? icons_bluetooth : ascii_bluetooth;

	dispatch(buttons, LEN(buttons), NULL);

	state = getbtstate();
	if (state < 0)
		state = 0;

	printf("%s%s" CLR_NRM "\n", clr_get(clr_bt), icons[state ? 1 : 0]);

	return 0;
}
