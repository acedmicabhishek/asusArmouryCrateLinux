#pragma once
#include <functional>





namespace AsusKeybind {

    enum class Action {
        BacklightUp,
        BacklightDown,
        RgbCycle,
        PerfCycle,
        Count
    };

    const char* action_key(Action);    
    const char* action_label(Action);  

    int  get_binding(Action);          
    void set_binding(Action, int code);

    void perform(Action);              
    int  run_daemon();                 

    bool is_supported();               

    
    bool service_is_enabled();
    bool service_enable();
    bool service_disable();
    void service_reload();             

    
    void begin_capture(std::function<void(int)> on_captured);
    void cancel_capture();
}
