#pragma once

#include <vector>

namespace AsusFanControl {
    struct FanCurvePoint {
        int temp;
        int pwm;
    };

    bool is_supported();
    bool set_manual_mode(bool enable);
    bool get_manual_mode();
    void set_fan_speed(int percent);
    int get_fan_speed();
    // curve is disabled btw , I might add in future

    void set_curve_enabled(bool enabled);
    bool is_curve_enabled();
    void set_curve_points(const std::vector<FanCurvePoint>& points);
    std::vector<FanCurvePoint> get_curve_points();
    int calculate_curve_pwm(int temp); 
}
