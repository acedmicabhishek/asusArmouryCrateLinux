#pragma once
#include <string>
#include <gtk/gtk.h>
#include <adwaita.h>

class AsusNotification {
public:
    static void init();
    static void show_toast(const std::string& msg);
    
    struct LoadingHandle {
        AdwDialog* dialog;
        GtkWidget* spinner;
        GtkWidget* label;
        void close();
    };
    
    static LoadingHandle* show_loading(GtkWidget* parent, const std::string& msg);
};
