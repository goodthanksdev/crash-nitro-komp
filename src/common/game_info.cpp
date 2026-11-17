#include "app/app.h"
#include "core/input.h"

// the cheats (cheats.cpp)
namespace cnk {
extern bool g_cheat_wumpa, g_cheat_boost;
void Frame(double fps);
void Pad(int port, input::Pad& p);
}  // namespace cnk
extern "C" bool cnk_cheat_crates, cnk_cheat_unlock, cnk_cheat_rubber;

const app::GameInfo app::kGame = {
    .title = L"Crash Nitro Kart",               // window and splash
    .short_title = L"Crash Nitro Kart",               // message boxes
    .name = L"CrashNitroKart",                      // window classes, crash dumps, handover event
    .log = "cnk",                        // log file prefix
    .settings = "settings.toml",          // settings file beside the exe
    .splash = L"splash",                  // optional splash.png or splash.bmp beside the exe
    .missing = L"Put the game in the assets folder: its disc image (.iso), or default.xbe with its folders beside it.",
    .images = {{"default.xbe", "CrashNitroKart"}},  // each recompiled xbe and its exe, for title launches
    .options = {{"cheat_wumpa", &cnk::g_cheat_wumpa, nullptr}, {"cheat_boost", &cnk::g_cheat_boost, nullptr}, {"cheat_crates", &cnk_cheat_crates, nullptr}, {"cheat_unlock", &cnk_cheat_unlock, nullptr}, {"cheat_rubber", &cnk_cheat_rubber, nullptr}},                        // game settings kept in the settings file
    .load = nullptr,                      // reads more from the settings file's toml::table
    .save = nullptr,                      // writes more into it
    .init = nullptr,                      // after the settings load, before the window and boot
    .loaded = nullptr,                    // image in memory, before its entry point runs
    .window = nullptr,                    // the game window is up
    .message = nullptr,                   // window messages first; true takes one
    .ui = cnk::Frame,                     // each frame: the cheats that write the game's memory
    .pad = cnk::Pad,                      // changes a polled pad (local test input)
    .rumble = nullptr,                    // changes a rumble before it is set
    // keyboard play, rebound in the options menu; the controller layout is the game's layout A
    .keys = {{"left", "Steer left", "Left, A"},
             {"right", "Steer right", "Right, D"},
             {"up", "Menu up", "Up"},
             {"down", "Menu down", "Down"},
             {"a", "Gas / accept", "W, Enter"},
             {"x", "Brake / reverse", "S"},
             {"rt", "Hop", "Space"},
             {"lt", "Boost", "Left Shift"},
             {"b", "Power-up / back", "E, Backspace"},
             {"black", "Rear view", "Q"},
             {"white", "Team frenzy", "F"},
             {"y", "Toggle HUD", "H"},
             {"back", "Change view", "C"},
             {"start", "Pause", "Esc"}},
    .version = "0.1.4",  // the options menu header
    .tier = "beta",      // alpha, beta or complete
    .menu = {{"Cheats", "Infinite Wumpa", "Wumpa fruit stays at 10: juiced power-ups.", &cnk::g_cheat_wumpa, nullptr},
             {"Cheats", "Infinite boost", "Always boosting.", &cnk::g_cheat_boost, nullptr},
             {"Cheats", "Best crate items", "Crates roll as if you were in last place.", &cnk_cheat_crates, nullptr},
             {"Cheats", "Unlock everything", "Every character, track and kart while on.", &cnk_cheat_unlock, nullptr},
             {"Cheats", "No rubberbanding", "The AI keeps one speed wherever it is.", &cnk_cheat_rubber, "Next race"}},
};
