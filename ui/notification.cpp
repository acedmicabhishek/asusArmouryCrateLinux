#include "notification.h"
#include <iostream>

extern "C" void kd_show_toast(const char* msg) __attribute__((weak));

void AsusNotification::init() {
}

void AsusNotification::show_toast(const std::string& msg) {
    if (kd_show_toast) {
        kd_show_toast(msg.c_str());
    } else {
        std::cerr << "[AsusNotification] kd_show_toast symbol not found!" << std::endl;
    }
}

AsusNotification::LoadingHandle* AsusNotification::show_loading(GtkWidget* parent, const std::string& msg) {
    AdwDialog* dialog = adw_dialog_new();
    adw_dialog_set_title(dialog, "Please Wait");
    adw_dialog_set_content_width(dialog, 300);
    adw_dialog_set_content_height(dialog, 100);
    
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_margin_top(box, 30);
    gtk_widget_set_margin_bottom(box, 30);
    gtk_widget_set_margin_start(box, 20);
    gtk_widget_set_margin_end(box, 20);
    
    GtkWidget* spinner = gtk_spinner_new();
    gtk_widget_set_size_request(spinner, 32, 32);
    gtk_spinner_start(GTK_SPINNER(spinner));
    
    GtkWidget* label = gtk_label_new(msg.c_str());
    gtk_widget_add_css_class(label, "title-2");
    
    gtk_box_append(GTK_BOX(box), spinner);
    gtk_box_append(GTK_BOX(box), label);
    
    adw_dialog_set_child(dialog, box);
    
    GtkWidget* root = parent;
    if (!GTK_IS_ROOT(root)) root = GTK_WIDGET(gtk_widget_get_root(parent));
    
    adw_dialog_present(dialog, root);
    
    LoadingHandle* handle = new LoadingHandle{dialog, spinner, label};
    return handle;
}

void AsusNotification::LoadingHandle::close() {
    if (dialog) {
        adw_dialog_force_close(dialog);
    }
    delete this;
}
