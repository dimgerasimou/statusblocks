# statusblocks

A set of small programs, each printing one part of a status line. Meant for
[dwmblocks](https://github.com/dimgerasimou/dwmasyncblocks), but they are plain
executables that write a line to stdout, so any status bar that can run a command
will do.

They are coloured, clickable, and configured in a single `config.h`.

## Blocks

| Block | Shows | Clicks | Needs |
|---|---|---|---|
| `time` | `HH:MM` | — | — |
| `date` | day and month | **L** notification calendar, current day marked | — |
| `battery` | charge level and state | **L** notification with capacity and status | — |
| `memory` | memory in use | **R** task manager | htop |
| `system` | pending updates and kernel release | **L** notification with per-source counts · **R** system upgrade | package manager |
| `volume` | volume, muted state | **L** sink and source info · **M** equalizer · **R** mute · **scroll** volume | libpulse, a volume script |
| `keyboard` | current layout | **L** next layout | a layout script |
| `bluetooth` | adapter state | **L** TUI · **M** toggle adapter | bluez, bluetuith |
| `internet` | connection type, wifi strength | **L** MAC, IPv4, gateway, IPv6, SSID, signal · **R** menu: toggle wifi, connect, TUI | NetworkManager, xmenu |
| `power` | a button | **L** menu: shutdown, reboot, logout, lock, restart the bar | xmenu, slock |

Blocks that need a script of your own (volume, keyboard, wifi connect) default to
helpers from [dimgerasimou/binaries](https://github.com/dimgerasimou/binaries).
Any executable taking the same arguments works: a `wpctl` or `setxkbmap` wrapper
is fine. A missing program does not break the block, only the click.

The `battery` block finds the battery itself, no path to configure. The `system`
block caches its update counts, so `checkupdates` and `paru` do not run on every
refresh, and notices a package transaction immediately (see below). The calendar
takes its month and day names from the locale.

## Requirements

`pkg-config`, `libnotify`, `libX11`, and a nerd font in your bar. Per block:
`libpulse` (volume), `libxkbfile` (keyboard), `dbus` (bluetooth), `libnm` and
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

## Colours

Defaults live in `config.h` and work out of the box. Anything in the X resource
database overrides them without a rebuild:

```
statusblocks.clr_bat_crt:  #F38BA8
statusblocks.clr_date:     #CBA6F7
statusblocks.clr_tim:      #FAB387
statusblocks.clr_cal:      #F38BA8
```

Names are the `enum Color` entries in `src/include/colors.h`. Values must be
`#RRGGBB`. Colours are cached in `$XDG_RUNTIME_DIR` and refreshed when
`xresources_path` (see below) changes, so only the first block of a session
talks to X.

## Icon style

Every block's icon can be switched, per block, between its compiled-in glyph
(usually a Nerd Font icon) and a plain-ASCII fallback — no nerd font, no
rebuild required. Defaults live in `icon_defaults` in `config.h`; anything in
the X resource database overrides them without a rebuild, e.g.

```
statusblocks.icon_bat:  0
statusblocks.icon_net:  0
```

`1` selects the icon, `0` the fallback. Names are the `enum Toggle` entries
in `src/include/toggle.h`. For `date`, `keyboard`, `memory`, `time` and
`system`, the fallback is simply no icon, since the text that follows it
already says what the block is. For `battery`, `bluetooth`, `internet`,
`power` and `volume` the icon *is* the block's whole output, so the fallback
carries real information instead of a lookalike glyph:

| Block | Icon fallback |
|---|---|
| `battery` | `B:87%`, or `CHG` while charging, or `B:--` with no battery |
| `volume` | `V:87%`, or `M:87%` while muted |
| `internet` | `x` no connection · `eth` wired · `I`…`IIIII` wifi signal bars · `!` error |
| `bluetooth` | `BT-` off · `BT+` on |
| `power` | `PWR` |

The battery and volume tags (`ascii_bat_tag`, `ascii_vol_tag`, ...) are
configurable in `config.h`, same as everything else. Toggles are cached
alongside colours in `$XDG_RUNTIME_DIR` and refreshed the same way.

## Cache invalidation

Colours and icon toggles are resolved from X once per session and cached in
`$XDG_RUNTIME_DIR`. The cache is rebuilt whenever `xresources_path` in
`config.h` has a newer mtime than the cache, which is what makes "edit the
file, run `xrdb -merge`" pick up immediately instead of waiting for the next
login. It defaults to `~/.Xresources`; point it at whatever file you actually
edit and reload if that's not it, e.g. `~/.config/xresources/Xresources`.
Editing the file alone is enough to invalidate the cache — the actual values
still come from the X server's live resource database, so an edit needs an
`xrdb -merge` (or equivalent) to take effect, not just a save.

### Other status bars

The colour escape is two macros in `config.h`:

| Bar | `CLR_FMT` | `CLR_NRM` |
|---|---|---|
| dwm + status2d | `"^c%s^"` | `"^d^"` |
| Pango: waybar, i3bar | `"<span color='%s'>"` | `"</span>"` |
| polybar | `"%%{F%s}"` | `"%%{F-}"` |
| none | `"%.0s"` | `""` |

Or build with `-DNO_COLOR` to drop colour and the X dependency entirely.

## Configuration

Everything is in `config.h`. It is split by block: each section is guarded by a
macro only that block defines, which is why names such as `path_wifi_connect`
appear more than once without clashing. A setting placed in the wrong section
is ignored silently, so keep each one under the block that reads it.

Icons, menu entries, commands and paths are all there. Paths accept `~` and
`$VAR`. Settings that need an external program are marked `Requires:`.

Two feature toggles, on by default. Comment them out if you do not have the
programs:

- `POWER_MANAGEMENT` adds optimus-manager to the power menu and the battery notification
- `CLIPBOARD` adds clipmenu entries to the power menu

## Package updates

The `system` block counts updates from two configurable sources, so it is not
tied to pacman. Set `cmd_updates_primary`, `cmd_updates_secondary` and
`update_watch_path`; apt, dnf and xbps equivalents are in the comments.

Counts are cached for `update_cache_ttl` seconds, and invalidated as soon as
`update_watch_path` changes. That is how the count drops to zero right after an
upgrade instead of at the next tick:

```ini
[Trigger]
Operation = Install
Operation = Upgrade
Operation = Remove
Type = Package
Target = *

[Action]
Description = Refresh the statusblocks system block
When = PostTransaction
Exec = /usr/bin/pkill -RTMIN+9 dwmblocks
```

A left click always refreshes, cache or not.

## Hacking

Blocks never abort while drawing: a failure warns on stderr and prints a
placeholder, because a block that exits silently leaves stale text in the bar.
Run a block by hand in a terminal to see those warnings, dwmblocks discards them.

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
