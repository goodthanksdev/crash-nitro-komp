// The cheats in the options menu (GameInfo::menu in game_info.cpp): their switches, and what each frame does for the
// ones that write the game's memory. The others are hooks in the game code (src/game/hooks.cpp).
#include <cstdint>
#include <cstring>

#include "core/input.h"
#include "kernel/kernel.h"

namespace cnk {

bool g_cheat_wumpa = false;  // Cheats: Wumpa fruit stays at 10 (juiced weapons)
bool g_cheat_boost = false;  // Cheats: always boosting
}  // namespace cnk
// Cheats: crates roll as if you were last. Read by the crate hook in the game code (src/game/hooks.cpp).
extern "C" bool cnk_cheat_crates = false;
// Cheats: every character, track and kart unlocked while on (src/game/hooks.cpp).
extern "C" bool cnk_cheat_unlock = false;
// Cheats: the AI keeps one speed wherever it is, from the next race (src/game/hooks.cpp).
extern "C" bool cnk_cheat_rubber = false;
namespace cnk {

// The player's Wumpa count: the game's list of karts ([[[0x4B86AC] + 0x18] + 4]: first entry at +8, the end at
// +0x10; each entry's kart at +8, the next entry at +0) holds the human player's kart with flag 2 at +0x38, and its
// count at +0xB0. Only written while it reads as a count (0 to 10), and only for pointers inside the console's memory.
uint32_t PlayerKart() {  // also used by the boost hook (src/game/hooks.cpp)
    auto ok = [](uint32_t p) { return p >= 0x10000 && p < 0x4000000; };
    uint32_t a = kernel::R32(0x4B86AC);
    if (!ok(a)) return 0;
    uint32_t b = kernel::R32(a + 0x18);
    if (!ok(b)) return 0;
    uint32_t list = kernel::R32(b + 4);
    if (!ok(list)) return 0;
    uint32_t end = kernel::R32(list + 0x10);
    uint32_t node = kernel::R32(list + 8);
    for (int i = 0; i < 64 && ok(node) && node != end; i++, node = kernel::R32(node)) {
        uint32_t kart = kernel::R32(node + 8);
        if (ok(kart) && (kernel::R32(kart + 0x38) & 2)) return kart;
    }
    return 0;
}
// The boost cheat is in src/game/hooks.cpp: it chains real boosts through the game's AddBoost.
// Unlocks: the unlockables ([[0x4B8734] + 0x38]: gameprogression/unlockables.csv, count at +0x18) keep a bit each in
// [[unlocks + 0xC] + 4] (the bit count at +8). The cheat sets them all and keeps your own; turning it off puts those
// back (a save made meanwhile keeps everything unlocked until the next one).
static void ApplyUnlocks() {
    static bool kept = false;
    static uint32_t own[8], words = 0, where = 0;
    auto ok = [](uint32_t p) { return p >= 0x10000 && p < 0x4000000; };
    uint32_t g = kernel::R32(0x4B8734), u = ok(g) ? kernel::R32(g + 0x38) : 0, bits = ok(u) ? kernel::R32(u + 0xC) : 0;
    uint32_t w = ok(bits) ? kernel::R32(bits + 4) : 0, n = ok(bits) ? kernel::R32(bits + 8) : 0;
    if (!ok(w) || n == 0 || n > 32 * 8) { kept = false; return; }
    uint32_t nw = (n + 31) / 32;
    if (cnk_cheat_unlock) {
        if (!kept || where != w) { for (uint32_t i = 0; i < nw; i++) own[i] = kernel::R32(w + 4 * i); words = nw; where = w; kept = true; }
        for (uint32_t i = 0; i < nw; i++) kernel::W32(w + 4 * i, i + 1 < nw || n % 32 == 0 ? 0xFFFFFFFFu : (1u << (n % 32)) - 1);
    } else if (kept) {
        if (where == w) for (uint32_t i = 0; i < words; i++) kernel::W32(w + 4 * i, own[i]);
        kept = false;
    }
}
static void ApplyCheats() {
    ApplyUnlocks();
    if (!g_cheat_wumpa) return;
    uint32_t kart = PlayerKart();
    if (!kart) return;
    if (g_cheat_wumpa && kernel::R32(kart + 0xB0) <= 10) kernel::W32(kart + 0xB0, 10);
}

// each frame, inside the ImGui frame (GameInfo::ui)
void Frame(double) { ApplyCheats(); }

// local test input (a file kept out of the repository) can set this to drive the game
void (*g_dev_pad)(int port, input::Pad& p) = nullptr;
void Pad(int port, input::Pad& p) {
    if (g_dev_pad) g_dev_pad(port, p);
}

}  // namespace cnk
