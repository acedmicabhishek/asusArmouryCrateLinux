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


    AsusMode get_saved_mode(bool ac) {
        std::string key = ac ? "ModeAC" : "ModeBattery";
        int m = AsusConfig::get_int(ConfigCategory::Power, "Power", key, -1);
        if (m == -1) m = AsusConfig::get_int(ConfigCategory::Power, "Power", "Mode", 1);
        return static_cast<AsusMode>(m);
    }

    bool set_mode_for(AsusMode mode, bool ac) {
        if (mode == AsusMode::Unknown) return false;
        std::string key = ac ? "ModeAC" : "ModeBattery";
        AsusConfig::set_int(ConfigCategory::Power, "Power", key, static_cast<int>(mode));
        
        if (AsusBattery::is_on_ac() == ac) {
            return SysfsWriter::write(THROTTLE_POLICY_PATH, std::to_string(static_cast<int>(mode)));
        }
        return true;
    }

    bool set_mode(AsusMode mode) {
        return set_mode_for(mode, AsusBattery::is_on_ac());
    }

    void init() {
         last_is_ac = AsusBattery::is_on_ac();
         set_mode(get_saved_mode(last_is_ac));
    }

    void update_auto() {
        bool current_is_ac = AsusBattery::is_on_ac();
        if (current_is_ac != last_is_ac) {
            last_is_ac = current_is_ac;
            set_mode(get_saved_mode(current_is_ac));
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
