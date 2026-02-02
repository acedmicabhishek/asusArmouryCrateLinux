#include "keyboard.h"
#include "config.h"
#include "sysfs_writer.h"
#include <filesystem>
#include <vector>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iostream>
#include <iomanip>
#include <sstream>

namespace AsusKeyboard {
    namespace fs = std::filesystem;
    static const std::string BASE_PATH = "/sys/class/leds/asus::kbd_backlight";
    static int g_max_brightness = 3; 
    static Color g_current_color = {255, 0, 0};
    static RgbMode g_current_mode = RgbMode::Static;
    static int g_current_speed = 1;

    void init() {
        if (fs::exists(BASE_PATH + "/max_brightness")) {
             auto val = SysfsWriter::read(BASE_PATH + "/max_brightness");
             if (val) {
                 try { g_max_brightness = std::stoi(*val); } catch(...) {}
             }
        }
        
        // Load persist
        g_current_mode = static_cast<RgbMode>(AsusConfig::get_int(ConfigCategory::RGB, "Keyboard", "RgbMode", (int)RgbMode::Static));
        g_current_speed = AsusConfig::get_int(ConfigCategory::RGB, "Keyboard", "RgbSpeed", 1);
        g_current_color.r = AsusConfig::get_int(ConfigCategory::RGB, "Keyboard", "ColorR", 255);
        g_current_color.g = AsusConfig::get_int(ConfigCategory::RGB, "Keyboard", "ColorG", 0);
        g_current_color.b = AsusConfig::get_int(ConfigCategory::RGB, "Keyboard", "ColorB", 0);
        
        // Restore Hardware
        apply_rgb(g_current_mode, g_current_color, g_current_speed);
        
        int bright = AsusConfig::get_int(ConfigCategory::RGB, "Keyboard", "Brightness", -1);
        if (bright != -1) set_brightness(bright);
    }

    int get_brightness() {
        auto val = SysfsWriter::read(BASE_PATH + "/brightness");
        if (val) {
            try { return std::stoi(*val); } catch(...) {}
        }
        return 0;
    }

    void set_brightness(int val) {
        if (val < 0) val = 0;
        if (val > g_max_brightness) val = g_max_brightness;
        SysfsWriter::write(BASE_PATH + "/brightness", std::to_string(val));
        AsusConfig::set_int(ConfigCategory::RGB, "Keyboard", "Brightness", val);
    }

    int get_max_brightness() {
        return g_max_brightness;
    }

    int get_rgb_mode() {
        auto val = SysfsWriter::read(BASE_PATH + "/kbd_rgb_mode");
        if (val) {
            try { return std::stoi(*val); } catch(...) {}
        }
        return 0;
    }

    static void write_bytes(const std::string& attr, const std::vector<uint8_t>& bytes) {
        std::string data;
        for (size_t i = 0; i < bytes.size(); ++i) {
            data += std::to_string(bytes[i]);
            if (i < bytes.size() - 1) data += " ";
        }
        SysfsWriter::write(BASE_PATH + "/" + attr, data);
    }

    void apply_rgb(RgbMode mode, Color color, int speed) {
        // Packet Structure
        // [Cmd, Mode, R, G, B, Speed]
        // Cmd: 0 = Apply
        // Mode: 0=Static, 1=Breath, 2=Cycle, 3=Strobe
        
        std::vector<uint8_t> packet;
        packet.push_back(0);
        packet.push_back(static_cast<uint8_t>(mode));
        packet.push_back(std::clamp(color.r, 0, 255));
        packet.push_back(std::clamp(color.g, 0, 255));
        packet.push_back(std::clamp(color.b, 0, 255));
        packet.push_back(std::clamp(speed, 0, 2));
 
        write_bytes("kbd_rgb_mode", packet);
        
        g_current_color = color;
        g_current_mode = mode;
        g_current_speed = speed;
        
        // Save
        AsusConfig::set_int(ConfigCategory::RGB, "Keyboard", "RgbMode", (int)mode);
        AsusConfig::set_int(ConfigCategory::RGB, "Keyboard", "RgbSpeed", speed);
        AsusConfig::set_int(ConfigCategory::RGB, "Keyboard", "ColorR", color.r);
        AsusConfig::set_int(ConfigCategory::RGB, "Keyboard", "ColorG", color.g);
        AsusConfig::set_int(ConfigCategory::RGB, "Keyboard", "ColorG", color.g);
        AsusConfig::set_int(ConfigCategory::RGB, "Keyboard", "ColorB", color.b);
    }

    void set_rgb_mode(RgbMode mode, int speed) {
        apply_rgb(mode, g_current_color, speed);
    }
    
    void set_rgb_mode_int(int mode) {
        set_rgb_mode(static_cast<RgbMode>(mode), g_current_speed);
    }
    
    RgbMode get_current_mode() {
        return g_current_mode;
    }

    int get_current_speed() {
        return g_current_speed;
    }

    Color get_color() {
        return g_current_color;
    }

    void set_color(int r, int g, int b) {
        apply_rgb(RgbMode::Static, {r, g, b}, g_current_speed);
    }

    bool is_supported() {
        return fs::exists(BASE_PATH);
    }

    bool has_rgb() {
        return fs::exists(BASE_PATH + "/kbd_rgb_mode");
    }
}
