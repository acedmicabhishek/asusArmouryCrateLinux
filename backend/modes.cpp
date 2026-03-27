#include "modes.h"
#include "sysfs_writer.h"
#include "config.h"
#include "battery.h"
#include <filesystem>
#include <string>


static const std::string THROTTLE_POLICY_PATH = "/sys/devices/platform/asus-nb-wmi/throttle_thermal_policy";

namespace AsusModes {
    static bool last_is_ac = true;

    bool is_supported() {
        return std::filesystem::exists(THROTTLE_POLICY_PATH);
    }


    bool set_mode(AsusMode mode) {
        if (mode == AsusMode::Unknown) return false;
        bool res = SysfsWriter::write(THROTTLE_POLICY_PATH, std::to_string(static_cast<int>(mode)));
        if (res) {
             std::string key = AsusBattery::is_on_ac() ? "ModeAC" : "ModeBattery";
             AsusConfig::set_int(ConfigCategory::Power, "Power", key, static_cast<int>(mode));
        }
        return res;
    }

    void init() {
         last_is_ac = AsusBattery::is_on_ac();
         std::string key = last_is_ac ? "ModeAC" : "ModeBattery";
         int m = AsusConfig::get_int(ConfigCategory::Power, "Power", key, -1);
         if (m == -1) {
              m = AsusConfig::get_int(ConfigCategory::Power, "Power", "Mode", -1);
         }
         
         if (m != -1) set_mode(static_cast<AsusMode>(m));
    }

    void update_auto() {
        bool current_is_ac = AsusBattery::is_on_ac();
        if (current_is_ac != last_is_ac) {
            last_is_ac = current_is_ac;
            std::string key = current_is_ac ? "ModeAC" : "ModeBattery";
            int m = AsusConfig::get_int(ConfigCategory::Power, "Power", key, -1);
            if (m != -1) set_mode(static_cast<AsusMode>(m));
        }
    }

    AsusMode get_mode() {
        auto val = SysfsWriter::read(THROTTLE_POLICY_PATH);
        if (!val) return AsusMode::Unknown;
        
        try {
            int v = std::stoi(*val);
            if (v == 0) return AsusMode::Silent;
            if (v == 1) return AsusMode::Balanced;
            if (v == 2) return AsusMode::Turbo;
        } catch (...) {}
        
        return AsusMode::Unknown;
    }
}
