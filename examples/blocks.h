/*
 * Example dwmasyncblocks configuration for statusblocks. Only the blocks
 * array is shown; take the colours table and the remaining settings from
 * dwmasyncblocks' config.def.h.
 *
 * Persistent blocks update themselves the moment something changes, and
 * their interval is ignored:
 *
 *   time       wakes on each minute boundary
 *   date       wakes at midnight
 *   battery    kernel power_supply uevents, rechecked every minute
 *   keyboard   XKB layout events
 *   volume     the sound server
 *   bluetooth  BlueZ adapter signals
 *   updates    recounts once a pacman transaction settles, and every
 *              update_cache_ttl seconds; no pacman hook needed
 *
 * The rest poll:
 *
 *   memory     10  a plain read of /proc/meminfo
 *   internet   10  cheap, and the signal strength moves
 *   kernel      0  changes only on reboot
 *   power       0  a button, never updates on its own
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
	/*Command             Update Interval   Update Signal   Icons   Persist */
	{ BLOCK("internet"),  10,               1,              1,      0 },
	{ BLOCK("bluetooth"), 0,                2,              1,      1 },
	{ BLOCK("memory"),    10,               3,              1,      0 },
	{ BLOCK("keyboard"),  0,                4,              1,      1 },
	{ BLOCK("volume"),    0,                5,              1,      1 },
	{ BLOCK("battery"),   0,                6,              1,      1 },
	{ BLOCK("date"),      0,                7,              1,      1 },
	{ BLOCK("time"),      0,                8,              1,      1 },
	{ BLOCK("updates"),   0,                9,              1,      1 },
	{ BLOCK("kernel"),    0,                11,             1,      0 },
	{ BLOCK("power"),     0,                10,             1,      0 },
};
