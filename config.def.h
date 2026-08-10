/* See LICENSE file for copyright and license details. */

#ifndef CONFIG_H
#define CONFIG_H

/*
 * HOW THIS FILE IS ORGANISED
 *
 * Everything before the first "#ifdef" is shared by every block. After
 * that, each block has its own section guarded by a macro that only that
 * block defines: battery.c does "#define BATTERY_C" before including this
 * file, so it sees the BATTERY BLOCK section and nothing else.
 *
 * That is why names such as path_wifi_connect appear several times below without
 * colliding: exactly one section is ever compiled into a given block. It
 * also means a setting placed in the wrong section is silently ignored
 * rather than reported, so keep each one under the block that reads it.
 *
 * Settings marked "Requires:" name an external program that must be on
 * $PATH, or a script you must supply yourself. Clicking a block whose
 * program is missing does nothing and writes a message to stderr, which
 * dwmblocks discards; run the block by hand in a terminal to see it.
 */

/*
 * Terminal used by blocks that open a TUI. $TERMINAL overrides it at
 * runtime, so this is only the fallback. Requires: one of the two.
 */
static const char term_cmd[] = "st";
static const char term_title_opt[] = "-t";

/*
 * Path to the X resources file whose mtime marks the colour/icon cache
 * stale, e.g. right after you edit it and run `xrdb -merge`. Accepts '~'
 * and '$VAR' (see envexpand() in utils.h). Point this at whatever file you
 * actually edit and reload: plenty of setups use something other than the
 * traditional ~/.Xresources, e.g. ~/.config/xresources/Xresources.
 */
static const char xresources_path[] = "~/.Xresources";

/* ============================================================
 * COLOURS
 * ============================================================ */

/*
 * How a colour is written into the status line. CLR_FMT must contain
 * exactly one %s, which receives a "#RRGGBB" string; CLR_NRM ends the
 * coloured run. The defaults target dwm's status2d patch.
 *
 *   status2d (dwm)   "^c%s^"                  "^d^"
 *   Pango (waybar,
 *   polybar, i3bar)  "<span color='%s'>"      "</span>"
 *   polybar native   "%%{F%s}"                "%%{F-}"
 *   no colour        "%.0s"                   ""
 */
#define CLR_FMT "^c%s^"
#define CLR_NRM "^d^"

/*
 * Default colours, listed in enum Color order (see src/include/colors.h).
 * An empty string leaves that colour unset, so the block renders in the
 * status bar's own colour. Matching entries in the X resource database
 * override these at runtime, e.g.
 *
 *   statusblocks.clr_bat_crt: #F38BA8
 *
 * A compile-time check in colors.c fails the build if this list and the
 * enum ever fall out of step.
 */
static const char *const clr_defaults[] = {
	"#F38BA8",  /* clr_bat_crt  battery critical */
	"#F9E2AF",  /* clr_bat_low  battery low      */
	"#CDD6F4",  /* clr_bat_nrm  battery normal   */
	"#F9E2AF",  /* clr_bat_chg  battery charging */
	"#CDD6F4",  /* clr_bt       bluetooth        */
	"#CDD6F4",  /* clr_date     date             */
	"#CDD6F4",  /* clr_net_nrm  network normal   */
	"#F38BA8",  /* clr_net_err  network error    */
	"#89B4FA",  /* clr_sys_pkg  pending updates  */
	"#CDD6F4",  /* clr_sys_nrm  kernel release   */
	"#CDD6F4",  /* clr_kbd      keyboard layout  */
	"#CDD6F4",  /* clr_mem      memory           */
	"#F38BA8",  /* clr_pwr      power menu       */
	"#CDD6F4",  /* clr_tim      clock            */
	"#CDD6F4",  /* clr_vol_nrm  volume normal    */
	"#F38BA8",  /* clr_vol_mut  volume muted     */
	"#F38BA8"   /* clr_cal      calendar accent   */
};

/* ============================================================
 * ICON STYLE
 * ============================================================ */

