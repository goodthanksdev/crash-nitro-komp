// The PC options menu: F1 or Back + Start opens it. Controller, keyboard (arrows, Enter, Esc) or mouse; every change
// applies at once and is saved to the settings file.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

#include <imgui.h>

#include "app/app.h"
#include "audio/audio.h"
#include "core/input.h"
#include "core/log.h"
#include "kernel/kernel.h"

namespace cnk {

bool g_show_fps = false;  // a settings file option, see game_info.cpp
bool g_cheat_wumpa = false;  // Cheats: Wumpa fruit stays at 10 (juiced weapons)

// Player 1's Wumpa count: the player table at 0x4B86D8 holds each player's kart at +0x28 (20 bytes a player), and the
// count sits 0xDC0 before the kart. Only written while it reads as a count (0 to 10), so a stale kart after a race
// is left alone.
static void ApplyCheats() {
    if (!g_cheat_wumpa) return;
    uint32_t table = kernel::R32(0x4B86D8);
    if (table < 0x10000 || table >= 0x4000000) return;
    uint32_t kart = kernel::R32(table + 0x28);
    if (kart < 0x10000 + 0xDC0 || kart >= 0x4000000) return;
    uint32_t count = kart - 0xDC0;
    if (kernel::R32(count) <= 10) kernel::W32(count, 10);
}

namespace {

enum Cmd { kUp, kDown, kLeft, kRight, kAccept, kBack, kToggle };

std::atomic<bool> g_open{false};
std::mutex g_cmd_lock;
std::vector<Cmd> g_cmds;
const auto g_start = std::chrono::steady_clock::now();

void Push(Cmd c) {
    std::lock_guard lock(g_cmd_lock);
    g_cmds.push_back(c);
}

double Now() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - g_start).count(); }

struct Res {
    uint32_t w, h;
};
std::vector<Res> Resolutions() {
    static std::vector<Res> list;
    if (!list.empty()) return list;
    DEVMODEW m{};
    m.dmSize = sizeof m;
    for (DWORD i = 0; EnumDisplaySettingsW(nullptr, i, &m); i++) {
        if (m.dmPelsWidth < 800 || m.dmPelsHeight < 600) continue;
        Res r{m.dmPelsWidth, m.dmPelsHeight};
        if (std::none_of(list.begin(), list.end(), [&](const Res& o) { return o.w == r.w && o.h == r.h; })) list.push_back(r);
    }
    std::sort(list.begin(), list.end(), [](const Res& a, const Res& b) { return a.w * a.h < b.w * b.h; });
    return list;
}

// one menu row: its name, the value as text, and a step through its choices
struct Row {
    const char* name;
    std::string (*value)();
    void (*step)(int dir);  // null: a button, accept runs press
    void (*press)();
};

template <typename T>
void Cycle(T& v, const std::vector<T>& choices, int dir) {
    size_t i = std::find(choices.begin(), choices.end(), v) - choices.begin();
    if (i >= choices.size()) i = 0;
    else i = (i + choices.size() + dir) % choices.size();
    v = choices[i];
}

void Changed(bool display = false) {
    app::Save();
    if (display) app::ApplyDisplay();
}

const std::vector<uint32_t> kPercents = {50, 75, 100, 125, 150, 200, 300, 400};
const std::vector<uint32_t> kCaps = {30, 60, app::kFpsCapRefresh, 0};
const std::vector<uint32_t> kAniso = {0, 2, 4, 8, 16};

