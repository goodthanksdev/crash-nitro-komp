#include "app/app.h"
#include "core/input.h"

// the PC options menu (pc_menu.cpp)
namespace cnk {
extern bool g_show_fps;
bool MenuMessage(void* hwnd, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* result);
void MenuPad(int port, input::Pad& p);
void MenuUi(double fps);
}  // namespace cnk

const app::GameInfo app::kGame = {
    .title = L"Crash Nitro Kart",               // window and splash
    .short_title = L"Crash Nitro Kart",               // message boxes
    .name = L"CrashNitroKart",                      // window classes, crash dumps, handover event
    .log = "cnk",                        // log file prefix
    .settings = "settings.toml",          // settings file beside the exe
    .splash = L"splash",                  // optional splash.png or splash.bmp beside the exe
    .missing = L"Put the game in the assets folder: its disc image (.iso), or default.xbe with its folders beside it.",
    .images = {{"default.xbe", "CrashNitroKart"}},  // each recompiled xbe and its exe, for title launches
    .options = {{"show_fps", &cnk::g_show_fps, nullptr}},                        // game settings kept in the settings file
    .load = nullptr,                      // reads more from the settings file's toml::table
    .save = nullptr,                      // writes more into it
    .init = nullptr,                      // after the settings load, before the window and boot
    .loaded = nullptr,                    // image in memory, before its entry point runs
    .window = nullptr,                    // the game window is up
    .message = cnk::MenuMessage,                   // window messages first; true takes one
    .ui = cnk::MenuUi,                        // ImGui drawing each frame
    .pad = cnk::MenuPad,                  // changes a polled pad (the options menu takes it while open)
    .rumble = nullptr,                    // changes a rumble before it is set
};
