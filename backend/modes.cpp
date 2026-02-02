#include "modes.h"
#include "sysfs_writer.h"
#include <filesystem>
#include <string>

static const std::string THROTTLE_POLICY_PATH = "/sys/devices/platform/asus-nb-wmi/throttle_thermal_policy";

namespace AsusModes {
    bool is_supported() {
        return std::filesystem::exists(THROTTLE_POLICY_PATH);
    }

    bool set_mode(AsusMode mode) {
        if (mode == AsusMode::Unknown) return false;
        return SysfsWriter::write(THROTTLE_POLICY_PATH, std::to_string(static_cast<int>(mode)));
    }

    AsusMode get_mode() {
        auto val = SysfsWriter::read(THROTTLE_POLICY_PATH);
        if (!val) return AsusMode::Unknown;
        
        try {
            int v = std::stoi(*val);
            if (v == 0) return AsusMode::Balanced;
            if (v == 1) return AsusMode::Turbo;
            if (v == 2) return AsusMode::Silent;
        } catch (...) {}
        
        return AsusMode::Unknown;
    }
}
