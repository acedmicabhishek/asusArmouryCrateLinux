#pragma once
#include <string>
#include <array>

namespace AsusKeyboard {
    
    struct Color { int r; int g; int b; };

    enum class RgbMode {
        Static = 0,
        Breathing = 1,
        Cycle = 2,
        Strobe = 3,
        Unknown = 255
    };

    void init();
    int get_brightness();
    void set_brightness(int val);
    int get_max_brightness();
    
    int get_rgb_mode();
    RgbMode get_current_mode();
    int get_current_speed();
    
    void set_rgb_mode(RgbMode mode, int speed = 1);
    
    Color get_color();
    void set_color(int r, int g, int b);

    void apply_rgb(RgbMode mode, Color color, int speed);

    bool is_supported();
    bool has_rgb();
}