/*
 * Whether each block renders its compiled-in icon (typically a Nerd Font
 * glyph) or that block's plain-ASCII fallback. 1 selects the icon, 0 the
 * fallback. Listed in enum Toggle order (see src/include/toggle.h).
 * Matching entries in the X resource database override these at runtime,
 * e.g.
 *
 *   statusblocks.icon_bat: 0
 *
 * A compile-time check in toggle.c fails the build if this list and the
 * enum ever fall out of step. For battery, bluetooth, internet, power and
 * volume the icon is the whole block, so their fallback is a short ASCII
 * tag rather than nothing, and stays distinct from every other block's.
 */
static const unsigned int icon_defaults[] = {
	1,  /* show_bat  battery       */
	1,  /* show_bt   bluetooth     */
	1,  /* show_date date/calendar */
	1,  /* show_kbd  keyboard      */
	1,  /* show_mem  memory        */
	1,  /* show_net  internet      */
	1,  /* show_pwr  power menu    */
	1,  /* show_sys  system        */
	1,  /* show_tim  clock         */
	1,  /* show_vol  volume        */
};

/* ============================================================
 * BATTERY BLOCK
 * ============================================================ */
#ifdef BATTERY_C

/* Enable power management features (optimus-manager support) */
#define POWER_MANAGEMENT


/* Status icons, ordered from empty to full; the last is "charging". */
static const char *const icons_battery[] = {
	" ",
	" ",
	" ",
	" ",
	" ",
	" ",
};

/*
 * Plain-ASCII fallback used when show_bat is 0. ascii_bat_tag is followed
 * by the live percentage, e.g. "B:87%"; ascii_bat_chg replaces the whole
 * thing while charging, and ascii_bat_none when no battery is present.
 */
static const char ascii_bat_tag[]  = "B:";
static const char ascii_bat_chg[]  = "CHG";
static const char ascii_bat_none[] = "B:--";
#endif

/* ============================================================
 * BLUETOOTH BLOCK
 * ============================================================ */
#ifdef BLUETOOTH_C

/* TUI application for bluetooth settings */
const char *bt_tui_cmd[] = { term_cmd, "bluetuith", NULL };

/*
 * Plain-ASCII fallback: [0] disabled, [1] enabled. Used in place of
 * icons_bluetooth when show_bt is 0.
 */
static const char *const ascii_bluetooth[] = {
	"BT-",
	"BT+",
};

/* Status icons: [0] disabled, [1] enabled. */
static const char *const icons_bluetooth[] = {
	"󰂲",
	"󰂯",
};
#endif

/* ============================================================
 * DATE BLOCK
 * ============================================================ */
#ifdef DATE_C

/*
 * First column of the calendar, as a tm_wday value: 1 = Monday (most of
 * Europe), 0 = Sunday (US), 6 = Saturday. Month and weekday names come
 * from the locale, so run the date block under the locale you want.
 */
static const int calendar_week_start = 1;

/* Icon shown in the bar when show_date is set. */
static const char icon_date[] = " ";

#endif

/* ============================================================
 * INTERNET BLOCK
 * ============================================================ */
#ifdef INTERNET_C

/* Network management TUI */
const char *args_tui_internet[] = {
	term_cmd,
	term_title_opt, "Network Configuration",
	"nmtui",
	NULL
};

/* WiFi connection script */
static const char path_wifi_connect[] = "~/.local/bin/wifi-prompt";
const char *args_wifi_connect[] = {"wifi-prompt", NULL};


/* Bar icons, indexed by connection state. */
static const char *const icons_internet[] = {
	"󰤮 ",  /* 0: no primary connection / unknown */
	" ",  /* 1: ethernet */
	"󰤯 ",  /* 2: wifi 0 */
	"󰤟 ",  /* 3: wifi 1 */
	"󰤢 ",  /* 4: wifi 2 */
	"󰤥 ",  /* 5: wifi 3 */
	"󰤨 ",  /* 6: wifi 4 */
	"󰤫 ",  /* 7: error */
};

/*
 * Plain-ASCII fallback, same order as icons_internet. Used in place of it
 * when show_net is 0.
 */
