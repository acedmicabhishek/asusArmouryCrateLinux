#include "battery.h"
#include "sysfs_writer.h"
#include "config.h"
#include <filesystem>
#include <fstream>
#include <string>


namespace AsusBattery {
    static std::string find_battery_path() {
        static std::string cached_path;
        if (!cached_path.empty() && std::filesystem::exists(cached_path)) return cached_path;

        for (const auto& entry : std::filesystem::directory_iterator("/sys/class/power_supply")) {
            auto path = entry.path() / "charge_control_end_threshold";
            if (std::filesystem::exists(path)) {
                cached_path = path.string();
                return cached_path;
            }
        }
        return "";
    }

    bool is_supported() {
        return !find_battery_path().empty();
    }


    bool is_on_ac() {
        std::ifstream f("/sys/class/power_supply/ACAD/online");
        if (!f.is_open()) {
            for (const auto& entry : std::filesystem::directory_iterator("/sys/class/power_supply")) {
                if (entry.path().filename().string().find("AC") != std::string::npos || 
                    entry.path().filename().string().find("ADP") != std::string::npos) {
                    std::ifstream f2(entry.path() / "online");
                    if (f2.is_open()) {
                        int online = 0;
                        f2 >> online;
                        return online == 1;
                    }
                }
            }
            return true; 
        }
        int online = 0;
        f >> online;
        return online == 1;
    }

    void init() {
        int limit = AsusConfig::get_int(ConfigCategory::Power, "Battery", "ChargeLimit", -1);
        if (limit != -1) set_charge_limit(limit);
    }

    bool set_charge_limit(int limit) {
        if (limit < 40) limit = 40;
        if (limit > 100) limit = 100;
        
        std::string path = find_battery_path();
        if (path.empty()) return false;
        
        bool res = SysfsWriter::write(path, std::to_string(limit));
        if (res) {
            AsusConfig::set_int(ConfigCategory::Power, "Battery", "ChargeLimit", limit);
        }
        return res;
    }

    int get_charge_limit() {
        std::string path = find_battery_path();
        if (path.empty()) return -1;
        
        auto val = SysfsWriter::read(path);
        if (!val) return -1;
        
        try { return std::stoi(*val); } catch(...) { return -1; }
    }
}
