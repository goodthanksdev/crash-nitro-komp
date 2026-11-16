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

// The options menu's "no rubberbanding" cheat.
extern "C" bool cnk_cheat_rubber;

// The rubberbanding object, kept from its loader below.
static uint32_t g_rubber;

// 0x5C830 loads dda/rubberbanding.csv into the rubberbanding object (edi) as each race starts. Its four lists (count
// at +0x1C, at +0x20, +0x24, +0x28, +0x2C) are speed ranges by row; 0x5CB60 copies one row to +0x30 to +0x3C.
X86_EXTERN_FUNC(__imp__sub_0005C830);
X86_FUNC(sub_0005C830) {
    g_rubber = ctx.edi;
    __imp__sub_0005C830(ctx, base);
}

// 0x5E820 is the catch-up ("DDA") update: every so often one of its parts sets the AI karts' target speed (kart +
// 0x1F4) from where they are against the player: slower ahead and faster behind (rubberbanding.csv), close to the
// player's own speed for "buddy" karts (buddyrubberbanding.csv), and more. With the cheat every AI kart gets the
// middle of the race's rubberbanding row instead, wherever it is.
X86_EXTERN_FUNC(__imp__sub_0005E820);
X86_FUNC(sub_0005E820) {
    uint32_t dda = R32(ctx.esp + 4);  // the catch-up object; +0xC the list of karts its parts set speeds for
    __imp__sub_0005E820(ctx, base);
    if (!cnk_cheat_rubber || !g_rubber) return;
    auto get = [](uint32_t va) { uint32_t v = R32(va); float f; std::memcpy(&f, &v, 4); return f; };
    float speed = (get(g_rubber + 0x30) + get(g_rubber + 0x34) + get(g_rubber + 0x38) + get(g_rubber + 0x3C)) / 4;
    if (!(speed > 5.0f && speed < 100.0f)) return;
    uint32_t v;
    std::memcpy(&v, &speed, 4);
    auto ok = [](uint32_t p) { return p >= 0x10000 && p < 0x4000000; };
    uint32_t list = ok(dda) ? R32(dda + 0xC) : 0;
    if (!ok(list)) return;
    uint32_t end = R32(list + 0x10), node = R32(list + 8);
    for (int n = 0; n < 64 && ok(node) && node != end; n++, node = R32(node)) {
        uint32_t kart = R32(node + 8);
        if (!ok(kart) || (R32(kart + 0x38) & 2)) continue;
        float now = get(kart + 0x1F4);  // only over a speed: anything else there is not a racing kart's
        if (now >= 0.0f && now < 100.0f) W32(kart + 0x1F4, v);
    }
}
