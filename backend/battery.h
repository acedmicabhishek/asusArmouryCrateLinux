#pragma once
#include <string>

namespace AsusBattery {
    bool set_charge_limit(int limit);
    int get_charge_limit();
    bool is_supported();
}
