#include "fan_control.h"
#include "sysfs_writer.h"
#include "config.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <iostream>

namespace AsusFanControl {
    namespace fs = std::filesystem;



    struct FanPaths {
        std::string pwm_enable;
        std::string pwm_control;
        bool scanned = false;
    };

    static FanPaths g_paths;
    static bool g_curve_enabled = false;
    static std::vector<FanCurvePoint> g_curve_points = {
        {30, 0}, {50, 20}, {60, 50}, {70, 80}, {80, 100}
    };

    static void scan() {
        if (g_paths.scanned) return;
        g_paths.pwm_enable = "";
        g_paths.pwm_control = "";

        if (fs::exists("/sys/class/hwmon")) {
            for (const auto& entry : fs::directory_iterator("/sys/class/hwmon")) {
                std::string path = entry.path().string();
                std::string name_path = path + "/name";
                
                auto name_opt = SysfsWriter::read(name_path);
                if (name_opt && *name_opt == "asus") {
                    if (fs::exists(path + "/pwm1_enable")) g_paths.pwm_enable = path + "/pwm1_enable";
                    if (fs::exists(path + "/pwm1")) g_paths.pwm_control = path + "/pwm1";
                }
            }
        }
        g_paths.scanned = true;
    }

    bool is_supported() {
        if (fs::exists("/tmp/kerneldrive_debug_fan")) return true;
        scan();
        return !g_paths.pwm_enable.empty();
    }



    void init() {
         scan();
         int manual = AsusConfig::get_int(ConfigCategory::Power, "Fan", "ManualMode", 0);
         if (manual == 1) set_manual_mode(true);
         
         int speed = AsusConfig::get_int(ConfigCategory::Power, "Fan", "Speed", -1);
         if (speed != -1 && manual == 1) set_fan_speed(speed);
    }

    bool set_manual_mode(bool enable) {
        scan();
        if (g_paths.pwm_enable.empty()) return false;
        
        // 2 = Auto, 1 = Manual/PWM
        std::string target = enable ? "1" : "2"; 
        
        SysfsWriter::write(g_paths.pwm_enable, target);
        // debug block for locked bios
        if (fs::exists("/tmp/kerneldrive_debug_fan")) {
            std::cout << "[AsusFanControl] DEBUG OVERRIDE: Simulating successful mode switch." << std::endl;
             AsusConfig::set_int(ConfigCategory::Power, "Fan", "ManualMode", enable ? 1 : 0);
            return true;
        }

        // BIOS Verification
        auto val = SysfsWriter::read(g_paths.pwm_enable);
        if (!val) return false;
        
        int current = 0;
        try { current = std::stoi(*val); } catch(...) { return false; }
        
        int expected = enable ? 1 : 2;
        if (current != expected) {
            std::cerr << "[AsusFanControl] BIOS rejected fan mode change! Wanted " << expected << " got " << current << std::endl;
            return false;
        }
        
        AsusConfig::set_int(ConfigCategory::Power, "Fan", "ManualMode", enable ? 1 : 0);
        return true;
    }

    int calculate_curve_pwm(int current_temp) {
        if (g_curve_points.empty()) return 0;
        
        if (current_temp <= g_curve_points.front().temp) return g_curve_points.front().pwm;
        if (current_temp >= g_curve_points.back().temp) return g_curve_points.back().pwm;
        
        for (size_t i = 0; i < g_curve_points.size() - 1; ++i) {
            if (current_temp >= g_curve_points[i].temp && current_temp <= g_curve_points[i+1].temp) {
                float t1 = g_curve_points[i].temp;
                float p1 = g_curve_points[i].pwm;
                float t2 = g_curve_points[i+1].temp;
                float p2 = g_curve_points[i+1].pwm;
                
                float factor = (current_temp - t1) / (t2 - t1);
                return (int)(p1 + (p2 - p1) * factor);
            }
        }
        return 0;
    }

    void set_curve_enabled(bool enabled) {
        g_curve_enabled = enabled;
        if (enabled) {
            if (!get_manual_mode()) set_manual_mode(true);
        }
    }

    bool is_curve_enabled() {
        return g_curve_enabled;
    }

    void set_curve_points(const std::vector<FanCurvePoint>& points) {
        g_curve_points = points;
    }

    std::vector<FanCurvePoint> get_curve_points() {
        return g_curve_points;
    }


    bool get_manual_mode() {
        scan();
        if (fs::exists("/tmp/kerneldrive_debug_fan")) {
            return true;
        }
        if (g_paths.pwm_enable.empty()) return false;
        auto val = SysfsWriter::read(g_paths.pwm_enable);
        if (!val) return false;
        try { return std::stoi(*val) == 1; } catch(...) { return false; }
    }

    void set_fan_speed(int percent) {
        scan();
        if (g_paths.pwm_control.empty()) return;
        
        int pwm = (percent * 255) / 100;
        if (pwm < 0) pwm = 0;
        if (pwm > 255) pwm = 255;
        
        SysfsWriter::write(g_paths.pwm_control, std::to_string(pwm));
        AsusConfig::set_int(ConfigCategory::Power, "Fan", "Speed", percent);
    }

    int get_fan_speed() {
        scan();
        if (g_paths.pwm_control.empty()) return 0;
        auto val = SysfsWriter::read(g_paths.pwm_control);
        if (!val) return 0;
        try {
            int pwm = std::stoi(*val);
            return (pwm * 100) / 255;
        } catch(...) { return 0; }
    }
}
