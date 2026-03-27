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
    bool set_mode_for(AsusMode mode, bool ac);
    AsusMode get_mode();
    AsusMode get_saved_mode(bool ac);
    bool is_supported();
    void update_auto();
}
