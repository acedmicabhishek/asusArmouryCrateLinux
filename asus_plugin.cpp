#include "backend/battery.h"
#include "backend/brightness.h"
#include "backend/config.h"
#include "backend/core.h"
#include "backend/display.h"
#include "backend/fan_control.h"
#include "backend/gpu.h"
#include "backend/gpu_mux.h"
#include "backend/keyboard.h"
#include "backend/modes.h"
#include "backend/monitor.h"
#include "backend/stress.h"
#include "backend/visuals.h"
#include "plugin_interface.h"
#include "ui/notification.h"
#include <adwaita.h>
#include <filesystem>
#include <gtk/gtk.h>
#include <iostream>
#include <thread>

class AsusArmouryPlugin : public KdPlugin {
public:
  std::string get_name() const override { return "Asus Armoury Control"; }

  std::string get_slug() const override { return "asus-armoury-control"; }

  bool init() override {
    return AsusModes::is_supported() || AsusCore::is_supported() ||
           AsusBattery::is_supported();
  }

  GtkWidget *create_config_widget() override {
    AsusConfig::init();
    AsusMonitor::init();
    AsusBrightness::init();
    AsusKeyboard::init();
    AsusModes::init();
    AsusBattery::init();
    AsusFanControl::init();
    AsusDisplay::init();

    GtkWidget *page = adw_preferences_page_new();
    GtkWidget *group = adw_preferences_group_new();
    adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(group),
                                    "Performance Modes");
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(page),
                             ADW_PREFERENCES_GROUP(group));

    GtkWidget *status_row = adw_action_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(status_row),
                                  AsusBattery::is_on_ac()
                                      ? "Current Mode (Charger)"
                                      : "Current Mode (Battery)");
    adw_action_row_set_subtitle(ADW_ACTION_ROW(status_row), "Reading...");
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), status_row);

    auto update_status_text = [](GtkWidget *row, AsusMode mode) {
      switch (mode) {
      case AsusMode::Silent:
        adw_action_row_set_subtitle(ADW_ACTION_ROW(row), "Silent");
        break;
      case AsusMode::Balanced:
        adw_action_row_set_subtitle(ADW_ACTION_ROW(row), "Balanced");
        break;
      case AsusMode::Turbo:
        adw_action_row_set_subtitle(ADW_ACTION_ROW(row), "Turbo");
        break;
      default:
        adw_action_row_set_subtitle(ADW_ACTION_ROW(row), "Unknown");
        break;
      }
    };

    GtkWidget *fan_group = adw_preferences_group_new();
    adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(fan_group), "Core");
    adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(fan_group), "Core");
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(page),
                             ADW_PREFERENCES_GROUP(fan_group));

    // Advanced Toggle
    GtkWidget *adv_row = adw_action_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(adv_row),
                                  "Advanced Stats");
    GtkWidget *adv_switch = gtk_switch_new();
    gtk_widget_set_valign(adv_switch, GTK_ALIGN_CENTER);
    adw_action_row_add_suffix(ADW_ACTION_ROW(adv_row), adv_switch);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(fan_group), adv_row);

    // Manual Fan Control
    if (AsusFanControl::is_supported()) {
      GtkWidget *manual_group = adw_preferences_group_new();
      adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(manual_group),
                                      "Manual Fan Control");
      adw_preferences_page_add(ADW_PREFERENCES_PAGE(page),
                               ADW_PREFERENCES_GROUP(manual_group));

      GtkWidget *manual_row = adw_switch_row_new();
      adw_preferences_row_set_title(ADW_PREFERENCES_ROW(manual_row),
                                    "Enable Manual Mode");
      adw_action_row_set_subtitle(ADW_ACTION_ROW(manual_row),
                                  "Override BIOS fan curves.");

      adw_switch_row_set_active(ADW_SWITCH_ROW(manual_row),
                                AsusFanControl::get_manual_mode());

      GtkWidget *speed_row = adw_action_row_new();
      adw_preferences_row_set_title(ADW_PREFERENCES_ROW(speed_row),
                                    "Fan Speed");
      GtkWidget *speed_scale =
          gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 5);
      gtk_widget_set_hexpand(speed_scale, TRUE);
      gtk_range_set_value(GTK_RANGE(speed_scale),
                          AsusFanControl::get_fan_speed());
      adw_action_row_add_suffix(ADW_ACTION_ROW(speed_row), speed_scale);

      gtk_widget_set_visible(speed_row, AsusFanControl::get_manual_mode());

      g_signal_connect(
          manual_row, "notify::active",
          G_CALLBACK(+[](GObject *row, GParamSpec *, gpointer data) {
            bool active = adw_switch_row_get_active(ADW_SWITCH_ROW(row));
            GtkWidget *s_row = GTK_WIDGET(data);

            if (AsusFanControl::set_manual_mode(active)) {
              gtk_widget_set_visible(s_row, active);
            } else {
              g_signal_handlers_block_by_func(
                  row,
                  (gpointer)G_CALLBACK(
                      +[](GObject *, GParamSpec *, gpointer) {}),
                  data);
              adw_switch_row_set_active(ADW_SWITCH_ROW(row), !active);
              g_signal_handlers_unblock_by_func(
                  row,
                  (gpointer)G_CALLBACK(
                      +[](GObject *, GParamSpec *, gpointer) {}),
                  data);

              AdwDialog *dlg = adw_alert_dialog_new(
                  "BIOS Locked",
                  "Your BIOS rejected the manual fan control request.");
              adw_alert_dialog_add_response(ADW_ALERT_DIALOG(dlg), "ok", "OK");
              GtkWidget *root =
                  GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(row)));
              adw_dialog_present(dlg, root);
            }
          }),
          speed_row);

      g_signal_connect(speed_scale, "value-changed",
                       G_CALLBACK(+[](GtkRange *range, gpointer) {
                         int val = (int)gtk_range_get_value(range);
                         AsusFanControl::set_fan_speed(val);
                       }),
                       NULL);

      adw_preferences_group_add(ADW_PREFERENCES_GROUP(manual_group),
                                manual_row);
      adw_preferences_group_add(ADW_PREFERENCES_GROUP(manual_group), speed_row);
    }

    std::string cpu_name = "CPU • " + AsusCore::get_cpu_name();
    std::string gpu_name = "GPU • " + AsusCore::get_gpu_name();

    GtkWidget *cpu_row = adw_action_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(cpu_row),
                                  cpu_name.c_str());
    adw_action_row_set_subtitle(ADW_ACTION_ROW(cpu_row), "0 RPM");
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(fan_group), cpu_row);

    GtkWidget *gpu_row = adw_action_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(gpu_row),
                                  gpu_name.c_str());

    bool dgpu_available = true;
    if (gpu_name.find("dGPU Unavailable") != std::string::npos) {
      dgpu_available = false;
      adw_action_row_set_subtitle(ADW_ACTION_ROW(gpu_row),
                                  "Not detected (Eco Mode?)");
      gtk_widget_set_sensitive(gpu_row, FALSE);
    } else {
      adw_action_row_set_subtitle(ADW_ACTION_ROW(gpu_row), "0 RPM");
    }
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(fan_group), gpu_row);

    // Stress Test Buttons
    GtkWidget *stress_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(stress_box, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(stress_box, 10);

    GtkWidget *cpu_stress_btn = gtk_button_new_with_label("Stress CPU");
    GtkWidget *gpu_stress_btn = gtk_button_new_with_label("Stress GPU");
    gtk_widget_add_css_class(cpu_stress_btn, "destructive-action");
    gtk_widget_add_css_class(gpu_stress_btn, "destructive-action");

    if (!dgpu_available) {
      gtk_widget_set_sensitive(gpu_stress_btn, FALSE);
      gtk_widget_set_tooltip_text(gpu_stress_btn, "dGPU is unavailable.");
    }

    struct StressData {
      GtkWidget *c_btn;
      GtkWidget *g_btn;
      GtkWidget *page;
    };
    StressData *sdata = new StressData{cpu_stress_btn, gpu_stress_btn, page};

    auto show_install_error = [](GtkWidget *parent, const char *app) {
      std::string msg =
          std::string("Please install '") + app + "' to use this feature.";
      AdwDialog *dlg = adw_alert_dialog_new("Missing Component", msg.c_str());
      adw_alert_dialog_add_response(ADW_ALERT_DIALOG(dlg), "ok", "OK");
      adw_dialog_present(dlg, parent);
    };
    struct CallbackCtx {
      StressData *s;
      decltype(show_install_error) err_fn;
    };
    CallbackCtx *ctx = new CallbackCtx{sdata, show_install_error};

    g_signal_connect(cpu_stress_btn, "clicked",
                     G_CALLBACK(+[](GtkButton *btn, gpointer user_data) {
                       CallbackCtx *c = (CallbackCtx *)user_data;
                       if (AsusStress::is_cpu_stress_running()) {
                         AsusStress::stop_cpu_stress();
                         gtk_button_set_label(btn, "Stress CPU");
                       } else {
                         if (AsusStress::has_cpu_burn()) {
                           if (AsusStress::start_cpu_stress()) {
                             gtk_button_set_label(btn, "Stop CPU");
                           }
                         } else {
                           c->err_fn(GTK_WIDGET(btn), "stress-ng");
                         }
                       }
                     }),
                     ctx);

    g_signal_connect(gpu_stress_btn, "clicked",
                     G_CALLBACK(+[](GtkButton *btn, gpointer user_data) {
                       CallbackCtx *c = (CallbackCtx *)user_data;
                       if (AsusStress::is_gpu_stress_running()) {
                         AsusStress::stop_gpu_stress();
                         gtk_button_set_label(btn, "Stress GPU");
                       } else {
                         if (AsusStress::has_gpu_burn()) {
                           if (AsusStress::start_gpu_stress()) {
                             gtk_button_set_label(btn, "Stop GPU");
                           }
                         } else {
                           c->err_fn(GTK_WIDGET(btn), "gpu_burn");
                         }
                       }
                     }),
                     ctx);

    g_timeout_add(
        1000,
        +[](gpointer user_data) -> gboolean {
          StressData *s = (StressData *)user_data;
          if (!GTK_IS_WIDGET(s->c_btn))
            return G_SOURCE_REMOVE;

          if (!AsusStress::is_cpu_stress_running())
            gtk_button_set_label(GTK_BUTTON(s->c_btn), "Stress CPU");
          if (!AsusStress::is_gpu_stress_running())
            gtk_button_set_label(GTK_BUTTON(s->g_btn), "Stress GPU");

          return G_SOURCE_CONTINUE;
        },
        sdata);

    gtk_box_append(GTK_BOX(stress_box), cpu_stress_btn);
    gtk_box_append(GTK_BOX(stress_box), gpu_stress_btn);

    adw_preferences_group_add(ADW_PREFERENCES_GROUP(fan_group), stress_box);

    // Dynamic Boost (nvidia-powerd)
    if (AsusGpu::is_dynamic_boost_supported()) {
      GtkWidget *boost_row = adw_switch_row_new();
      adw_preferences_row_set_title(ADW_PREFERENCES_ROW(boost_row),
                                    "Dynamic Boost");

      if (dgpu_available) {
        adw_action_row_set_subtitle(
            ADW_ACTION_ROW(boost_row),
            "Shifts 15W-25W power from CPU to GPU (nvidia-powerd).");
        adw_switch_row_set_active(ADW_SWITCH_ROW(boost_row),
                                  AsusGpu::get_dynamic_boost());

        g_signal_connect(boost_row, "notify::active",
                         G_CALLBACK(+[](GObject *row, GParamSpec *, gpointer) {
                           bool active =
                               adw_switch_row_get_active(ADW_SWITCH_ROW(row));
                           if (!AsusGpu::set_dynamic_boost(active)) {
                           }
                         }),
                         NULL);
      } else {
        adw_action_row_set_subtitle(ADW_ACTION_ROW(boost_row),
                                    "Unavailable (dGPU is off).");
        gtk_widget_set_sensitive(boost_row, FALSE);
      }

      adw_preferences_group_add(ADW_PREFERENCES_GROUP(fan_group), boost_row);
    }

    // GPU Mux Switch
    if (AsusMux::is_supported()) {
      GtkWidget *mux_row = adw_combo_row_new();
      adw_preferences_row_set_title(ADW_PREFERENCES_ROW(mux_row), "GPU Mode");
      adw_action_row_set_subtitle(ADW_ACTION_ROW(mux_row),
                                  "Requires reboot to apply.");

      const char *modes[] = {"Standard (Hybrid)", "Eco (Integrated)",
                             "Ultimate (Nvidia)", NULL};
      GtkStringList *list = gtk_string_list_new(modes);
      adw_combo_row_set_model(ADW_COMBO_ROW(mux_row), G_LIST_MODEL(list));

      AsusMux::Mode current = AsusMux::get_mode();
      int idx = 0;
      if (current == AsusMux::Mode::Integrated)
        idx = 1;
      if (current == AsusMux::Mode::Nvidia)
        idx = 2;

      adw_combo_row_set_selected(ADW_COMBO_ROW(mux_row), idx);

      auto gpu_cb = +[](GObject *row, GParamSpec *, gpointer) {
        int idx = adw_combo_row_get_selected(ADW_COMBO_ROW(row));
        AsusMux::Mode m = AsusMux::Mode::Hybrid;
        if (idx == 1)
          m = AsusMux::Mode::Integrated;
        if (idx == 2)
          m = AsusMux::Mode::Nvidia;

        auto handle = AsusNotification::show_loading(
            GTK_WIDGET(row), "Switching GPU Mode...\n(May ask for password)");

        struct AsyncData {
          AsusNotification::LoadingHandle *h;
          AsusMux::Mode m;
        };
        AsyncData *ad = new AsyncData{handle, m};

        g_timeout_add(
            100,
            +[](gpointer ptr) -> gboolean {
              AsyncData *ad = (AsyncData *)ptr;

              bool success = AsusMux::set_mode(ad->m);

              ad->h->close();

              if (success) {
                AsusNotification::show_toast("GPU Mode Set. Please Reboot.");
              } else {
                AsusNotification::show_toast("Failed to set GPU Mode.");
              }

              delete ad;
              return G_SOURCE_REMOVE;
            },
            ad);
      };

      g_signal_connect(mux_row, "notify::selected", G_CALLBACK(gpu_cb), NULL);

      adw_preferences_group_add(ADW_PREFERENCES_GROUP(fan_group), mux_row);
    }
    // screen
    if (AsusBrightness::is_supported()) {
      GtkWidget *screen_group = adw_preferences_group_new();
      adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(screen_group),
                                      "Screen");
      adw_preferences_page_add(ADW_PREFERENCES_PAGE(page),
                               ADW_PREFERENCES_GROUP(screen_group));

      GtkWidget *bright_row = adw_action_row_new();
      adw_preferences_row_set_title(ADW_PREFERENCES_ROW(bright_row),
                                    "Brightness");

      GtkWidget *bright_scale =
          gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
      gtk_widget_set_hexpand(bright_scale, TRUE);
      gtk_range_set_value(GTK_RANGE(bright_scale),
                          AsusBrightness::get_brightness());

      g_signal_connect(bright_scale, "value-changed",
                       G_CALLBACK(+[](GtkRange *range, gpointer data) {
                         (void)data;
                         int val = (int)gtk_range_get_value(range);
                         AsusBrightness::set_brightness(val);
                       }),
                       NULL);

      adw_action_row_add_suffix(ADW_ACTION_ROW(bright_row), bright_scale);
      adw_preferences_group_add(ADW_PREFERENCES_GROUP(screen_group),
                                bright_row);
    }

    // Keyboard
    if (AsusKeyboard::is_supported()) {
      GtkWidget *kbd_group = adw_preferences_group_new();
      adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(kbd_group),
                                      "Keyboard");
      adw_preferences_page_add(ADW_PREFERENCES_PAGE(page),
                               ADW_PREFERENCES_GROUP(kbd_group));

      // Backlight
      GtkWidget *bl_row = adw_action_row_new();
      adw_preferences_row_set_title(ADW_PREFERENCES_ROW(bl_row), "Backlight");

      // 0, 1, 2, 3
      GtkWidget *bl_scale = gtk_scale_new_with_range(
          GTK_ORIENTATION_HORIZONTAL, 0, AsusKeyboard::get_max_brightness(), 1);
      gtk_widget_set_hexpand(bl_scale, TRUE);
      gtk_scale_set_draw_value(GTK_SCALE(bl_scale), FALSE);
      gtk_scale_add_mark(GTK_SCALE(bl_scale), 0, GTK_POS_BOTTOM, "Off");
      gtk_scale_add_mark(GTK_SCALE(bl_scale),
                         AsusKeyboard::get_max_brightness(), GTK_POS_BOTTOM,
                         "Max");

      gtk_range_set_value(GTK_RANGE(bl_scale), AsusKeyboard::get_brightness());

      g_signal_connect(bl_scale, "value-changed",
                       G_CALLBACK(+[](GtkRange *range, gpointer data) {
                         (void)data;
                         int val = (int)gtk_range_get_value(range);
                         AsusKeyboard::set_brightness(val);
                       }),
                       NULL);

      adw_action_row_add_suffix(ADW_ACTION_ROW(bl_row), bl_scale);
      adw_preferences_group_add(ADW_PREFERENCES_GROUP(kbd_group), bl_row);

      // RGB Mode
      if (AsusKeyboard::has_rgb()) {
        GtkWidget *rgb_row = adw_combo_row_new();
        adw_preferences_row_set_title(ADW_PREFERENCES_ROW(rgb_row), "RGB Mode");
        adw_action_row_set_subtitle(ADW_ACTION_ROW(rgb_row), "Lighting Effect");

        const char *items[] = {"Static", "Breathing", "Color Cycle", "Strobe",
                               NULL};
        GtkStringList *list = gtk_string_list_new(items);

        adw_combo_row_set_model(ADW_COMBO_ROW(rgb_row), G_LIST_MODEL(list));

        // Default to Static
        adw_combo_row_set_selected(ADW_COMBO_ROW(rgb_row),
                                   (guint)AsusKeyboard::get_current_mode());

        // Speed Slider
        GtkWidget *speed_row = adw_action_row_new();
        adw_preferences_row_set_title(ADW_PREFERENCES_ROW(speed_row),
                                      "Animation Speed");

        GtkWidget *speed_scale =
            gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 2, 1);
        gtk_widget_set_hexpand(speed_scale, TRUE);
        gtk_scale_set_draw_value(GTK_SCALE(speed_scale), FALSE);
        gtk_scale_add_mark(GTK_SCALE(speed_scale), 0, GTK_POS_BOTTOM, "Slow");
        gtk_scale_add_mark(GTK_SCALE(speed_scale), 1, GTK_POS_BOTTOM, "Med");
        gtk_scale_add_mark(GTK_SCALE(speed_scale), 2, GTK_POS_BOTTOM, "Fast");

        gtk_range_set_value(GTK_RANGE(speed_scale),
                            AsusKeyboard::get_current_speed());

        adw_action_row_add_suffix(ADW_ACTION_ROW(speed_row), speed_scale);

        // Mode Change
        g_signal_connect(
            rgb_row, "notify::selected",
            G_CALLBACK(+[](GObject *row, GParamSpec *, gpointer data) {
              guint idx = adw_combo_row_get_selected(ADW_COMBO_ROW(row));
              GtkWidget *s_row = GTK_WIDGET(data);

              AsusKeyboard::RgbMode m = AsusKeyboard::RgbMode::Static;
              if (idx == 1)
                m = AsusKeyboard::RgbMode::Breathing;
              if (idx == 2)
                m = AsusKeyboard::RgbMode::Cycle;
              if (idx == 3)
                m = AsusKeyboard::RgbMode::Strobe;

              AsusKeyboard::set_rgb_mode(m, AsusKeyboard::get_current_speed());
              gtk_widget_set_visible(s_row, m != AsusKeyboard::RgbMode::Static);
            }),
            speed_row);
        g_signal_connect(speed_scale, "value-changed",
                         G_CALLBACK(+[](GtkRange *range, gpointer) {
                           int val = (int)gtk_range_get_value(range);
                           AsusKeyboard::set_rgb_mode(
                               AsusKeyboard::get_current_mode(), val);
                         }),
                         NULL);

        adw_preferences_group_add(ADW_PREFERENCES_GROUP(kbd_group), rgb_row);
        gtk_widget_set_visible(speed_row, AsusKeyboard::get_current_mode() !=
                                              AsusKeyboard::RgbMode::Static);
        adw_preferences_group_add(ADW_PREFERENCES_GROUP(kbd_group), speed_row);

        GtkWidget *color_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_widget_set_margin_top(color_box, 15);
        gtk_widget_set_margin_bottom(color_box, 15);
        gtk_widget_set_halign(color_box, GTK_ALIGN_CENTER);

        GtkWidget *grid_label = gtk_label_new("Quick Colors");
        gtk_widget_add_css_class(grid_label, "dim-label");
        gtk_box_append(GTK_BOX(color_box), grid_label);

        GtkWidget *flow = gtk_flow_box_new();
        gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(flow), 4);
        gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow), 8);
        gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flow), GTK_SELECTION_NONE);
        gtk_widget_set_halign(flow, GTK_ALIGN_CENTER);

        struct ColorDef {
          const char *name;
          const char *css;
          int r;
          int g;
          int b;
        };
        std::vector<ColorDef> colors = {
            {"Red", "background: #FF0000;", 255, 0, 0},
            {"Green", "background: #00FF00;", 0, 255, 0},
            {"Blue", "background: #0000FF;", 0, 0, 255},
            {"Cyan", "background: #00FFFF;", 0, 255, 255},
            {"Magenta", "background: #FF00FF;", 255, 0, 255},
            {"Yellow", "background: #FFFF00;", 255, 255, 0},
            {"White", "background: #FFFFFF;", 255, 255, 255},
            {"Orange", "background: #FFA500;", 255, 165, 0}};

        for (const auto &c : colors) {
          GtkWidget *btn = gtk_button_new();
          gtk_widget_set_size_request(btn, 40, 40);

          GtkCssProvider *provider = gtk_css_provider_new();
          std::string css =
              "button { " + std::string(c.css) +
              " border-radius: 20px; box-shadow: none; border: 1px solid "
              "rgba(0,0,0,0.2); } button:active { opacity: 0.8; }";
          gtk_css_provider_load_from_data(provider, css.c_str(), -1);
          gtk_style_context_add_provider(
              gtk_widget_get_style_context(btn), GTK_STYLE_PROVIDER(provider),
              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

          gtk_widget_set_tooltip_text(btn, c.name);

          struct ColorData {
            int r;
            int g;
            int b;
          };
          ColorData *cd = new ColorData{c.r, c.g, c.b};

          g_signal_connect_data(
              btn, "clicked", G_CALLBACK(+[](GtkButton *, gpointer user_data) {
                ColorData *data = (ColorData *)user_data;
                AsusKeyboard::set_color(data->r, data->g, data->b);
              }),
              cd,
              [](gpointer data, GClosure *) {
                delete static_cast<ColorData *>(data);
              },
              (GConnectFlags)0);

          gtk_flow_box_append(GTK_FLOW_BOX(flow), btn);
        }

        gtk_box_append(GTK_BOX(color_box), flow);
        adw_preferences_group_add(ADW_PREFERENCES_GROUP(kbd_group), color_box);

        GtkWidget *picker_row = adw_action_row_new();
        adw_preferences_row_set_title(ADW_PREFERENCES_ROW(picker_row),
                                      "Custom Color");
        adw_action_row_set_subtitle(ADW_ACTION_ROW(picker_row),
                                    "Pick any color.");

        GtkWidget *color_btn = gtk_color_button_new();
        gtk_widget_set_valign(color_btn, GTK_ALIGN_CENTER);

        GdkRGBA initial_color = {1.0, 0.0, 0.0, 1.0};
        gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(color_btn),
                                   &initial_color);

        g_signal_connect(color_btn, "color-set",
                         G_CALLBACK(+[](GtkColorButton *btn, gpointer) {
                           GdkRGBA c;
                           gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(btn),
                                                      &c);

                           int r = (int)(c.red * 255.0);
                           int g = (int)(c.green * 255.0);
                           int b = (int)(c.blue * 255.0);

                           AsusKeyboard::set_color(r, g, b);
                         }),
                         NULL);

        adw_action_row_add_suffix(ADW_ACTION_ROW(picker_row), color_btn);
        adw_preferences_group_add(ADW_PREFERENCES_GROUP(kbd_group), picker_row);
      }
    }
    struct FanData {
      GtkWidget *cpu;
      GtkWidget *gpu;
      GtkWidget *adv;
      GtkWidget *switcher_stack;
      GtkWidget *power_row;
      GtkWidget *status_row;
      GtkWidget *install_btn;
      GtkWidget *install_row;
      decltype(update_status_text) update_fn;
      bool last_ac;
    };
    FanData *fdata = new FanData{
        cpu_row, gpu_row,    adv_switch,         nullptr,
        nullptr, status_row, nullptr,            nullptr,
        update_status_text,  AsusBattery::is_on_ac()};

    guint timeout_id = g_timeout_add(
        2000,
        +[](gpointer user_data) -> gboolean {
          AsusModes::update_auto();
          FanData *d = static_cast<FanData *>(user_data);

          bool current_ac = AsusBattery::is_on_ac();
          if (current_ac != d->last_ac) {
            d->last_ac = current_ac;
            if (d->switcher_stack)
              gtk_stack_set_visible_child_name(GTK_STACK(d->switcher_stack),
                                               current_ac ? "ac" : "dc");
            if (d->power_row) {
              adw_action_row_set_subtitle(ADW_ACTION_ROW(d->power_row),
                                          current_ac
                                              ? "\u26A1 Charger (AC)"
                                              : "\U0001F50B Battery (DC)");
            }
            if (d->status_row) {
              adw_preferences_row_set_title(ADW_PREFERENCES_ROW(d->status_row),
                                            current_ac
                                                ? "Current Mode (Charger)"
                                                : "Current Mode (Battery)");
              d->update_fn(d->status_row, AsusModes::get_mode());
            }
          }

          if (!GTK_IS_WIDGET(d->cpu))
            return G_SOURCE_CONTINUE;

          AsusCore::FanMetrics m = AsusCore::get_metrics();

          char buf[64];

          if (m.cpu_temp > 0)
            snprintf(buf, sizeof(buf), "%d RPM  •  %d°C", m.cpu_rpm,
                     m.cpu_temp);
          else
            snprintf(buf, sizeof(buf), "%d RPM", m.cpu_rpm);

          std::string cpu_sub = buf;
          if (gtk_switch_get_active(GTK_SWITCH(d->adv))) {
            auto cm = AsusMonitor::get_cpu_metrics();
            if (cm.freq_mhz > 0)
              cpu_sub += "  •  " + AsusMonitor::format_freq(cm.freq_mhz);
            if (cm.ram_mt_s > 0)
              cpu_sub += "  •  " + AsusMonitor::format_speed(cm.ram_mt_s);
            if (cm.power_w > 0)
              cpu_sub += "  •  " + std::to_string(cm.power_w) + " W";
          }
          adw_action_row_set_subtitle(ADW_ACTION_ROW(d->cpu), cpu_sub.c_str());

          if (m.gpu_temp > 0)
            snprintf(buf, sizeof(buf), "%d RPM  •  %d°C", m.gpu_rpm,
                     m.gpu_temp);
          else
            snprintf(buf, sizeof(buf), "%d RPM", m.gpu_rpm);

          if (m.gpu_temp > 0)
            snprintf(buf, sizeof(buf), "%d RPM  •  %d°C", m.gpu_rpm,
                     m.gpu_temp);
          else
            snprintf(buf, sizeof(buf), "%d RPM", m.gpu_rpm);

          std::string gpu_sub = buf;
          if (gtk_switch_get_active(GTK_SWITCH(d->adv))) {
            auto gm = AsusMonitor::get_gpu_metrics();
            if (gm.core_clock_mhz > 0)
              gpu_sub += "  •  " + std::to_string(gm.core_clock_mhz) + " MHz";
            if (gm.vram_used_mb > 0)
              gpu_sub += "  •  " + std::to_string(gm.vram_used_mb) + " MB";
            if (gm.power_w > 0 || gm.power_limit_w > 0)
              gpu_sub += "  •  " + std::to_string(gm.power_w) + "W / " +
                         std::to_string(gm.power_limit_w) + "W";
          }
          adw_action_row_set_subtitle(ADW_ACTION_ROW(d->gpu), gpu_sub.c_str());

          if (d->install_btn) {
            bool installed = std::filesystem::exists("/usr/local/bin/AAC");
            if (installed) {
              adw_preferences_row_set_title(ADW_PREFERENCES_ROW(d->install_row),
                                            "AAC CLI Tool (Installed)");
              gtk_button_set_label(GTK_BUTTON(d->install_btn),
                                   "Uninstall AAC Tool");
              gtk_widget_remove_css_class(d->install_btn, "suggested-action");
              gtk_widget_add_css_class(d->install_btn, "destructive-action");
            } else {
              adw_preferences_row_set_title(ADW_PREFERENCES_ROW(d->install_row),
                                            "AAC CLI Tool");
              gtk_button_set_label(GTK_BUTTON(d->install_btn),
                                   "Install AAC Tool");
              gtk_widget_remove_css_class(d->install_btn, "destructive-action");
              gtk_widget_add_css_class(d->install_btn, "suggested-action");
            }
          }

          return G_SOURCE_CONTINUE;
        },
        fdata);

    g_signal_connect(page, "destroy",
                     G_CALLBACK(+[](GtkWidget *, gpointer data) {
                       guint id = GPOINTER_TO_UINT(data);
                       g_source_remove(id);
                     }),
                     GUINT_TO_POINTER(timeout_id));

    g_object_set_data_full(
        G_OBJECT(page), "fan-data", fdata,
        +[](gpointer data) { delete static_cast<FanData *>(data); });

    // Battery
    GtkWidget *bat_group = adw_preferences_group_new();
    adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(bat_group),
                                    "Battery Health");
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(page),
                             ADW_PREFERENCES_GROUP(bat_group));

    GtkWidget *limit_row = adw_spin_row_new_with_range(40, 100, 1);
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(limit_row),
                                  "Charge Limit %");
    adw_action_row_set_subtitle(
        ADW_ACTION_ROW(limit_row),
        "Stop charging at this percentage to extend lifespan.");
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(bat_group), limit_row);

    int current_limit = AsusBattery::get_charge_limit();
    if (current_limit > 0) {
      adw_spin_row_set_value(ADW_SPIN_ROW(limit_row), current_limit);
    }

    // Apply on change
    g_signal_connect(
        limit_row, "notify::value",
        G_CALLBACK(+[](GObject *row, GParamSpec *, gpointer) {
          int val = (int)adw_spin_row_get_value(ADW_SPIN_ROW(row));
          std::cout << "[AsusPlugin] Setting charge limit to: " << val << "%"
                    << std::endl;
          if (AsusBattery::set_charge_limit(val)) {
            std::cout << "[AsusPlugin] Limit applied." << std::endl;
          } else {
            std::cerr << "[AsusPlugin] Failed to set limit." << std::endl;
          }
        }),
        NULL);

    // Display
    GtkWidget *disp_group = adw_preferences_group_new();
    adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(disp_group),
                                    "Display");
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(page),
                             ADW_PREFERENCES_GROUP(disp_group));

    // 1. Refresh Rate
    GtkWidget *rate_row = adw_action_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(rate_row),
                                  "Refresh Rate");
    adw_action_row_set_subtitle(ADW_ACTION_ROW(rate_row),
                                "Control screen smoothness.");

    GtkWidget *rate_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

    std::vector<int> rates = AsusDisplay::get_available_refresh_rates();
    for (int hz : rates) {
      GtkWidget *btn =
          gtk_button_new_with_label((std::to_string(hz) + "Hz").c_str());
      g_signal_connect(btn, "clicked",
                       G_CALLBACK(+[](GtkButton *, gpointer data) {
                         int h = GPOINTER_TO_INT(data);
                         AsusDisplay::set_refresh_rate(h);
                       }),
                       GINT_TO_POINTER(hz));
      gtk_box_append(GTK_BOX(rate_box), btn);
    }
    adw_action_row_add_suffix(ADW_ACTION_ROW(rate_row), rate_box);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(disp_group), rate_row);

    // 2. Panel Overdrive
    GtkWidget *od_row = adw_switch_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(od_row),
                                  "Panel Overdrive");

    if (AsusDisplay::is_overdrive_supported()) {
      adw_action_row_set_subtitle(ADW_ACTION_ROW(od_row),
                                  "Reduces ghosting but may cause overshoot.");
      adw_switch_row_set_active(ADW_SWITCH_ROW(od_row),
                                AsusDisplay::get_panel_overdrive());
      g_signal_connect(od_row, "notify::active",
                       G_CALLBACK(+[](GObject *row, GParamSpec *, gpointer) {
                         bool active =
                             adw_switch_row_get_active(ADW_SWITCH_ROW(row));
                         AsusDisplay::set_panel_overdrive(active);
                       }),
                       NULL);
    } else {
      adw_action_row_set_subtitle(ADW_ACTION_ROW(od_row),
                                  "Not supported by your device.");
      gtk_widget_set_sensitive(od_row, FALSE);
    }
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(disp_group), od_row);

    // 3. Panel Power Saver (MiniLED)
    GtkWidget *miniled_row = adw_switch_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(miniled_row),
                                  "Panel Power Saver");

    if (AsusDisplay::is_miniled_supported()) {
      adw_action_row_set_subtitle(ADW_ACTION_ROW(miniled_row),
                                  "Optimizes MiniLED zones for power saving.");
      adw_switch_row_set_active(ADW_SWITCH_ROW(miniled_row),
                                AsusDisplay::get_miniled_mode());
      g_signal_connect(miniled_row, "notify::active",
                       G_CALLBACK(+[](GObject *row, GParamSpec *, gpointer) {
                         bool active =
                             adw_switch_row_get_active(ADW_SWITCH_ROW(row));
                         AsusDisplay::set_miniled_mode(active);
                       }),
                       NULL);
    } else {
      adw_action_row_set_subtitle(ADW_ACTION_ROW(miniled_row),
                                  "Not supported by your device.");
      gtk_widget_set_sensitive(miniled_row, FALSE);
    }
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(disp_group), miniled_row);

    update_status_text(status_row, AsusModes::get_mode());

    GtkWidget *switcher_stack = gtk_stack_new();
    GtkWidget *ac_view = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *dc_view = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    gtk_stack_add_titled(GTK_STACK(switcher_stack), ac_view, "ac",
                         "Charger (AC)");
    gtk_stack_add_titled(GTK_STACK(switcher_stack), dc_view, "dc",
                         "Battery (DC)");
    gtk_stack_set_visible_child_name(GTK_STACK(switcher_stack),
                                     AsusBattery::is_on_ac() ? "ac" : "dc");
    gtk_widget_set_visible(switcher_stack, FALSE);

    GtkWidget *power_row = adw_action_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(power_row),
                                  "Power Source");
    adw_action_row_set_subtitle(ADW_ACTION_ROW(power_row),
                                AsusBattery::is_on_ac() ? "Charger (AC)"
                                                        : "Battery (DC)");

    GtkWidget *power_icon = gtk_image_new_from_icon_name(
        AsusBattery::is_on_ac() ? "battery-full-charging-symbolic"
                                : "battery-symbolic");
    adw_action_row_add_prefix(ADW_ACTION_ROW(power_row), power_icon);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), power_row);

    GtkWidget *buttons_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(buttons_box, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(buttons_box, 10);
    gtk_widget_set_margin_bottom(buttons_box, 15);

    auto create_mode_btn = [=](const char *label, const char *icon,
                               AsusMode mode, const char *css_class,
                               GtkWidget *st_stack) {
      GtkWidget *btn = gtk_button_new();
      GtkWidget *btn_content = adw_button_content_new();
      adw_button_content_set_label(ADW_BUTTON_CONTENT(btn_content), label);
      adw_button_content_set_icon_name(ADW_BUTTON_CONTENT(btn_content), icon);
      gtk_button_set_child(GTK_BUTTON(btn), btn_content);
      gtk_widget_add_css_class(btn, "suggested-action");
      gtk_widget_add_css_class(btn, css_class);

      struct CallbackData {
        AsusMode mode;
        GtkWidget *row;
        GtkWidget *stack;
        decltype(update_status_text) update_fn;
      };
      CallbackData *data =
          new CallbackData{mode, status_row, st_stack, update_status_text};

      auto cb = +[](GtkButton *btn, gpointer user_data) {
        CallbackData *d = static_cast<CallbackData *>(user_data);
        const char *visible =
            gtk_stack_get_visible_child_name(GTK_STACK(d->stack));
        bool is_ac_view = (std::string(visible) == "ac");

        auto handle = AsusNotification::show_loading(
            GTK_WIDGET(btn),
            is_ac_view ? "Setting Charger Mode..." : "Setting Battery Mode...");

        struct AsyncData {
          AsusNotification::LoadingHandle *h;
          CallbackData *d;
          bool is_ac;
        };
        AsyncData *ad = new AsyncData{handle, d, is_ac_view};

        g_timeout_add(
            100,
            +[](gpointer ptr) -> gboolean {
              AsyncData *ad = (AsyncData *)ptr;

              bool success = AsusModes::set_mode_for(ad->d->mode, ad->is_ac);

              ad->h->close();

              if (success) {
                AsusNotification::show_toast(ad->is_ac
                                                 ? "Charger Mode Updated"
                                                 : "Battery Mode Updated");
                ad->d->update_fn(ad->d->row, ad->d->mode);
              } else {
                AsusNotification::show_toast("Failed to set mode");
              }
              delete ad;
              return G_SOURCE_REMOVE;
            },
            ad);
      };

      g_signal_connect_data(
          btn, "clicked", (GCallback)cb, data,
          [](gpointer data, GClosure *) {
            delete static_cast<CallbackData *>(data);
          },
          (GConnectFlags)0);

      return btn;
    };

    gtk_box_append(GTK_BOX(buttons_box),
                   create_mode_btn("Silent", "emblem-system-symbolic",
                                   AsusMode::Silent, "silent-mode",
                                   switcher_stack));
    gtk_box_append(GTK_BOX(buttons_box),
                   create_mode_btn("Balanced", "emblem-system-symbolic",
                                   AsusMode::Balanced, "balanced-mode",
                                   switcher_stack));
    gtk_box_append(GTK_BOX(buttons_box),
                   create_mode_btn("Turbo", "emblem-system-symbolic",
                                   AsusMode::Turbo, "turbo-mode",
                                   switcher_stack));

    adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), buttons_box);

    // Tools & About
    GtkWidget *tools_group = adw_preferences_group_new();
    adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(tools_group),
                                    "Tools");
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(page),
                             ADW_PREFERENCES_GROUP(tools_group));

    GtkWidget *download_row = adw_action_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(download_row),
                                  "AAC CLI Tool");
    adw_action_row_set_subtitle(
        ADW_ACTION_ROW(download_row),
        "Build and install the standalone tool to /usr/local/bin/AAC.");

    GtkWidget *download_btn = gtk_button_new_with_label("Install AAC Tool");
    gtk_widget_set_valign(download_btn, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class(download_btn, "suggested-action");

    g_signal_connect(
        download_btn, "clicked", G_CALLBACK(+[](GtkButton *btn, gpointer) {
          bool installed = std::filesystem::exists("/usr/local/bin/AAC");

          if (installed) {
            auto handle = AsusNotification::show_loading(
                GTK_WIDGET(btn), "Uninstalling AAC Tool...");

            std::thread([handle]() {
              const char *cmd = "pkexec rm -f /usr/local/bin/AAC";
              int ret = system(cmd);

              g_idle_add(
                  +[](gpointer data) -> gboolean {
                    auto h = (AsusNotification::LoadingHandle *)data;
                    h->close();
                    return G_SOURCE_REMOVE;
                  },
                  handle);

              if (ret == 0) {
                g_idle_add(
                    +[](gpointer) -> gboolean {
                      AsusNotification::show_toast("AAC Tool Uninstalled.");
                      return G_SOURCE_REMOVE;
                    },
                    NULL);
              } else {
                g_idle_add(
                    +[](gpointer) -> gboolean {
                      AsusNotification::show_toast("Uninstallation failed.");
                      return G_SOURCE_REMOVE;
                    },
                    NULL);
              }
            }).detach();
          } else {
            auto handle = AsusNotification::show_loading(
                GTK_WIDGET(btn), "Building & Installing AAC Tool...");

            std::thread([handle]() {
              std::string base = AAC_SOURCE_DIR;
              std::string cmd =
                  "pkexec sh -c \"cd " + base + " && g++ -O3 -std=c++23 aac.cpp "
                  "backend/config.cpp backend/sysfs_writer.cpp "
                  "backend/modes.cpp backend/battery.cpp "
                  "backend/gpu.cpp backend/gpu_mux.cpp "
                  "backend/keyboard.cpp -I. "
                  "$(pkg-config --cflags --libs glib-2.0) -o "
                  "/usr/local/bin/AAC\"";

              int ret = system(cmd.c_str());

              g_idle_add(
                  +[](gpointer data) -> gboolean {
                    auto h = (AsusNotification::LoadingHandle *)data;
                    h->close();
                    return G_SOURCE_REMOVE;
                  },
                  handle);

              if (ret == 0) {
                g_idle_add(
                    +[](gpointer) -> gboolean {
                      AsusNotification::show_toast(
                          "AAC Tool installed to /usr/local/bin/AAC");
                      return G_SOURCE_REMOVE;
                    },
                    NULL);
              } else {
                g_idle_add(
                    +[](gpointer) -> gboolean {
                      AsusNotification::show_toast("Installation failed.");
                      return G_SOURCE_REMOVE;
                    },
                    NULL);
              }
            }).detach();
          }
        }),
        NULL);

    adw_action_row_add_suffix(ADW_ACTION_ROW(download_row), download_btn);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(tools_group), download_row);

    fdata->switcher_stack = switcher_stack;
    fdata->power_row = power_row;
    fdata->install_btn = download_btn;
    fdata->install_row = download_row;

    return page;
  }
};

extern "C" KdPlugin *create_plugin() { return new AsusArmouryPlugin(); }
