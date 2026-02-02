#include "gpu_mux.h"
#include <cstdlib>
#include <iostream>
#include <array>
#include <memory>
#include <filesystem>
#include <unistd.h>
#include <limits.h>

namespace AsusMux {
    
    static std::string get_tool_path() {
        if (std::filesystem::exists("/usr/bin/optimus_helper")) return "/usr/bin/optimus_helper";
        if (std::filesystem::exists("build/optimus_helper")) return "build/optimus_helper";
        return "optimus_helper"; 
    }

    static std::string exec(const char* cmd) {
        std::array<char, 128> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
        if (!pipe) return "";
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
        return result;
    }

    Mode get_mode() {
        std::string cmd = get_tool_path() + " --query";
        std::string out = exec(cmd.c_str());
        
        if (!out.empty() && out.back() == '\n') out.pop_back();

        if (out == "integrated") return Mode::Integrated;
        if (out == "nvidia") return Mode::Nvidia;
        return Mode::Hybrid;
    }

    bool set_mode(Mode mode) {
        std::string flag;
        switch(mode) {
            case Mode::Integrated: flag = "--integrated"; break;
            case Mode::Nvidia: flag = "--nvidia"; break;
            case Mode::Hybrid: default: flag = "--hybrid"; break;
        }

        std::string cmd = "pkexec " + get_tool_path() + " " + flag;
        std::cout << "[AsusMux] Executing: " << cmd << std::endl;
        
        int ret = system(cmd.c_str());
        return (ret == 0);
    }

    std::string mode_to_string(Mode mode) {
        switch(mode) {
            case Mode::Integrated: return "Eco (Integrated)";
            case Mode::Nvidia: return "Ultimate (Nvidia)";
            case Mode::Hybrid: default: return "Standard (Hybrid)";
        }
    }

    bool is_supported() {
        return true; 
    }
}
