#include "wii/app.h"

#include <gccore.h>
#include <ogc/lwp_watchdog.h>
#include <wiiuse/wpad.h>

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/launch.h"
#include "core/library.h"
#include "core/recent.h"
#include "core/scanner.h"
#include "core/settings.h"
#include "core/ui_state.h"
#include "wii/boot.h"
#include "wii/fs_wii.h"
#include "wii/gfx.h"
#include "wii/input.h"
#include "wii/screens.h"

namespace app {

namespace {

constexpr char kSettingsPath[] = "sd:/launcher/settings.txt";
constexpr char kRecentPath[] = "sd:/launcher/recent.txt";
// Cover-Budget: ein 2D-Cover (160x228 RGBA8) hat ~146 KB, 16 MB also ~110 Cover. Kleiner als
// die 24 MB der Demo, weil bootDol die Programmdatei (bis 16 MB) noch auf den Heap liest
// (vor bootDol wird der Cache geleert).
constexpr size_t kCoverBudget = 16u * 1024 * 1024;

constexpr uint64_t kRepeatDelayMs = 400;
constexpr uint64_t kRepeatMs = 120;
constexpr float kOverlayFadeMs = 150;
constexpr float kSlideMs = 150;
constexpr float kLeaveFadeMs = 200;
constexpr uint64_t kErrorMs = 3000;
constexpr float kErrorFadeMs = 150;

const char kEllipsis[] = "\xE2\x80\xA6";

uint64_t nowMs() { return ticks_to_millisecs(gettime()); }

// Power/Reset der Konsole und Power der Wiimote: Callbacks laufen im Interrupt bzw. im
// WPAD-Thread und setzen nur Flags, die Hauptschleife wertet sie aus.
volatile bool gPowerRequested = false;
volatile bool gResetRequested = false;

void onPower() { gPowerRequested = true; }
void onWiimotePower(s32 /*chan*/) { gPowerRequested = true; }
void onReset(u32 /*irq*/, void* /*ctx*/) { gResetRequested = true; }

// Wiederholung für gehaltenes Steuerkreuz: erst nach 400 ms, dann alle 120 ms.
class KeyRepeat {
public:
    void update(const InputState& in, uint64_t now, std::vector<Action>& out) {
        for (const Dir& d : kDirs) {
            if (in.pressed & d.button) {
                out.push_back(d.action);
                button_ = d.button;
                action_ = d.action;
                next_ = now + kRepeatDelayMs;
            }
        }
        if (!button_ || (in.pressed & button_)) return;
        if (!(in.held & button_)) {
            button_ = 0;
        } else if (now >= next_) {
            out.push_back(action_);
            next_ = now + kRepeatMs;
        }
    }

private:
    struct Dir {
        uint32_t button;
        Action action;
    };
    // Wiimote zeigt auf den Fernseher (hochkant gehalten): natürliche Zuordnung.
    static constexpr Dir kDirs[4] = {{WPAD_BUTTON_UP, Action::Up},
                                     {WPAD_BUTTON_DOWN, Action::Down},
                                     {WPAD_BUTTON_LEFT, Action::Left},
                                     {WPAD_BUTTON_RIGHT, Action::Right}};
    uint32_t button_ = 0;
    Action action_ = Action::Up;
    uint64_t next_ = 0;
};

class App {
public:
    void run();

private:
    void startup();
    void scanAll(bool usb);
    void rebuildLibrary(std::vector<Game> games);
    void refreshRecent();
    void saveSettings();

    void collectActions(const InputState& in, const screens::Hit& hit, uint64_t now,
                        std::vector<std::pair<Action, screens::Hit>>& out);
    void handle(const Command& c);
    void launch(const Game& g);
    [[noreturn]] void leave(Command::Kind kind);
    void retryUsb();

    void animate(uint64_t now);
    void drawScene(const InputState* in, const screens::Hit& hit, uint64_t now);
    void present();
    void messageFrame(const std::string& title, const std::string& detail = std::string());
    void fadeOut(const InputState* in);

    WiiFs fs_;
    bool sdOk_ = false;
    Settings settings_;
    std::vector<RecentEntry> recentEntries_;
    std::unique_ptr<Library> lib_ = std::make_unique<Library>(std::vector<Game>());
    std::vector<const Game*> recent_;
    UiState state_;
    screens::Covers covers_{kCoverBudget};
    KeyRepeat repeat_;