const Row kRows[] = {
    {"Display mode",
     [] {
         const char* n[] = {"Fullscreen", "Borderless window", "Window"};
         return std::string(n[uint32_t(app::Get().display_mode) % 3]);
     },
     [](int dir) {
         uint32_t m = (uint32_t(app::Get().display_mode) + 3 + dir) % 3;
         app::Get().display_mode = app::DisplayMode(m);
         Changed(true);
     },
     nullptr},
    {"Resolution",
     [] {
         const auto& s = app::Get();
         if (!s.width || !s.height) {
             app::Mode d = app::Desktop();
             return "Desktop (" + std::to_string(d.width) + " x " + std::to_string(d.height) + ")";
         }
         return std::to_string(s.width) + " x " + std::to_string(s.height);
     },
     [](int dir) {
         auto& s = app::Get();
         std::vector<Res> list = Resolutions();
         int i = -1;  // -1: desktop
         for (size_t k = 0; k < list.size(); k++)
             if (list[k].w == s.width && list[k].h == s.height) i = int(k);
         i += dir;
         if (i < -1) i = int(list.size()) - 1;
         if (i >= int(list.size())) i = -1;
         s.width = i < 0 ? 0 : list[size_t(i)].w;
         s.height = i < 0 ? 0 : list[size_t(i)].h;
         Changed(true);
     },
     nullptr},
    {"Render resolution",
     [] {
         const auto& s = app::Get();
         uint32_t h = app::Output().height * s.render_percent / 100;
         return std::to_string(s.render_percent) + "%  (" + std::to_string(h) + "p)";
     },
     [](int dir) {
         Cycle(app::Get().render_percent, kPercents, dir);
         Changed();
     },
     nullptr},
    {"Frame rate cap",
     [] {
         uint32_t c = app::Get().fps_cap;
         if (c == 0) return std::string("Unlimited");
         if (c == app::kFpsCapRefresh) return std::string("Monitor refresh rate");
         return std::to_string(c) + " fps";
     },
     [](int dir) {
         Cycle(app::Get().fps_cap, kCaps, dir);
         Changed();
     },
     nullptr},
    {"VSync", [] { return std::string(app::Get().vsync ? "On" : "Off"); },
     [](int) {
         app::Get().vsync = !app::Get().vsync;
         Changed();
     },
     nullptr},
    {"Texture filtering",
     [] {
         uint32_t a = app::Get().aniso;
         return a ? std::to_string(a) + "x anisotropic" : std::string("As the game sets it");
     },
     [](int dir) {
         Cycle(app::Get().aniso, kAniso, dir);
         Changed();
     },
     nullptr},
    {"Audio output",
     [] {
         const char* n[] = {"Automatic", "Stereo", "Surround", "Mono"};
         return std::string(n[app::Get().audio_output % 4]);
     },
     [](int dir) {
         auto& s = app::Get();
         s.audio_output = (s.audio_output + 4 + dir) % 4;
         audio::SetOutputMode(s.audio_output);
         Changed();
     },
     nullptr},
    {"Texture packs", [] { return std::string(app::Get().custom_textures ? "On" : "Off"); },
     [](int) {
         app::Get().custom_textures = !app::Get().custom_textures;
         Changed();
     },
     nullptr},
    {"Show FPS", [] { return std::string(g_show_fps ? "On" : "Off"); },
     [](int) {
         g_show_fps = !g_show_fps;
         Changed();
     },
     nullptr},
    {"Cheat: infinite Wumpa", [] { return std::string(g_cheat_wumpa ? "On" : "Off"); },
     [](int) {
         g_cheat_wumpa = !g_cheat_wumpa;
         Changed();
     },
     nullptr},
    {"Reset to defaults", nullptr, nullptr,
     [] {
         app::Get() = app::Settings{};
         g_show_fps = false;
         g_cheat_wumpa = false;
         audio::SetOutputMode(0);
         Changed(true);
     }},
    {"Back to game", nullptr, nullptr, [] { g_open = false; }},
};
constexpr int kRowCount = int(sizeof kRows / sizeof kRows[0]);
int g_sel = 0;

void Run(Cmd c) {
    const Row& r = kRows[g_sel];
    switch (c) {
        case kToggle: g_open = !g_open; break;
        case kBack: g_open = false; break;
        case kUp: g_sel = (g_sel + kRowCount - 1) % kRowCount; break;
        case kDown: g_sel = (g_sel + 1) % kRowCount; break;
        case kLeft:
        case kRight:
            if (r.step) r.step(c == kLeft ? -1 : 1);
            break;
        case kAccept:
            if (r.step) r.step(1);
            else if (r.press) r.press();
            break;
    }
}

}  // namespace

bool MenuOpen() { return g_open; }

// window thread: F1 toggles, and the keys drive the menu while it is open
bool MenuMessage(void*, uint32_t msg, uintptr_t wp, intptr_t, intptr_t* result) {
    if (msg != WM_KEYDOWN && msg != WM_SYSKEYDOWN) return false;
    if (wp == VK_F1) {
        Push(kToggle);
    } else if (g_open) {
        switch (wp) {
            case VK_UP: Push(kUp); break;
            case VK_DOWN: Push(kDown); break;
            case VK_LEFT: Push(kLeft); break;
            case VK_RIGHT: Push(kRight); break;
            case VK_RETURN:
            case VK_SPACE: Push(kAccept); break;
            case VK_ESCAPE:
            case VK_BACK: Push(kBack); break;
            default: return false;
        }
    } else {
        return false;
    }
    *result = 0;
    return true;
}

// game thread: Back + Start toggles; while open the pad drives the menu and the game sees nothing pressed
void MenuPad(int port, input::Pad& p) {
    if (port != 0) return;
    constexpr uint16_t kUpB = 1, kDownB = 2, kLeftB = 4, kRightB = 8, kStartB = 16, kBackB = 32;
    static uint16_t prev = 0;
    static bool a_prev = false, b_prev = false;
    static double held_since = 0, next_repeat = 0;
    uint16_t b = p.buttons;
    // the left stick counts as the d-pad in the menu
    if (p.ly > 16000) b |= kUpB;
    if (p.ly < -16000) b |= kDownB;
    if (p.lx < -16000) b |= kLeftB;
    if (p.lx > 16000) b |= kRightB;
    bool a = p.analog[0] > 0x80, bb = p.analog[1] > 0x80;
    uint16_t pressed = b & ~prev;
    if ((b & (kStartB | kBackB)) == (kStartB | kBackB) && (pressed & (kStartB | kBackB))) Push(kToggle);
    else if (g_open) {
        uint16_t dirs = b & (kUpB | kDownB | kLeftB | kRightB);
        double now = Now();
        uint16_t fire = pressed & dirs;
        if (dirs && !(prev & dirs)) held_since = now, next_repeat = now + 0.4;
        else if (dirs && now >= next_repeat) fire = dirs, next_repeat = now + 0.12;
        if (fire & kUpB) Push(kUp);
        if (fire & kDownB) Push(kDown);
        if (fire & kLeftB) Push(kLeft);
        if (fire & kRightB) Push(kRight);
        if (a && !a_prev) Push(kAccept);
        if (bb && !b_prev) Push(kBack);
    }
    prev = b;
    a_prev = a;
    b_prev = bb;
    if (g_open) {
        bool connected = p.connected;
        p = input::Pad{};
        p.connected = connected;
    }
}

