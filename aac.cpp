#include "backend/modes.h"
#include "backend/battery.h"
#include "backend/gpu_mux.h"
#include "backend/gpu.h"
#include "backend/keyboard.h"
#include "backend/config.h"
#include <iostream>
#include <string>
#include <algorithm>

static void print_usage() {
    std::cout << "AAC - Asus Armoury Control CLI\n\n"
              << "Usage:\n"
              << "  AAC -P  silent|balanced|turbo    Set performance mode\n"
              << "  AAC -G  eco|hybrid|nvidia        Set GPU MUX mode (reboot required)\n"
              << "  AAC -B  <percent>                Set battery charge limit (40-100)\n"
              << "  AAC -DB on|off                   Toggle dynamic boost\n"
              << "  AAC -RGB <hex>                   Set keyboard RGB color (e.g. ff00aa)\n"
              << "  AAC --status                     Show current status\n"
              << "  AAC --help                       Show this help\n";
}

static std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

static void show_status() {
    // Power mode
    AsusMode mode = AsusModes::get_mode();
    std::string mode_str = "Unknown";
    switch (mode) {
        case AsusMode::Silent:   mode_str = "Silent"; break;
        case AsusMode::Balanced: mode_str = "Balanced"; break;
        case AsusMode::Turbo:    mode_str = "Turbo"; break;
        default: break;
    }
    std::cout << "Power Mode:     " << mode_str << "\n";

    // Power source
    std::cout << "Power Source:   " << (AsusBattery::is_on_ac() ? "Charger (AC)" : "Battery (DC)") << "\n";

    // Battery limit
    int limit = AsusBattery::get_charge_limit();
    if (limit > 0)
        std::cout << "Charge Limit:   " << limit << "%\n";

    // GPU MUX
    if (AsusMux::is_supported()) {
        std::cout << "GPU Mode:       " << AsusMux::mode_to_string(AsusMux::get_mode()) << "\n";
    }

    // Dynamic boost
    if (AsusGpu::is_dynamic_boost_supported()) {
        std::cout << "Dynamic Boost:  " << (AsusGpu::get_dynamic_boost() ? "On" : "Off") << "\n";
    }

    // RGB
    if (AsusKeyboard::is_supported() && AsusKeyboard::has_rgb()) {
        auto c = AsusKeyboard::get_color();
        char hex[8];
        snprintf(hex, sizeof(hex), "%02x%02x%02x", c.r, c.g, c.b);
        std::cout << "Keyboard RGB:   #" << hex << "\n";
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    AsusConfig::init();

    std::string flag = argv[1];

    // --help
    if (flag == "--help" || flag == "-h") {
        print_usage();
        return 0;
    }

    // --status
    if (flag == "--status" || flag == "-s") {
        show_status();
        return 0;
    }

    if (argc < 3) {
        std::cerr << "Error: Missing value for " << flag << "\n";
        print_usage();
        return 1;
    }

    std::string value = to_lower(argv[2]);

    // -P: Performance mode
    if (flag == "-P" || flag == "-p") {
        AsusMode mode = AsusMode::Unknown;
        if (value == "silent")        mode = AsusMode::Silent;
        else if (value == "balanced") mode = AsusMode::Balanced;
        else if (value == "turbo")    mode = AsusMode::Turbo;
        else {
            std::cerr << "Error: Invalid mode '" << argv[2] << "'. Use: silent, balanced, turbo\n";
            return 1;
        }

        bool ac = AsusBattery::is_on_ac();
        if (AsusModes::set_mode_for(mode, ac)) {
            std::cout << "Power mode set to " << argv[2] << " (" << (ac ? "AC" : "Battery") << ")\n";
            return 0;
        } else {
            std::cerr << "Failed to set power mode.\n";
            return 1;
        }
    }

    // -G: GPU MUX
    if (flag == "-G" || flag == "-g") {
        AsusMux::Mode mode = AsusMux::Mode::Unknown;
        if (value == "eco" || value == "integrated")  mode = AsusMux::Mode::Integrated;
        else if (value == "hybrid" || value == "standard") mode = AsusMux::Mode::Hybrid;
        else if (value == "nvidia" || value == "ultimate") mode = AsusMux::Mode::Nvidia;
        else {
            std::cerr << "Error: Invalid GPU mode '" << argv[2] << "'. Use: eco, hybrid, nvidia\n";
            return 1;
        }

        if (AsusMux::set_mode(mode)) {
            std::cout << "GPU mode set to " << AsusMux::mode_to_string(mode) << ". Reboot required.\n";
            return 0;
        } else {
            std::cerr << "Failed to set GPU mode.\n";
            return 1;
        }
    }

    // -B: Battery charge limit
    if (flag == "-B" || flag == "-b") {
        int limit;
        try {
            // Strip trailing % if present
            std::string v = value;
            if (!v.empty() && v.back() == '%') v.pop_back();
            limit = std::stoi(v);
        } catch (...) {
            std::cerr << "Error: Invalid percentage '" << argv[2] << "'\n";
            return 1;
        }

        if (limit < 40 || limit > 100) {
            std::cerr << "Error: Charge limit must be between 40 and 100.\n";
            return 1;
        }

        if (AsusBattery::set_charge_limit(limit)) {
            std::cout << "Charge limit set to " << limit << "%\n";
            return 0;
        } else {
            std::cerr << "Failed to set charge limit.\n";
            return 1;
        }
    }

    // -DB: Dynamic Boost
    if (flag == "-DB" || flag == "-db") {
        bool enable;
        if (value == "on" || value == "1" || value == "true")       enable = true;
        else if (value == "off" || value == "0" || value == "false") enable = false;
        else {
            std::cerr << "Error: Invalid value '" << argv[2] << "'. Use: on, off\n";
            return 1;
        }

        if (!AsusGpu::is_dynamic_boost_supported()) {
            std::cerr << "Error: Dynamic boost is not supported (nvidia-powerd not found).\n";
            return 1;
        }

        if (AsusGpu::set_dynamic_boost(enable)) {
            std::cout << "Dynamic boost " << (enable ? "enabled" : "disabled") << "\n";
            return 0;
        } else {
            std::cerr << "Failed to set dynamic boost.\n";
            return 1;
        }
    }

    // -RGB: Keyboard color
    if (flag == "-RGB" || flag == "-rgb") {
        std::string hex = value;
        // Strip leading # if present
        if (!hex.empty() && hex[0] == '#') hex = hex.substr(1);

        if (hex.length() != 6) {
            std::cerr << "Error: Invalid hex color '" << argv[2] << "'. Use 6-digit hex (e.g. ff00aa)\n";
            return 1;
        }

        int r, g, b;
        try {
            r = std::stoi(hex.substr(0, 2), nullptr, 16);
            g = std::stoi(hex.substr(2, 2), nullptr, 16);
            b = std::stoi(hex.substr(4, 2), nullptr, 16);
        } catch (...) {
            std::cerr << "Error: Invalid hex color '" << argv[2] << "'\n";
            return 1;
        }

        if (!AsusKeyboard::is_supported() || !AsusKeyboard::has_rgb()) {
            std::cerr << "Error: RGB keyboard not supported.\n";
            return 1;
        }

        AsusKeyboard::init();
        AsusKeyboard::set_color(r, g, b);
        std::cout << "Keyboard color set to #" << hex << "\n";
        return 0;
    }

    std::cerr << "Error: Unknown flag '" << flag << "'\n";
    print_usage();
    return 1;
}
