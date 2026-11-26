/*
 * Example dwmasyncblocks configuration for statusblocks. Only the blocks
 * array is shown; take the colours table and the remaining settings from
 * dwmasyncblocks' config.def.h.
 *
 * Intervals are chosen so that each block costs about what it is worth:
 *
 *   time      60   the clock only shows hours and minutes
 *   date     3600  the day changes once a day; the hourly tick is slack
 *   battery    30  fast enough to notice a drain, cheap to read
 *   memory     10  a plain read of /proc/meminfo
 *   keyboard    0  signal only; nothing changes it but the user
 *   volume      0  signal only; the volume script signals after changing
 *   bluetooth  60  a D-Bus round trip, so not more often
 *   internet   10  cheap, and the signal strength moves
 *   updates   600  the counts are cached anyway, see below
 *   kernel      0  changes only on reboot
 *   power       0  a button, never updates on its own
 *
 * Blocks with interval 0 are updated only on request, e.g.
 * `dwmblocks --update volume` from the volume script.
 *
 * The updates block queries the network, so it caches its counts (see
 * update_cache_ttl in config.h). Pair it with this pacman hook, dropped in
 * /etc/pacman.d/hooks/, so the count drops to zero right after an upgrade
 * rather than at the next interval:
 *
 *   [Trigger]
 *   Operation = Install
 *   Operation = Upgrade
 *   Operation = Remove
 *   Type = Package
 *   Target = *
 *
 *   [Action]
 *   Description = Refresh the statusblocks updates block
 *   When = PostTransaction
 *   Exec = /usr/bin/dwmblocks --update updates
 *
 * The block already detects the transaction on its own by watching
 * update_watch_path, so the hook only needs to make it re-run.
 *
 * The Icons column picks each block's icon (1) or ASCII (0) form; the
 * statusblocks.icon_<block> X resource overrides it at runtime.
 */

#define CLICKABLE_BLOCKS 1
#define LEADING_DELIMITER 1
#define DELIMITER " "
#define TRIM_TRAILING_SPACES 1

#define BLOCK(NAME) "$HOME/.local/bin/statusblocks/" #NAME

const Block blocks[] = {
	/*Command             Update Interval   Update Signal   Icons */
	{ BLOCK("internet"),  10,               1,              1 },
	{ BLOCK("bluetooth"), 60,               2,              1 },
	{ BLOCK("memory"),    10,               3,              1 },
	{ BLOCK("keyboard"),  0,                4,              1 },
	{ BLOCK("volume"),    0,                5,              1 },
	{ BLOCK("battery"),   30,               6,              1 },
	{ BLOCK("date"),      3600,             7,              1 },
	{ BLOCK("time"),      60,               8,              1 },
	{ BLOCK("updates"),   600,              9,              1 },
	{ BLOCK("kernel"),    0,                11,             1 },
	{ BLOCK("power"),     0,                10,             1 },
};
