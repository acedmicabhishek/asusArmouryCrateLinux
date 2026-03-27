#pragma once

enum class AsusMode {
    Silent = 0,
    Balanced = 1,
    Turbo = 2,
    Unknown = -1
};

namespace AsusModes {
    void init();
    bool set_mode(AsusMode mode);
    AsusMode get_mode();
    bool is_supported();
}
