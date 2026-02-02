#pragma once
#include <string>
#include <glib.h>

enum class ConfigCategory {
    RGB,
    Power,
    Extra
};

class AsusConfig {
public:
    static void init();
    static void save(ConfigCategory cat);

    static void set_string(ConfigCategory cat, const std::string& group, const std::string& key, const std::string& val);
    static std::string get_string(ConfigCategory cat, const std::string& group, const std::string& key, const std::string& def);

    static void set_int(ConfigCategory cat, const std::string& group, const std::string& key, int val);
    static int get_int(ConfigCategory cat, const std::string& group, const std::string& key, int def);

    static void set_bool(ConfigCategory cat, const std::string& group, const std::string& key, bool val);
    static bool get_bool(ConfigCategory cat, const std::string& group, const std::string& key, bool def);

private:
    static GKeyFile* get_file(ConfigCategory cat);
    static std::string get_path(ConfigCategory cat);
    static void ensure_dir(const std::string& path);
};
