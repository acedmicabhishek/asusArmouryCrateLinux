#include "keybind.h"
#include "keyboard.h"
#include "modes.h"
#include "config.h"
#include <linux/input.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <glib.h>
#include <glib-unix.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <array>
#include <algorithm>
#include <chrono>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <iostream>

namespace fs = std::filesystem;

namespace AsusKeybind {

static constexpr const char* UNIT      = "asus-keybind.service";
static constexpr const char* CFG_GROUP = "Keybind";

const char* action_key(Action a) {
    switch (a) {
        case Action::BacklightUp:   return "backlight_up";
        case Action::BacklightDown: return "backlight_down";
        case Action::RgbCycle:      return "rgb_cycle";
        case Action::PerfCycle:     return "perf_cycle";
        default:                    return "";
    }
}

const char* action_label(Action a) {
    switch (a) {
        case Action::BacklightUp:   return "Keyboard Backlight +";
        case Action::BacklightDown: return "Keyboard Backlight -";
        case Action::RgbCycle:      return "Cycle RGB Mode";
        case Action::PerfCycle:     return "Cycle Performance Mode";
        default:                    return "";
    }
}

int get_binding(Action a) {
    return AsusConfig::get_int(ConfigCategory::Extra, CFG_GROUP, action_key(a), -1);
}

void set_binding(Action a, int code) {
    AsusConfig::set_int(ConfigCategory::Extra, CFG_GROUP, action_key(a), code);
}

bool is_supported() {
    return AsusKeyboard::is_supported() || AsusModes::is_supported();
}

static void backlight_step(int delta) {
    int max = AsusKeyboard::get_max_brightness();
    int cur = AsusKeyboard::get_brightness();
    AsusKeyboard::set_brightness(std::clamp(cur + delta, 0, max));
}

static void rgb_cycle() {
    int mode = static_cast<int>(AsusKeyboard::get_current_mode());
    if (mode < 0 || mode > 3) mode = 0;
    mode = (mode + 1) % 4;
    AsusKeyboard::set_rgb_mode(static_cast<AsusKeyboard::RgbMode>(mode),
                               AsusKeyboard::get_current_speed());
}

static void perf_cycle() {
    int m = static_cast<int>(AsusModes::get_mode());
    if (m < 0 || m > 2) m = static_cast<int>(AsusMode::Balanced);
    m = (m + 1) % 3;
    AsusModes::set_mode(static_cast<AsusMode>(m));
}

void perform(Action a) {
    switch (a) {
        case Action::BacklightUp:   backlight_step(+1); break;
        case Action::BacklightDown: backlight_step(-1); break;
        case Action::RgbCycle:      rgb_cycle();        break;
        case Action::PerfCycle:     perf_cycle();       break;
        default: break;
    }
}

static std::vector<int> open_key_devices(int flags) {
    std::vector<int> fds;
    const std::string base = "/dev/input";
    if (!fs::exists(base)) return fds;

    for (const auto& e : fs::directory_iterator(base)) {
        if (e.path().filename().string().rfind("event", 0) != 0) continue;
        int fd = open(e.path().c_str(), flags);
        if (fd < 0) continue;

        unsigned long evbits = 0;
        if (ioctl(fd, EVIOCGBIT(0, sizeof(evbits)), &evbits) >= 0 &&
            (evbits & (1UL << EV_KEY))) {
            fds.push_back(fd);
        } else {
            close(fd);
        }
    }
    return fds;
}

int run_daemon() {
    AsusConfig::init();
    AsusKeyboard::init();
    AsusModes::init();

    if (!is_supported()) {
        std::cerr << "[Keybind] No Asus keyboard/modes sysfs found; exiting." << std::endl;
        return 1;
    }

    std::vector<int> fds = open_key_devices(O_RDONLY);
    if (fds.empty()) {
        std::cerr << "[Keybind] Could not open input devices (need root)." << std::endl;
        return 1;
    }

    std::vector<struct pollfd> pfds;
    for (int fd : fds) pfds.push_back({fd, POLLIN, 0});

    std::array<int, (size_t)Action::Count> binds{};
    for (size_t i = 0; i < binds.size(); ++i) binds[i] = get_binding((Action)i);

    auto last_fire = std::chrono::steady_clock::now() - std::chrono::seconds(1);

    std::cout << "[Keybind] Listening on " << fds.size() << " device(s)." << std::endl;

    while (true) {
        if (poll(pfds.data(), pfds.size(), -1) < 0) break;

        for (auto& pfd : pfds) {
            if (!(pfd.revents & POLLIN)) continue;

            struct input_event ev;
            while (read(pfd.fd, &ev, sizeof(ev)) == sizeof(ev)) {
                if (ev.type != EV_KEY || ev.value != 1) continue;

                for (size_t i = 0; i < binds.size(); ++i) {
                    if (binds[i] >= 0 && ev.code == (unsigned)binds[i]) {
                        auto now = std::chrono::steady_clock::now();
                        if (now - last_fire < std::chrono::milliseconds(120)) continue;
                        last_fire = now;
                        perform((Action)i);
                    }
                }
            }
        }
    }

    for (int fd : fds) close(fd);
    return 0;
}

static std::string aac_path() {
    for (const char* p : {"/usr/local/bin/AAC", "/usr/bin/AAC"})
        if (fs::exists(p)) return p;
    return "AAC";
}

static std::string unit_path() {
    return std::string("/etc/systemd/system/") + UNIT;
}

bool service_is_enabled() {
    return std::system((std::string("systemctl is-enabled --quiet ") + UNIT).c_str()) == 0;
}

bool service_enable() {
    {
        std::ofstream f(unit_path());
        if (!f.is_open()) {
            std::cerr << "[Keybind] cannot write " << unit_path() << " (need root)" << std::endl;
            return false;
        }
        f << "[Unit]\n"
             "Description=Asus Armoury Control - Fn-key keybind daemon\n"
             "After=multi-user.target\n"
             "\n"
             "[Service]\n"
             "Type=simple\n"
             "ExecStart=" << aac_path() << " --keybind-daemon\n"
             "Restart=on-failure\n"
             "RestartSec=2\n"
             "\n"
             "[Install]\n"
             "WantedBy=multi-user.target\n";
        if (!f.good()) return false;
    }
    if (std::system("systemctl daemon-reload") != 0) return false;
    return std::system((std::string("systemctl enable --now ") + UNIT).c_str()) == 0;
}

bool service_disable() {
    std::system((std::string("systemctl disable --now ") + UNIT).c_str());
    std::remove(unit_path().c_str());
    std::system("systemctl daemon-reload");
    return true;
}

void service_reload() {
    if (!service_is_enabled()) return;
    std::system((std::string("systemctl restart ") + UNIT).c_str());
}

namespace {
    struct CaptureState {
        std::vector<int> fds;
        std::vector<guint> sources;
        std::function<void(int)> cb;
        bool done = false;
    };
    CaptureState* g_capture = nullptr;

