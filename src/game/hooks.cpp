// Hooks for default.xbe. A strong definition replaces the recompiled function at that address.
#include "xdk/hle.h"

using namespace kernel;

// The options menu's "best crate items" cheat (src/common/pc_menu.cpp).
extern "C" bool cnk_cheat_crates;

// 0x5B170 rolls what a crate gives the player's kart (the first stack argument; the AI karts roll in 0x5BBF0). Its
// place, [[kart + 0x7C] + 0x18] (1 to 8), picks the row of dda/playerweaponselection.csv's odds. With the cheat the
// kart rolls as if it were last, then gets its real place back.
X86_EXTERN_FUNC(__imp__sub_0005B170);
X86_FUNC(sub_0005B170) {
    uint32_t kart = R32(ctx.esp + 4), place = 0, real = 0;
    if (cnk_cheat_crates && kart) {
        place = R32(kart + 0x7C) + 0x18;
        real = R32(place);
        W32(place, 8);
    }
    __imp__sub_0005B170(ctx, base);
    if (place) W32(place, real);
}
