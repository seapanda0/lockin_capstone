#ifndef APP_LOGIC_H
#define APP_LOGIC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============= Public API =============

/**
 * Start a pomodoro timer with the given duration (in seconds)
 */
void start_timer(uint32_t duration_seconds);

/**
 * Stop the running timer
 */
void stop_timer();

/**
 * Check if timer is currently running
 */
bool is_timer_running();

/**
 * Get the remaining time in seconds
 */
uint32_t get_remaining_time();

/**
 * Initialize app logic (call on startup)
 */
void app_logic_init();

// ============= UI Variable Getters/Setters =============

/**
 * Get the pomodoro period display string (e.g., "25 min")
 */
const char *get_var_pomo_tim_period_str();

/**
 * Set the pomodoro period display string
 */
void set_var_pomo_tim_period_str(const char *value);

// ============= EEZ Studio Action Handlers =============
// These are called by the generated UI

/**
 * Increment pomodoro period by 1 minute
 */
void action_button_plus_pressed(lv_event_t * e);

/**
 * Decrement pomodoro period by 1 minute
 */
void action_button_minus_pressed(lv_event_t * e);

/**
 * Start the pomodoro timer with selected period
 */
void action_button_start_pomo_pressed(lv_event_t * e);

#ifdef __cplusplus
}
#endif

#endif // APP_LOGIC_H