static const char *const ascii_internet[] = {
	"down",  /* 0: no primary connection / unknown */
	"eth",   /* 1: ethernet */
	"wifi",  /* 2: wifi 0 */
	"wifi",  /* 3: wifi 1 */
	"wifi",  /* 4: wifi 2 */
	"wifi",  /* 5: wifi 3 */
	"wifi",  /* 6: wifi 4 */
	"err",   /* 7: error */
};

/* Notification icons: [0] error, [1] wired, [2] wireless. */
static const char *const icons_internet_notif[] = {
	"x",
	"tdenetworkmanager",
	"wifi-radar",
};

/* xmenu prompt. Each line is "<label>\t<value>". */
static const char menu_internet[] = "󱛄 Toggle Wifi\t0\n󱛃 Connect to wifi\t1\n󱚾 TUI options\t2";
#endif

/* ============================================================
 * SYSTEM BLOCK
 * ============================================================ */
#ifdef SYSTEM_C

/*
 * The block counts pending updates from two sources and shows the kernel
 * release. Nothing below is distribution-specific; the defaults are for
 * Arch, and the comments give equivalents for other package managers.
 * Set a command to "" to disable that source.
 *
 *   Debian/Ubuntu   primary:   "apt list --upgradable 2>/dev/null | tail -n +2"
 *                   watch:     "/var/lib/dpkg/status"
 *   Fedora          primary:   "dnf -q --refresh check-update"
 *                   watch:     "/var/lib/rpm"
 *   void            primary:   "xbps-install -Mun"
 *                   watch:     "/var/db/xbps"
 *
 * Each command should print one line per pending update; only the line
 * count is used. A non-zero exit status is ignored, because several of
 * these exit non-zero precisely when there is nothing to update.
 */

/* Requires: a package manager, and an AUR helper for the secondary source. */
const char *cmd_updates_primary   = "checkupdates";
const char *cmd_updates_secondary = "paru -Qua";

/* Labels used in the notification, one per source. */
static const char label_updates_primary[]   = "Pacman Updates";
static const char label_updates_secondary[] = "AUR Updates";

/* System update command. Requires: a package manager. */
const char *args_update_cmd[] = {
	term_cmd,
	term_title_opt, "System Upgrade",
	"sh", "-c",
	"echo \"Upgrading system\" && paru",
	NULL
};

/* Show release info in bar */
const unsigned int show_release = 1;

/* Show update count in bar*/
const unsigned int show_update_count = 1;

/* Bar icons: the kernel (tux) glyph and the pending-updates glyph. */
static const char icon_system_kernel[] = "";
static const char icon_system_pkg[] = "󰏖";

/*
 * Plain-ASCII tag printed before the pending-update count when show_sys
 * is 0, so the number isn't left bare (e.g. "U:5").
 */
static const char ascii_system_pkg[] = "U:";

/* Notification icons, one per update source. */
static const char icon_updates_primary[]   = "󰏖";
static const char icon_updates_secondary[] = "";

/*
 * Seconds before cached update counts are refreshed. Set to 0 to disable
 * caching entirely and query on every run.
 */
static const long update_cache_ttl = 3600;

/*
 * The cache is also invalidated whenever this path's mtime is newer than
 * it, so a post-transaction hook from the package manager makes the count
 * drop to zero immediately instead of waiting for the TTL. Point it at
 * whatever the package manager rewrites when it installs or removes
 * something. Set to "" to rely on the TTL alone.
 */
static const char update_watch_path[] = "/var/lib/pacman/local";
#endif

/* ============================================================
 * KEYBOARD BLOCK
 * ============================================================ */
#ifdef KEYBOARD_C

/*
 * Keyboard layout switching script, run on left click.
 * Requires: a script of your own that cycles the layout. The default
 * points at dwm-xkbnext from https://github.com/dimgerasimou/binaries
 * A plain alternative: setxkbmap with your layouts, wrapped in a script.
 */
static const char path_language_switch[] = "~/.local/bin/dwm-xkbnext";
const char *args_language_switch[] = { "dwm-xkbnext", NULL };