    screens::Hit lastHit_;
    Overlay shownOverlay_ = Overlay::None;
    float overlayAlpha_ = 0;
    float slideFrom_ = 0;
    uint64_t slideStart_ = 0;
    std::string errorTitle_, errorDetail_;
    uint64_t errorStart_ = 0, errorUntil_ = 0;
    uint64_t lastFrame_ = 0;
};

void App::run() {
    if (!gfx::init()) return;
    startup();

    std::vector<std::pair<Action, screens::Hit>> actions;
    lastFrame_ = nowMs();
    while (true) {
        // Power wie Home -> "Ausschalten", Reset wie Home -> "Wii-Menü" (mit Priiloader-Magic).
        if (gPowerRequested) leave(Command::PowerOff);
        if (gResetRequested) leave(Command::WiiMenu);

        uint64_t now = nowMs();
        InputState in = pollInput();
        screens::Hit hit;
        if (in.pointerValid) hit = screens::hitTest(in.pointerX, in.pointerY, state_, *lib_, recent_);
        // Hover nur bei Wechsel der Region, sonst überschreibt der ruhende Zeiger das Steuerkreuz.
        // Während der Fehlermeldung kein Hover (sie sperrt die Eingabe).
        if (in.pointerValid && hit != lastHit_ && now >= errorUntil_) pointerHover(state_, *lib_, hit);
        lastHit_ = in.pointerValid ? hit : std::nullopt;

        actions.clear();
        if (now < errorUntil_) {
            // Fehlermeldung: A oder B schließt sie vorzeitig, sonst keine Eingabe.
            if (in.pressed & (WPAD_BUTTON_A | WPAD_BUTTON_B)) errorUntil_ = std::min(errorUntil_, now + 1);
        } else {
            collectActions(in, hit, now, actions);
        }
        for (const auto& a : actions) {
            int carouselBefore = state_.carouselIndex;
            Command c = apply(state_, *lib_, recent_, a.first, a.second);
            if (state_.carouselIndex != carouselBefore) {
                slideFrom_ = state_.carouselIndex > carouselBefore ? 1.0f : -1.0f;
                slideStart_ = now;
            }
            handle(c);
        }

        animate(now);
        drawScene(&in, hit, now);
        present();
    }
}

void App::startup() {
    SYS_SetPowerCallback(onPower);
    SYS_SetResetCallback(onReset);
    WPAD_Init();
    WPAD_SetPowerButtonCallback(onWiimotePower);
    inputInit(gfx::width(), gfx::height());
    covers_.loadLogos();

    sdOk_ = fs_.mountSd();
    std::string text;
    if (sdOk_ && fs_.readText(kSettingsPath, text)) settings_ = parseSettings(text);
    state_.systems = settings_.systems;
    lib_->setEnabled(state_.systems);

    // mountUsb blockiert bis zu 10 x 500 ms; das zuletzt gezeigte Bild bleibt so lange stehen.
    messageFrame(std::string("Suche USB-Platte ") + kEllipsis);
    bool usb = fs_.mountUsb(10, 500);
    if (sdOk_ && fs_.readText(kRecentPath, text)) recentEntries_ = parseRecent(text);
    scanAll(usb);
}

// usb:/ plus settings.extraRoot (z. B. sd:/launcher/mock für Dolphin). Ohne USB und ohne
// extraRoot: Fehlerbild "USB-Platte nicht gefunden".
void App::scanAll(bool usb) {
    if (usb || !settings_.extraRoot.empty()) messageFrame(std::string("Lese Spieleliste ") + kEllipsis);
    std::vector<Game> games;
    if (usb) games = scanLibrary(fs_, "usb:/");
    if (!settings_.extraRoot.empty()) {
        std::vector<Game> extra = scanLibrary(fs_, settings_.extraRoot);
        games.insert(games.end(), std::make_move_iterator(extra.begin()), std::make_move_iterator(extra.end()));
    }
    state_.noUsb = !usb && settings_.extraRoot.empty();
    rebuildLibrary(std::move(games));
}

void App::rebuildLibrary(std::vector<Game> games) {
    // Alle Zeiger in die alte Library verwerfen, bevor sie zerstört wird.
    state_.carouselPreview = nullptr;
    recent_.clear();
    lib_ = std::make_unique<Library>(std::move(games));
    lib_->setEnabled(state_.systems);
    lib_->setTab(std::nullopt);
    state_.tabIndex = 0;
    state_.listIndex = 0;
    state_.letterIndex = 0;
    state_.carouselIndex = 0;
    refreshRecent();
}

void App::refreshRecent() {
    recent_ = recentForLibrary(recentEntries_, *lib_);
    // apply() klemmt den Index erst bei der nächsten Eingabe; bis dahin bliebe die Mitte leer.
    int count = static_cast<int>(recent_.empty() ? lib_->visible().size() : recent_.size());
    state_.carouselIndex = std::max(0, std::min(state_.carouselIndex, count - 1));
}

void App::saveSettings() {
    settings_.systems = state_.systems;
    if (sdOk_) fs_.writeText(kSettingsPath, serializeSettings(settings_));
}

void App::collectActions(const InputState& in, const screens::Hit& hit, uint64_t now,
                         std::vector<std::pair<Action, screens::Hit>>& out) {
    std::vector<Action> dpad;
    repeat_.update(in, now, dpad);
    for (Action a : dpad) out.emplace_back(a, std::nullopt);

    if (in.pressed & WPAD_BUTTON_A) {
        // Regel für A (Entscheidung des Leads):
        // - Zeiger gültig: A wirkt nur auf das Ziel des Zeigers. Bedienelement -> PointerClick.
        //   Außerhalb eines offenen Overlay-Panels (Region::None) -> PointerClick, schließt es.
        //   Leere Panelfläche (nullopt) oder Hintergrund des Hauptbilds (Region::None ohne
        //   Overlay) -> gar nichts, auch kein Action::A.
        // - Zeiger ungültig: Action::A auf die Auswahl des Steuerkreuzes.
        // - Ausnahme "USB-Platte nicht gefunden" ohne Overlay: A ist immer Retry (der Bildschirm
        //   verspricht "A: Erneut suchen"), egal wohin der Zeiger zeigt.
        if (!in.pointerValid || (state_.noUsb && state_.overlay == Overlay::None)) {
            out.emplace_back(Action::A, std::nullopt);
        } else if (hit && (hit->first != static_cast<int>(Region::None) || state_.overlay != Overlay::None)) {
            out.emplace_back(Action::PointerClick, hit);
        }
    }
    const std::pair<uint32_t, Action> buttons[] = {{WPAD_BUTTON_B, Action::B},     {WPAD_BUTTON_PLUS, Action::Plus},
                                                   {WPAD_BUTTON_MINUS, Action::Minus}, {WPAD_BUTTON_1, Action::One},
                                                   {WPAD_BUTTON_2, Action::Two},   {WPAD_BUTTON_HOME, Action::Home}};
    for (const auto& b : buttons)
        if (in.pressed & b.first) out.emplace_back(b.second, std::nullopt);
}

void App::handle(const Command& c) {
    switch (c.kind) {
        case Command::None: break;
        case Command::Launch:
            if (c.game) launch(*c.game);
            break;
        case Command::ToggleSystem:
            // apply hat die Library schon umgeschaltet; nur speichern und Karussell neu filtern.
            saveSettings();
            refreshRecent();
            break;
        case Command::WiiMenu:
        case Command::Hbc:
        case Command::PowerOff: leave(c.kind);
        case Command::Retry: retryUsb(); break;
    }
}

void App::launch(const Game& g) {
    RecentEntry e;
    e.timestamp = static_cast<int64_t>(std::time(nullptr));
    e.system = g.system;
    e.dir = g.dir;
    e.file = g.file;
    pushRecent(recentEntries_, e);
    if (sdOk_) fs_.writeText(kRecentPath, serializeRecent(recentEntries_));
    saveSettings();

    const std::string title = g.title;
    fadeOut(nullptr);
    messageFrame("Starte " + title + " " + kEllipsis);
    // Cover freigeben: bootDol liest die Programmdatei (bis 16 MB) auf den Heap.
    covers_.clear();

    LaunchPlan plan = makeLaunchPlan(g, fs_);
    bootDol(plan, fs_);  // kehrt nur bei Fehlern zurück, Mounts und Video sind dann intakt

    uint64_t now = nowMs();
    errorTitle_ = "Konnte " + title + " nicht starten";
    if (plan.dolPath.empty())
        errorDetail_ = "Kein Startprogramm bekannt";
    else if (!fs_.exists(plan.dolPath))
        errorDetail_ = "Fehlt auf der SD: " + plan.dolPath;
    else
        errorDetail_ = "Nicht lesbar oder kein g\xC3\xBCltiges Programm: " + plan.dolPath;
    errorStart_ = now;
    errorUntil_ = now + kErrorMs;
    // Karussell zeigt das gerade gewählte Spiel (jetzt vorn in recent): Vorschau weg,
    // offenes Overlay (z. B. Suche) zu.
    state_.carouselPreview = nullptr;
    state_.overlay = Overlay::None;
    state_.carouselIndex = 0;
    refreshRecent();
    lastFrame_ = now;
}

void App::leave(Command::Kind kind) {
    fadeOut(nullptr);
    covers_.release();
    gfx::shutdown();
    fs_.unmountAll();
    switch (kind) {
        case Command::WiiMenu: returnToWiiMenu(); break;
        case Command::Hbc: launchHbc(); break;
        default: powerOff(); break;
    }
    // Nur bei Fehler hier: Video und Mounts sind abgebaut, also ausschalten.
    powerOff();
    while (true) VIDEO_WaitVSync();
}

void App::retryUsb() {
    messageFrame(std::string("Suche USB-Platte ") + kEllipsis);
    if (!fs_.mountUsb(3, 500)) return;
    scanAll(true);
    covers_.forgetFailures();
    lastFrame_ = nowMs();
}

void App::animate(uint64_t now) {
    float dt = static_cast<float>(std::min<uint64_t>(now - lastFrame_, 50));
    lastFrame_ = now;
    if (state_.overlay != Overlay::None) {
        shownOverlay_ = state_.overlay;  // Wechsel zwischen Overlays ohne erneutes Einblenden
        overlayAlpha_ = std::min(1.0f, overlayAlpha_ + dt / kOverlayFadeMs);
    } else {
        overlayAlpha_ = std::max(0.0f, overlayAlpha_ - dt / kOverlayFadeMs);
        if (overlayAlpha_ <= 0) shownOverlay_ = Overlay::None;
    }
}

void App::drawScene(const InputState* in, const screens::Hit& hit, uint64_t now) {
    gfx::beginFrame(screens::kBg);
    if (state_.noUsb) {
        screens::drawNoUsb(state_.overlay == Overlay::None ? hit : std::nullopt);
    } else {
        float slide = 0;
        if (slideFrom_ != 0) {
            float t = static_cast<float>(now - slideStart_) / kSlideMs;
            if (t >= 1) slideFrom_ = 0;
            else slide = slideFrom_ * (1 - t) * (1 - t);  // abbremsen
        }
        screens::drawMain(state_, *lib_, recent_, covers_, state_.overlay == Overlay::None ? hit : std::nullopt,
                          slide);
    }
    float a = overlayAlpha_;
    screens::drawOverlay(shownOverlay_, state_, *lib_, a * (2 - a));  // weich auslaufend
    if (now < errorUntil_ + static_cast<uint64_t>(kErrorFadeMs) && errorUntil_ != 0) {
        float in_ = std::min(1.0f, static_cast<float>(now - errorStart_) / kErrorFadeMs);
        float out = now < errorUntil_ ? 1.0f : 1.0f - static_cast<float>(now - errorUntil_) / kErrorFadeMs;
        screens::drawMessagePanel(errorTitle_, errorDetail_, std::min(in_, out));
    }
    if (in && in->pointerValid) screens::drawPointer(in->pointerX, in->pointerY);
}

void App::present() {
    gfx::endFrame();
    covers_.endFrame();
}

void App::messageFrame(const std::string& title, const std::string& detail) {
    gfx::beginFrame(screens::kBg);
    screens::drawCenteredMessage(title, detail);
    present();
}

// Aktuelles Bild 200 ms lang nach Schwarz ausblenden (vor Spielstart und Verlassen).
void App::fadeOut(const InputState* in) {
    uint64_t start = nowMs();
    while (true) {
        uint64_t now = nowMs();
        float t = static_cast<float>(now - start) / kLeaveFadeMs;
        drawScene(in, std::nullopt, now);
        screens::drawFade(std::min(1.0f, t));
        present();
        if (t >= 1) break;
    }
}

}  // namespace

void run() {
    // Groß (Covers, Library), daher nicht auf dem Stack.
    static App app;
    app.run();
}

}  // namespace app
