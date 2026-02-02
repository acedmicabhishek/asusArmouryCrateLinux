#pragma once
#include <string>
#include <vector>

namespace AsusDisplay {
    // Refresh Rate Control
    std::vector<int> get_available_refresh_rates();
    bool set_refresh_rate(int hz);
    int get_current_refresh_rate();

    // Panel Overdrive (OD)
    bool set_panel_overdrive(bool enable);
    bool get_panel_overdrive();
    bool is_overdrive_supported();

    // Panel Power Saver (MiniLED)
    bool set_miniled_mode(bool enable);
    bool get_miniled_mode();
    bool is_miniled_supported();
}