/* Icon shown in the bar when show_kbd is set. */
static const char icon_keyboard[] = " ";
#endif

/* ============================================================
 * MEMORY BLOCK
 * ============================================================ */
#ifdef MEMORY_C

/* Task manager, opened on right click. Requires: htop, and term_cmd. */
const char *args_task_manager[] = { term_cmd, "sh", "-c", "htop", NULL };


/* Icon shown in the bar when show_mem is set. */
static const char icon_memory[] = " ";
#endif

/* ============================================================
 * POWER BLOCK
 * ============================================================ */
#ifdef POWER_C

/* Enable clipboard integration */
#define CLIPBOARD

/* Enable power management features (optimus-manager support) */
#define POWER_MANAGEMENT

/* Status bar restart cmd.
 * Requires: dwmblocks. Adjust if yours is something else. */
const char *args_dwmblocks_restart[] = {"dwmblocks", "--restart", NULL};

/* Lock screen command. Requires: slock, or any locker you prefer. */
const char *args_lockscreen[]       = {"slock", NULL};

/* Clipboard management */
const char *args_clipboard_delete[] = {"sh", "-c", "clipdel -d \".*\"", NULL};


/* Bar icon. */
static const char icon_power[] = "";

/* Plain-ASCII fallback for icon_power, used when show_pwr is 0. */
static const char ascii_power[] = "PWR";

/* xmenu prompts. Each line is "<label>\t<value>". */
static const char menu_power[] = " Shutdown\t0\n Reboot\t1\n\n󰗽 Logout\t2\n Lock\t3\n\n Restart DwmBlocks\t4";
static const char menu_power_optimus[] = "\n󰘚 Optimus Manager\t5";
static const char menu_power_clipboard[] = "\n󰅌 Clipmenu\t6";
static const char menu_optimus[] = "Integrated\t0\nHybrid\t1\nNvidia\t2";
static const char menu_clipboard[] = "Pause clipmenu for 1 minute\t0\nClear clipboard\t1";
static const char menu_yes_no[] = "Are you sure?\t-1\nYes\t1\nNo\t0";
#endif

/* ============================================================
 * TIME BLOCK
 * ============================================================ */
#ifdef TIME_C

/* Icon shown in the bar when show_tim is set. */
static const char icon_time[] = " ";
#endif


/* ============================================================
 * VOLUME BLOCK
 * ============================================================ */
#ifdef VOLUME_C

/* What the block displays:
 *   0 - icon and volume
 *   1 - icon only
 *   2 - volume only
 */
const unsigned int display_type = 0;

/* Padding for volume string; boolean */
const unsigned int volume_padding = 1;

/* Audio equalizer application */
const char *args_eqalizer[]        = {"easyeffects", NULL};

/* Volume control script and arguments */
const char *args_volume_increase[] = {"audio-ctl", "up", NULL};
const char *args_volume_decrase[]  = {"audio-ctl", "down", NULL};
const char *args_volume_mute[]     = {"audio-ctl", "mute", NULL};
/*
 * Volume control script, run on middle click and scroll.
 * Requires: a script of your own accepting "up", "down" and "mute". The
 * default points at audio-ctl from https://github.com/dimgerasimou/binaries
 * A plain alternative: a wrapper around wpctl or pamixer.
 */
static const char path_volume_control[] = "~/.local/bin/audio-ctl";


/* Bar icons: [0] muted, [1] low, [2] medium, [3] high, [4] no sink. */
static const char *const icons_volume[] = {
	" ",
	" ",
	" ",
	" ",
	"",
};

/*
 * Plain-ASCII tag used when show_vol is 0, printed right before the
 * volume percentage (e.g. "V:87%"). ascii_vol_mute_tag replaces both the
 * tag and the percentage while muted (e.g. "M").
 */
static const char ascii_vol_tag[]      = "V:";
static const char ascii_vol_mute_tag[] = "M";

/* Notification icons for the default sink and source. */
static const char icon_vol_sink[] = "";
static const char icon_vol_source[] = "";
#endif

#endif /* CONFIG_H */
