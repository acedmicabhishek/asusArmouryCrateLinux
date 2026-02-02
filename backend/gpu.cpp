#include "gpu.h"
#include <cstdlib>
#include <string>
#include <iostream>
#include <filesystem>
#include <array>
#include <memory>
#include <cstdio>

namespace AsusGpu {

    bool is_dynamic_boost_supported() {
        if (std::filesystem::exists("/usr/bin/nvidia-powerd")) return true;
        return false;
    }

    bool get_dynamic_boost() {
        int ret = system("systemctl is-active --quiet nvidia-powerd");
        return (ret == 0);
    }

    bool set_dynamic_boost(bool enable) {
        std::string cmd;
        if (enable) {
            cmd = "pkexec systemctl enable --now nvidia-powerd";
        } else {
            cmd = "pkexec systemctl disable --now nvidia-powerd";
        }
        
        std::cout << "[AsusGpu] Executing: " << cmd << std::endl;
        int ret = system(cmd.c_str());
        return (ret == 0);
    }
}
