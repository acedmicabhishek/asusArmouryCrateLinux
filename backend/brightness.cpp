#include "brightness.h"
#include "sysfs_writer.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>

namespace AsusBrightness {
    namespace fs = std::filesystem;

    static std::string g_backlight_path = "";
    static int g_max_brightness = 255;

    void init() {
        if (!g_backlight_path.empty()) return;

        if (fs::exists("/sys/class/backlight")) {
            for (const auto& entry : fs::directory_iterator("/sys/class/backlight")) {
                std::string path = entry.path().string();
                if (path.find("amdgpu") != std::string::npos || path.find("intel") != std::string::npos) {
                    g_backlight_path = path;
                    break;
                }
                if (g_backlight_path.empty()) g_backlight_path = path;
            }
        }
        
        if (!g_backlight_path.empty()) {
            auto max_opt = SysfsWriter::read(g_backlight_path + "/max_brightness");
            if (max_opt) {
                try { g_max_brightness = std::stoi(*max_opt); } catch (...) {}
            }
        }
        std::cout << "[AsusBrightness] Path: " << g_backlight_path << " Max: " << g_max_brightness << std::endl;
    }

    int get_brightness() {
        if (g_backlight_path.empty()) init();
        if (g_backlight_path.empty()) return 0;

        auto val_opt = SysfsWriter::read(g_backlight_path + "/brightness");
        if (val_opt) {
            try {
                int val = std::stoi(*val_opt);
                return (val * 100) / g_max_brightness;
            } catch (...) {}
        }
        return 0;
    }

    void set_brightness(int percent) {
        if (g_backlight_path.empty()) init();
        if (g_backlight_path.empty()) return;

        percent = std::clamp(percent, 0, 100);
        int val = (percent * g_max_brightness) / 100;
        
        SysfsWriter::write(g_backlight_path + "/brightness", std::to_string(val));
    }

    bool is_supported() {
        if (g_backlight_path.empty()) init();
        return !g_backlight_path.empty();
    }
}
