#pragma once
#include <string>

namespace AsusMux {
    enum class Mode {
        Hybrid,
        Integrated,
        Nvidia,
        Unknown
    };

    Mode get_mode();
    bool set_mode(Mode mode);
    std::string mode_to_string(Mode mode);
    bool is_supported();
}
