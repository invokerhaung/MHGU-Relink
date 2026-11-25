// [Supply Box] NoWait: each item taken from a quest's Supply Box goes to the pouch at once, and the next one can be
// taken right away. The game's rules stay: the pouch's room and each item's maximum, the shared box online (the host
// grants each item), the same sounds and messages. Only the icon's flight to the pouch, which kept the box from taking
// the next item, is cut.
//
// The game's Supply Box window, cUIOSupplyBox (vtable 0x1977214; its update 0x83a6b0), flies the item's icon to the
// pouch while there is one on its way: its first update there sets +0x50 = the flight's time, +0x4c = 20 (a float: the
// game's frames) and +0x54 = 1; each update takes the frame's time off +0x4c, and once it is spent the game ends the
// flight its own way and sets +0x54 = 0. Those 20 frames are the wait between items.
//
// Hooked here: 0x83a6b0. Before the game's own update, with an icon on its way (+0x54 = 1), +0x4c is spent, so that
// same update ends the flight the game's way. Nothing else is touched.
#include "../config.h"
#include "../hle.h"
#include "../platform.h"

#define FN_UI_UPDATE 0x83a6b0u  // cUIOSupplyBox's update (r0: the window)
#define VT_WINDOW    0x1977214u  // cUIOSupplyBox's vtable
#define UI_WAIT      0x4cu       // float: the frames left before the item's flight ends (20 when it starts)
#define UI_FLIGHT    0x54u       // u8: 0 no flight started, 1 an icon on its way to the pouch

static bool sane(uint32_t a) { return a >= 0x08000000u && a < 0xF0000000u && !(a & 3); }

static void ui_update(Cpu *c) {
    uint32_t w = c->r[0];
    if (sane(w) && LD32(w) == VT_WINDOW && config_live_supply_wait()) {
        if (LD8(w + UI_FLIGHT) == 1 && LDF(w + UI_WAIT) > 0) STF(w + UI_WAIT, 0.0f);
    }
    fnhook_call_original(c, FN_UI_UPDATE);
}

void supplybox_init(void) {
    fnhook_set(FN_UI_UPDATE, hle_register("supplybox:ui_update", ui_update));
}