// render thread, inside the ImGui frame
void MenuUi(double fps) {
    std::vector<Cmd> cmds;
    {
        std::lock_guard lock(g_cmd_lock);
        cmds.swap(g_cmds);
    }
    for (Cmd c : cmds) Run(c);
    ApplyCheats();

    ImGuiIO& io = ImGui::GetIO();
    float scale = std::max(1.0f, io.DisplaySize.y / 720.0f);
    if (g_show_fps) {
        ImGui::SetNextWindowPos({10 * scale, 10 * scale});
        ImGui::SetNextWindowBgAlpha(0.45f);
        ImGui::Begin("##fps", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                           ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
        ImGui::SetWindowFontScale(scale);
        ImGui::Text("%.0f FPS", fps);
        ImGui::End();
    }
    // a hint for the first seconds of a run
    double t = Now();
    if (!g_open && t > 1 && t < 12) {
        float alpha = float(std::min({1.0, t - 1, 12 - t}));
        ImGui::SetNextWindowPos({io.DisplaySize.x - 10 * scale, io.DisplaySize.y - 10 * scale}, 0, {1, 1});
        ImGui::SetNextWindowBgAlpha(0.5f * alpha);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
        ImGui::Begin("##hint", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                            ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
        ImGui::SetWindowFontScale(scale);
        ImGui::TextUnformatted("Options: F1 or Back + Start");
        ImGui::End();
        ImGui::PopStyleVar();
    }
    if (!g_open) return;

    float w = 560 * scale;
    ImGui::SetNextWindowPos({io.DisplaySize.x / 2, io.DisplaySize.y / 2}, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({w, 0});
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10 * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {22 * scale, 18 * scale});
    ImGui::Begin("##options", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetWindowFontScale(scale * 1.15f);
    ImGui::TextColored({1.0f, 0.75f, 0.2f, 1}, "OPTIONS");
    ImGui::SetWindowFontScale(scale);
    ImGui::Separator();
    ImGui::Dummy({0, 6 * scale});
    for (int i = 0; i < kRowCount; i++) {
        const Row& r = kRows[i];
        ImGui::PushID(i);
        if (r.step == nullptr && r.press && i > 0 && kRows[i - 1].step) ImGui::Dummy({0, 8 * scale});
        bool sel = i == g_sel;
        float y = ImGui::GetCursorPosY();
        if (ImGui::Selectable("##row", sel, ImGuiSelectableFlags_AllowOverlap, {0, ImGui::GetTextLineHeight() * 1.5f})) {
            g_sel = i;
            Run(kAccept);
        }
        if (ImGui::IsItemHovered() && io.MouseDelta.x * io.MouseDelta.x + io.MouseDelta.y * io.MouseDelta.y > 0) g_sel = i;
        float pad = ImGui::GetTextLineHeight() * 0.25f;
        ImGui::SetCursorPos({ImGui::GetStyle().WindowPadding.x + 8 * scale, y + pad});
        ImGui::TextColored(sel ? ImVec4{1, 1, 1, 1} : ImVec4{0.75f, 0.78f, 0.82f, 1}, "%s", r.name);
        if (r.value) {
            std::string v = r.value();
            std::string shown = sel && r.step ? "<  " + v + "  >" : v;
            float tw = ImGui::CalcTextSize(shown.c_str()).x;
            ImGui::SetCursorPos({w - ImGui::GetStyle().WindowPadding.x - 8 * scale - tw, y + pad});
            ImGui::TextColored(sel ? ImVec4{1.0f, 0.8f, 0.3f, 1} : ImVec4{0.95f, 0.95f, 0.95f, 1}, "%s", shown.c_str());
            // mouse: right click or wheel steps the other way
            if (sel && ImGui::IsWindowHovered() && r.step) {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) r.step(-1);
                if (io.MouseWheel > 0) r.step(1);
                if (io.MouseWheel < 0) r.step(-1);
            }
        }
        ImGui::SetCursorPosY(y + ImGui::GetTextLineHeight() * 1.5f + ImGui::GetStyle().ItemSpacing.y);
        ImGui::PopID();
    }
    ImGui::Dummy({0, 4 * scale});
    ImGui::Separator();
    ImGui::TextDisabled("Up/Down choose   Left/Right change   B / Esc close");
    ImGui::End();
    ImGui::PopStyleVar(2);
}

}  // namespace cnk