    void finish_capture(int code) {
        if (!g_capture || g_capture->done) return;
        g_capture->done = true;

        for (guint s : g_capture->sources) g_source_remove(s);
        for (int fd : g_capture->fds) close(fd);

        auto cb = g_capture->cb;
        delete g_capture;
        g_capture = nullptr;
        if (cb) cb(code);
    }

    gboolean on_capture_fd(gint fd, GIOCondition, gpointer) {
        struct input_event ev;
        while (read(fd, &ev, sizeof(ev)) == sizeof(ev)) {
            if (ev.type == EV_KEY && ev.value == 1) {
                finish_capture((int)ev.code);
                return G_SOURCE_REMOVE;
            }
        }
        return G_SOURCE_CONTINUE;
    }
}

void begin_capture(std::function<void(int)> on_captured) {
    cancel_capture();

    auto* st = new CaptureState();
    st->cb = std::move(on_captured);
    st->fds = open_key_devices(O_RDONLY | O_NONBLOCK);

    if (st->fds.empty()) {
        st->cb(-1);
        delete st;
        return;
    }

    g_capture = st;
    for (int fd : st->fds)
        st->sources.push_back(g_unix_fd_add(fd, G_IO_IN, on_capture_fd, nullptr));
}

void cancel_capture() {
    finish_capture(-1);
}

}
