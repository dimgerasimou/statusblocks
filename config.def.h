/* See LICENSE file for copyright and license details. */

#ifndef CONFIG_H
#define CONFIG_H

/*
 * HOW THIS FILE IS ORGANISED
 *
 * Colours and the choice between icons and ASCII text are not set here:
 * every block prints both its icon and ASCII form, and dwmblocks picks one
 * and colours it (see its config.h and the statusblocks.* X resources).
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

/* ============================================================
 * GPU PROFILE
 * ============================================================
 * Hybrid-graphics switching, used by the battery and power blocks.
 * Comment out GPU_PROFILE to build without it. */

#define GPU_PROFILE

/* Available backends. */
#define GPU_PROFILE_OPTIMUS_MANAGER 1
#define GPU_PROFILE_XGPUPROFILE     2

/* Which one to build against. */
#define GPU_PROFILE_BACKEND GPU_PROFILE_XGPUPROFILE

/* ============================================================
 * SYSTEM POWER PROFILE
 * ============================================================
 * CPU, embedded controller, Wi-Fi and brightness profiles through
 * powerprofile, used by the battery and power blocks. Separate from the
 * GPU switch above.
 * Comment out POWERPROFILE to build without it.
 * Requires: powerprofile, and passwordless sudo for the modes below. */

#define POWERPROFILE

/* ============================================================
 * BATTERY BLOCK
 * ============================================================ */
#ifdef BATTERY_C

/* Status icons, ordered from empty to full; the last is "charging". */
static const char *const icons_battery[] = {
	" ",
	" ",
	" ",
	" ",
	" ",
	" ",
};

/* Notification icon, a name from the active icon theme. */
static const char icon_battery_notif[] = "battery";

/*
 * Plain-ASCII text used in ASCII mode. ascii_bat_tag is followed
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
 * Plain-ASCII text: [0] disabled, [1] enabled. Used in place of
 * icons_bluetooth in ASCII mode.
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

/* Calendar accent, used when dwmblocks doesn't export BLOCK_CLR_CAL. */
static const char cal_clr_fallback[] = "#F38BA8";

/* Icon shown before the date in icon mode. */
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
 * Plain-ASCII text, same order as icons_internet. Used in place of it
 * in ASCII mode.
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
 * UPDATES BLOCK
 * ============================================================ */
#ifdef UPDATES_C

/*
 * The block counts pending updates from two sources. Nothing below is distribution-specific; the defaults are for
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

/* Show update count in bar */
const unsigned int show_update_count = 1;

/* Bar icon for pending updates. */
static const char icon_updates[] = "󰏖";

/*
 * Plain-ASCII tag printed before the pending-update count in ASCII mode,
 * so the number isn't left bare (e.g. "U:5").
 */
static const char ascii_updates[] = "U:";

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
 * KERNEL BLOCK
 * ============================================================ */
#ifdef KERNEL_C

/* Bar icon, shown before the release in icon mode. */
static const char icon_kernel[] = "";
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


/* Icon shown before the keyboard layout in icon mode. */
static const char icon_keyboard[] = " ";
#endif

/* ============================================================
 * MEMORY BLOCK
 * ============================================================ */
#ifdef MEMORY_C

/* Task manager, opened on right click. Requires: htop, and term_cmd. */
const char *args_task_manager[] = { term_cmd, "sh", "-c", "htop", NULL };


/* Icon shown before the memory usage in icon mode. */
static const char icon_memory[] = " ";
#endif

/* ============================================================
 * POWER BLOCK
 * ============================================================ */
#ifdef POWER_C

/* Enable clipboard integration */
#define CLIPBOARD

/* Status bar restart cmd.
 * Requires: dwmblocks. Adjust if yours is something else. */
const char *args_dwmblocks_restart[] = {"dwmblocks", "--restart", NULL};

/* Lock screen command. Requires: slock, or any locker you prefer. */
const char *args_lockscreen[]       = {"slock", NULL};

/* Clipboard management */
const char *args_clipboard_delete[] = {"sh", "-c", "clipdel -d \".*\"", NULL};


/* Bar icon. */
static const char icon_power[] = " ";

/* Plain-ASCII text for icon_power, used in ASCII mode. */
static const char ascii_power[] = "PWR";

/* xmenu prompts. Each line is "<label>\t<value>". */
static const char menu_power[] = " Shutdown\t0\n Reboot\t1\n\n󰗽 Logout\t2\n Lock\t3\n\n Restart DwmBlocks\t4";
static const char menu_power_gpu_profile[] = "\n󰘚 GPU Profile\t5";
static const char menu_power_profile[] = "\n󰓅 Power Profile\t7";
static const char menu_power_clipboard[] = "\n󰅌 Clipmenu\t6";
static const char menu_clipboard[] = "Pause clipmenu for 1 minute\t0\nClear clipboard\t1";
static const char menu_yes_no[] = "Are you sure?\t-1\nYes\t1\nNo\t0";
#endif

/* ============================================================
 * GPU PROFILE BACKEND
 * ============================================================ */
#ifdef GPUPROFILE_C
#ifdef GPU_PROFILE

/* A GPU arrangement that can be in effect. `match` is the substring that
 * identifies it in the backend's status output. */
struct GpuProfileState {
	const char *label;
	const char *match;
};

/* Something that can be asked for. Takes effect at the next session. */
struct GpuProfileMode {
	const char        *label;
	const char *const *argv;
};

#if GPU_PROFILE_BACKEND == GPU_PROFILE_OPTIMUS_MANAGER

static const char gpu_profile_status_cmd[] = "optimus-manager --status";

