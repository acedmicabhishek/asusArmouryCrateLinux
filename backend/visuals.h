#pragma once
#include <string>
#include <vector>

namespace AsusVisuals {
    struct VisualMode {
        std::string name;
        int id;
    };

    void init();
    bool is_supported();
    std::vector<VisualMode> get_modes();
    void set_mode(int id);
}
