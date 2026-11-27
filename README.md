# statusblocks

A set of small programs, each printing one part of a status line, for
[dwmasyncblocks](https://github.com/dimgerasimou/dwmasyncblocks). Each block
prints its text in both an icon and a plain-ASCII form; dwmasyncblocks picks one
and colours it, so the blocks know nothing about the bar.

They are clickable and configured in a single `config.h`.

## Blocks

| Block | Shows | Clicks | Needs |
|---|---|---|---|
| `time` | `HH:MM` | — | — |
| `date` | day and month | **L** notification calendar, current day marked | — |
| `battery` | charge level and state | **L** notification with capacity, status, power profile and GPU | — |
| `memory` | memory in use | **R** task manager | htop |
| `updates` | pending updates, hidden when there are none | **L** notification with per-source counts · **R** system upgrade | package manager |
| `kernel` | kernel release | — | — |
| `volume` | volume, muted state | **L** sink and source info · **M** equalizer · **R** mute · **scroll** volume ±5% | libpulse |
| `keyboard` | current layout | **L** next layout | — |
| `bluetooth` | adapter state | **L** TUI · **M** toggle adapter | bluez, bluetuith |
| `internet` | connection type, wifi strength | **L** MAC, IPv4, gateway, IPv6, SSID, signal · **R** menu: toggle wifi, connect, TUI | NetworkManager, xmenu |
| `power` | a button | **L** menu: shutdown, reboot, logout, lock, restart the bar, power and GPU profiles | xmenu, slock |

The wifi connect entry needs a script of your own and defaults to a helper from
[dimgerasimou/binaries](https://github.com/dimgerasimou/binaries); any executable
taking the same arguments works. A missing program does not break the block, only
the click. Volume and layout changes are done by the blocks themselves.

The `battery` block finds the battery itself, no path to configure. The `updates`
block caches its update counts, so `checkupdates` and `paru` do not run on every
refresh, and notices a package transaction immediately (see below). The calendar
takes its month and day names from the locale.

## Requirements

`pkg-config`, `libnotify`, and a nerd font in your bar. Per block:
`libpulse` (volume), `libxkbfile` and `libX11` (keyboard), `dbus` (bluetooth), `libnm` and
`glib` (internet). Each block links only what it uses, so a missing library stops
that block and nothing else.

Linux only: the blocks read `/proc` and `/sys`.

## Install

```sh
cp config.def.h config.h   # optional, make does it for you
$EDITOR config.h
make
make install
```

Installs to `~/.local/bin/statusblocks`. Override `PREFIX`, `BLOCKDIR` or
`DESTDIR` as usual. `make time` builds one block; `BLOCKS="time date battery"`
limits the set.

`examples/blocks.h` has a working bar configuration with sensible intervals and
signals for every block.

## Output, colours and icons

Each block prints one line of tab-separated fields:

```
class=bat_nrm<TAB>rich=<glyph><TAB>plain=B:87%
```

`rich` is the icon form, `plain` the ASCII one, and `class` the colour class.
Colours, the class table and the per-block choice between icon and ASCII form
all live in dwmasyncblocks: its `config.h` holds the defaults, and X resources
override them at runtime, e.g.

```
statusblocks.clr_bat_crt:   #F38BA8
statusblocks.icon_battery:  0
```

Changes made with `xrdb` show up immediately. The `date` block reads its
calendar accent from `BLOCK_CLR_CAL`, which dwmasyncblocks exports, and falls
back to `cal_clr_fallback` in `config.h`.

Classes: `bat_crt` `bat_low` `bat_nrm` `bat_chg` `bt` `date` `net_nrm`
`net_err` `sys_pkg` `sys_nrm` `kbd` `mem` `pwr` `tim` `vol_nrm` `vol_mut`, and
`cal` for the calendar.

## Configuration

Everything is in `config.h`. It is split by block: each section is guarded by a
macro only that block defines, which is why names such as `path_wifi_connect`
appear more than once without clashing. A setting placed in the wrong section
is ignored silently, so keep each one under the block that reads it.

Icons, menu entries, commands and paths are all there. Paths accept `~` and
`$VAR`. Settings that need an external program are marked `Requires:`.

Three feature toggles, on by default. Comment them out if you do not have the
programs:

- `POWERPROFILE` adds [powerprofile](https://github.com/dimgerasimou/powerprofile) to the power menu and the battery notification. Profiles are applied through `sudo -n`, so each one needs a line in `/etc/sudoers.d/powerprofile`
- `GPU_PROFILE` adds GPU switching to the power menu and the battery notification; `GPU_PROFILE_BACKEND` picks xgpuprofile or optimus-manager
- `CLIPBOARD` adds clipmenu entries to the power menu

## Package updates

The `updates` block counts updates from two configurable sources, so it is not
tied to pacman. Set `cmd_updates_primary`, `cmd_updates_secondary` and
`update_watch_path`; apt, dnf and xbps equivalents are in the comments.

Counts are cached for `update_cache_ttl` seconds, and invalidated as soon as
`update_watch_path` changes. Run persistently, the block watches that path
itself and recounts once a transaction settles, so the count drops right after
an upgrade with no hook; it also recounts every `update_cache_ttl` seconds,
including time spent suspended. Run on an interval, pair it with this hook so
the count drops right after an upgrade instead of at the next tick:

```ini
[Trigger]
Operation = Install
Operation = Upgrade
Operation = Remove
Type = Package
Target = *

[Action]
Description = Refresh the statusblocks updates block
When = PostTransaction
Exec = /usr/bin/dwmblocks --update updates
```

A left click always refreshes, cache or not.

## Hacking

Blocks print through `emit(class, rich, plain)` in `utils.h`, which takes care
of the field format. They never abort while drawing: a failure warns on stderr
and prints a placeholder, because a block that exits silently leaves stale text in the bar.
Run a block by hand in a terminal to see those warnings, dwmblocks discards them.

`volume`, `keyboard`, `time`, `date`, `battery`, `bluetooth`, `updates` and
`internet` can also run persistently (set `Persist` for them in dwmasyncblocks' `config.h`).
With `BLOCK_PERSIST=1` they stay running and print a line on every change:
volume subscribes to the sound server, keyboard waits for XKB layout events,
time and date sleep until the next minute and midnight, battery listens for
kernel `power_supply` uevents (and rechecks every minute, since not every
battery reports capacity steps), bluetooth follows BlueZ's adapter signals,
updates watches the package database and its own cache, and internet keeps one
NetworkManager client and follows the primary connection, the Wi-Fi access
point and its signal strength. Use `persistent()` from `utils.h` to add the same mode to a block;
`emit()` already skips unchanged lines. Exit on an unrecoverable error, such
as a lost connection, and dwmblocks restarts the block. Clicks still arrive
as a separate one-shot run, so keep the button handling in that path.

Clicks are a table:

```c
static const struct Button buttons[] = {
	{ 1, on_left },
	{ 3, on_right },
};

dispatch(buttons, LEN(buttons), NULL);
```

## License

GPLv3. See [LICENSE](./LICENSE).
