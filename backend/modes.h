#pragma once

enum class AsusMode {
    Balanced = 0,
    Turbo = 1,
    Silent = 2,
    Unknown = -1
};

namespace AsusModes {
    bool set_mode(AsusMode mode);
    AsusMode get_mode();
    bool is_supported();
}