/* The line naming the arrangement currently in effect. */
static const char gpu_profile_state_key[] = "Current";

static const struct GpuProfileState gpu_profile_states[] = {
	{ "Unknown",    ""           },
	{ "Integrated", "integrated" },
	{ "Hybrid",     "hybrid"     },
	{ "Discrete",   "nvidia"     },
};

static const char *const args_gpu_integrated[] = {"optimus-manager", "--no-confirm", "--switch", "integrated", NULL};
static const char *const args_gpu_hybrid[]     = {"optimus-manager", "--no-confirm", "--switch", "hybrid",     NULL};
static const char *const args_gpu_discrete[]   = {"optimus-manager", "--no-confirm", "--switch", "nvidia",     NULL};

static const struct GpuProfileMode gpu_profile_modes[] = {
	{ "Unknown",    NULL                },
	{ "Integrated", args_gpu_integrated },
	{ "Hybrid",     args_gpu_hybrid     },
	{ "Discrete",   args_gpu_discrete   },
};

#elif GPU_PROFILE_BACKEND == GPU_PROFILE_XGPUPROFILE

static const char gpu_profile_status_cmd[] = "xgpuprofile --status";

/* "Running" is the layout this session actually started with. The "Mode"
 * line is the saved setting, which under `auto` does not say which GPU
 * ended up driving the display. */
static const char gpu_profile_state_key[] = "Running";

/* "integrated GPU (hybrid)" contains both words, so the discrete entry is
 * matched first and the hybrid entry keys off "integrated". */
static const struct GpuProfileState gpu_profile_states[] = {
	{ "Unknown",  ""           },
	{ "Discrete", "discrete"   },
	{ "Hybrid",   "integrated" },
};

/* --once applies to the next session only and leaves the saved
 * configuration alone, which is what a menu pick should do. */
static const char *const args_gpu_hybrid[]   = {"sudo", "-n", "xgpuprofile", "--once", "hybrid", NULL};
static const char *const args_gpu_discrete[] = {"sudo", "-n", "xgpuprofile", "--once", "dgpu",   NULL};

static const struct GpuProfileMode gpu_profile_modes[] = {
	{ "Unknown",  NULL              },
	{ "Hybrid",   args_gpu_hybrid   },
	{ "Discrete", args_gpu_discrete },
};

#else
#error "GPU_PROFILE_BACKEND is not set to a known backend"
#endif

#endif /* GPU_PROFILE */
#endif /* GPUPROFILE_C */

/* ============================================================
 * SYSTEM POWER PROFILE BACKEND
 * ============================================================ */
#ifdef POWERPROFILE_C
#ifdef POWERPROFILE

/* A profile that can be in effect. `match` is the profile's name exactly
 * as it appears in /etc/powerprofile.conf, without spaces. A profile
 * missing from this list is shown as Unknown. */
struct PowerProfileState {
	const char *label;
	const char *match;
};

/* Something that can be asked for. Takes effect at once. */
struct PowerProfileMode {
	const char        *label;
	const char *const *argv;
};

/* Prints "ac: yes  profile: ac  override: none" on its first line, and
 * needs no privileges. "profile" is the one applied last, which is the
 * effective one: under `--auto` it follows the AC adapter. */
static const char power_profile_status_cmd[] = "powerprofile --status";
static const char power_profile_state_key[]  = "profile: ";

static const struct PowerProfileState power_profile_states[] = {
	{ "Unknown",  ""         },
	{ "Battery",  "battery"  },
	{ "Balanced", "balanced" },
	{ "Docked",   "docked"   },
	{ "Gaming",   "gaming"   },
	{ "Saver",    "saver"    },
};

/*
 * sudo matches these argument lists exactly, so each mode needs a line of
 * its own in /etc/sudoers.d/powerprofile; conf/powerprofile.sudoers in the
 * powerprofile repository has all of them. Modes for profiles you do not
 * define in /etc/powerprofile.conf can simply be deleted.
 *
 * "battery" and "balanced" are the on_battery and on_ac profiles of
 * conf/powerprofile.conf: applying one by name forces it as if the adapter
 * said so. Auto hands control back to the adapter and drops any manual
 * override.
 */
static const char *const args_profile_auto[]     = {"sudo", "-n", "powerprofile", "--auto", NULL};
static const char *const args_profile_battery[]  = {"sudo", "-n", "powerprofile", "--profile", "battery",  NULL};
static const char *const args_profile_balanced[] = {"sudo", "-n", "powerprofile", "--profile", "balanced", NULL};
static const char *const args_profile_docked[]   = {"sudo", "-n", "powerprofile", "--profile", "docked",   NULL};
static const char *const args_profile_gaming[]   = {"sudo", "-n", "powerprofile", "--profile", "gaming",   NULL};
static const char *const args_profile_saver[]    = {"sudo", "-n", "powerprofile", "--profile", "saver",    NULL};

static const struct PowerProfileMode power_profile_modes[] = {
	{ "Unknown",  NULL                  },
	{ "Auto",     args_profile_auto     },
	{ "Battery",  args_profile_battery  },
	{ "Balanced", args_profile_balanced },
	{ "Docked",   args_profile_docked   },
	{ "Gaming",   args_profile_gaming   },
	{ "Saver",    args_profile_saver    },
};

#endif /* POWERPROFILE */
#endif /* POWERPROFILE_C */

/* ============================================================
 * TIME BLOCK
 * ============================================================ */
#ifdef TIME_C

/* Icon shown before the time in icon mode. */
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
 * Plain-ASCII tag used in ASCII mode, printed right before the
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
