#include "config.h"
#include <filesystem>
#include <iostream>
#include <map>

static std::map<ConfigCategory, GKeyFile*> g_files;
static std::map<ConfigCategory, std::string> g_paths;

void AsusConfig::init() {

}

std::string AsusConfig::get_path(ConfigCategory cat) {
    if (g_paths.find(cat) != g_paths.end()) return g_paths[cat];

    const char* config_home = g_get_user_config_dir();
    std::string dir = std::string(config_home) + "/kerneldrive/asus_armoury";
    
    std::string filename;
    switch(cat) {
        case ConfigCategory::RGB: filename = "rgb.ini"; break;
        case ConfigCategory::Power: filename = "power.ini"; break;
        case ConfigCategory::Extra: filename = "extra.ini"; break;
    }
    
    std::string full_path = dir + "/" + filename;
    g_paths[cat] = full_path;
    return full_path;
}

void AsusConfig::ensure_dir(const std::string& path) {
    std::filesystem::path p(path);
    if (!p.parent_path().empty()) {
        std::filesystem::create_directories(p.parent_path());
    }
}

GKeyFile* AsusConfig::get_file(ConfigCategory cat) {
    if (g_files.find(cat) != g_files.end()) return g_files[cat];

    GKeyFile* f = g_key_file_new();
    std::string path = get_path(cat);

    GError* error = nullptr;
    if (!g_key_file_load_from_file(f, path.c_str(), G_KEY_FILE_NONE, &error)) {
        if (!g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
            // std::cerr << "[AsusConfig] Load failed for " << path << ": " << error->message << std::endl;
        }
        g_error_free(error);
    }
    
    g_files[cat] = f;
    return f;
}

void AsusConfig::save(ConfigCategory cat) {
    GKeyFile* f = get_file(cat);
    std::string path = get_path(cat);
    ensure_dir(path);
    
    GError* error = nullptr;
    if (!g_key_file_save_to_file(f, path.c_str(), &error)) {
        std::cerr << "[AsusConfig] Save failed for " << path << ": " << error->message << std::endl;
        g_error_free(error);
    }
}

void AsusConfig::set_string(ConfigCategory cat, const std::string& group, const std::string& key, const std::string& val) {
    GKeyFile* f = get_file(cat);
    g_key_file_set_string(f, group.c_str(), key.c_str(), val.c_str());
    save(cat);
}

std::string AsusConfig::get_string(ConfigCategory cat, const std::string& group, const std::string& key, const std::string& def) {
    GKeyFile* f = get_file(cat);
    GError* err = nullptr;
    gchar* val = g_key_file_get_string(f, group.c_str(), key.c_str(), &err);
    if (err) { g_error_free(err); return def; }
    std::string s = val;
    g_free(val);
    return s;
}

void AsusConfig::set_int(ConfigCategory cat, const std::string& group, const std::string& key, int val) {
    GKeyFile* f = get_file(cat);
    g_key_file_set_integer(f, group.c_str(), key.c_str(), val);
    save(cat);
}

int AsusConfig::get_int(ConfigCategory cat, const std::string& group, const std::string& key, int def) {
    GKeyFile* f = get_file(cat);
    GError* err = nullptr;
    int val = g_key_file_get_integer(f, group.c_str(), key.c_str(), &err);
    if (err) { g_error_free(err); return def; }
    return val;
}

void AsusConfig::set_bool(ConfigCategory cat, const std::string& group, const std::string& key, bool val) {
    GKeyFile* f = get_file(cat);
    g_key_file_set_boolean(f, group.c_str(), key.c_str(), val);
    save(cat);
}

bool AsusConfig::get_bool(ConfigCategory cat, const std::string& group, const std::string& key, bool def) {
    GKeyFile* f = get_file(cat);
    GError* err = nullptr;
    gboolean val = g_key_file_get_boolean(f, group.c_str(), key.c_str(), &err);
    if (err) { g_error_free(err); return def; }
    return val;
}
