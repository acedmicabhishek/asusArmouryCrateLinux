#include "display.h"
#include "sysfs_writer.h"
#include "config.h"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cstdio>
#include <memory>
#include <array>
#include <sstream>
#include <cmath>
#include <map>

namespace AsusDisplay {

    static const std::string PANEL_OD_PATH = "/sys/devices/platform/asus-nb-wmi/panel_od";
    static const std::string MINI_LED_PATH = "/sys/devices/platform/asus-nb-wmi/mini_led_mode";

    static std::string exec(const char* cmd) {
        std::array<char, 128> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
        if (!pipe) return "";
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
        return result;
    }



    void init() {
         bool od = AsusConfig::get_bool(ConfigCategory::Extra, "Display", "Overdrive", false);
         if (od) set_panel_overdrive(true);

         bool ml = AsusConfig::get_bool(ConfigCategory::Extra, "Display", "MiniLED", false);
         if (ml) set_miniled_mode(true);
         
         int hz = AsusConfig::get_int(ConfigCategory::Extra, "Display", "RefreshRate", -1);
         if (hz > 0) set_refresh_rate(hz);
    }

    bool is_overdrive_supported() {
        return std::filesystem::exists(PANEL_OD_PATH);
    }

    bool set_panel_overdrive(bool enable) {
        if (!is_overdrive_supported()) return false;
        bool res = SysfsWriter::write(PANEL_OD_PATH, enable ? "1" : "0");
        if (res) AsusConfig::set_bool(ConfigCategory::Extra, "Display", "Overdrive", enable);
        return res;
    }

    bool get_panel_overdrive() {
        auto val = SysfsWriter::read(PANEL_OD_PATH);
        if (!val) return false;
        try { return std::stoi(*val) == 1; } catch (...) { return false; }
    }

    bool is_miniled_supported() {
        return std::filesystem::exists(MINI_LED_PATH);
    }

    bool set_miniled_mode(bool enable) {
        if (!is_miniled_supported()) return false;
        bool res = SysfsWriter::write(MINI_LED_PATH, enable ? "1" : "0");
        if (res) AsusConfig::set_bool(ConfigCategory::Extra, "Display", "MiniLED", enable);
        return res;
    }

    bool get_miniled_mode() {
        auto val = SysfsWriter::read(MINI_LED_PATH);
        if (!val) return false;
        try { return std::stoi(*val) == 1; } catch (...) { return false; }
    }

    std::vector<int> get_available_refresh_rates() {
        std::vector<int> rates;
        std::string output = exec("xrandr -q");
        std::istringstream iss(output);
        std::string line;
        
        while (std::getline(iss, line)) {
            if (line.find("*") != std::string::npos) {
                 std::stringstream ls(line);
                 std::string token;
                 ls >> token;
                 
                 while (ls >> token) {
                      try {
                        std::string clean;
                        for (char c : token) if (isdigit(c) || c == '.') clean += c;
                        if (clean.empty()) continue;
                        int hz = (int)std::round(std::stof(clean));
                        if (hz > 0) rates.push_back(hz);
                      } catch (...) {}
                 }
                 break;
            }
        }
        
        if (rates.empty()) rates = {60, 144};
        std::sort(rates.begin(), rates.end());
        return rates;
    }

    bool set_refresh_rate(int hz) {
        std::cout << "[AsusDisplay] Setting refresh rate: " << hz << "Hz" << std::endl;
        std::string cmd = "xrandr -r " + std::to_string(hz);
        exec(cmd.c_str());
        AsusConfig::set_int(ConfigCategory::Extra, "Display", "RefreshRate", hz);
        return true;
    }

    int get_current_refresh_rate() {
        std::string out = exec("xrandr -q");
        std::istringstream iss(out);
        std::string line;
        while (std::getline(iss, line)) {
             if (line.find('*') != std::string::npos) {
                std::stringstream ls(line);
                std::string token;
                ls >> token; 
                while (ls >> token) {
                    if (token.find('*') != std::string::npos) {
                        try {
                            std::string clean;
                            for (char c : token) if (isdigit(c) || c == '.') clean += c;
                            return (int)std::round(std::stof(clean));
                        } catch(...) {}
                    }
                }
             }
        }
        return 60;
    }
    
    bool is_supported() {
        return true; 
    }
}

