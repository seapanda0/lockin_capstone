#ifndef EEZ_LVGL_UI_VARS_H
#define EEZ_LVGL_UI_VARS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// enum declarations

// Flow global variables

enum FlowGlobalVariables {
    FLOW_GLOBAL_VARIABLE_TIMER_ARC_VALUE = 0,
    FLOW_GLOBAL_VARIABLE_DISPLAY_TIM_STR = 1,
    FLOW_GLOBAL_VARIABLE_START_END_STR = 2
};

// Native global variables

extern int32_t get_var_timer_arc_value();
extern void set_var_timer_arc_value(int32_t value);
extern const char *get_var_display_tim_str();
extern void set_var_display_tim_str(const char *value);
extern const char *get_var_start_end_str();
extern void set_var_start_end_str(const char *value);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_VARS_H*/