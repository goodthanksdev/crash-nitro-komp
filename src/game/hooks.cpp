// Hooks for default.xbe. A strong definition replaces the recompiled function at that address.
#include <cstring>

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


// Infinite boost: the options menu's switch, and the player's kart (both in src/common/pc_menu.cpp).
namespace cnk {
extern bool g_cheat_boost;
uint32_t PlayerKart();
}  // namespace cnk

// 0x6D1A0 runs every frame for a kart's boost object (edi; the kart's +0x78): it counts the boost time at +0x4C8 down.
// With the cheat, the player's boost is topped up before it runs out by the game's own AddBoost (0x6CF80: ecx the
// boost object, eax the boost type, one float on the stack, ret 4), as a chained top-level slide boost (type 5) would.
// Going through AddBoost keeps the boost counter, the streak and the effects right.
X86_EXTERN_FUNC(__imp__sub_0006D1A0);
X86_EXTERN_FUNC(sub_0006CF80);
X86_FUNC(sub_0006D1A0) {
    if (cnk::g_cheat_boost) {
        uint32_t kart = cnk::PlayerKart();
        if (kart && ctx.edi == R32(kart + 0x78)) {
            uint32_t t = R32(ctx.edi + 0x4C8);
            float left;
            std::memcpy(&left, &t, 4);
            if (left < 0.25f) {
                uint32_t eax = ctx.eax, ecx = ctx.ecx, edx = ctx.edx, esp = ctx.esp;
                ctx.ecx = ctx.edi;
                ctx.eax = 5;
                X86_PUSH32(0x3F800000u);  // 1.0
                X86_PUSH32(0x6D271u);     // a return address inside the update, as the game's own call has
                sub_0006CF80(ctx, base);
                ctx.eax = eax; ctx.ecx = ecx; ctx.edx = edx; ctx.esp = esp;
            }
        }
    }
    __imp__sub_0006D1A0(ctx, base);
}
