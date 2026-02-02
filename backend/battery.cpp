#include "battery.h"
#include "sysfs_writer.h"
#include <filesystem>
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

    bool set_charge_limit(int limit) {
        if (limit < 40) limit = 40;
        if (limit > 100) limit = 100;
        
        std::string path = find_battery_path();
        if (path.empty()) return false;
        
        std::string path_attr = path + "/charge_control_end_threshold";
        
        return SysfsWriter::write(path, std::to_string(limit));
    }

    int get_charge_limit() {
        std::string path = find_battery_path();
        if (path.empty()) return -1;
        
        auto val = SysfsWriter::read(path);
        if (!val) return -1;
        
        try { return std::stoi(*val); } catch(...) { return -1; }
    }
}
