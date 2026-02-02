#pragma once
#include <string>

namespace AsusBrightness {
    
    void init();
    int get_brightness();
    void set_brightness(int percent);
    bool is_supported();
}
