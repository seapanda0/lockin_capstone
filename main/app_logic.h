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

// ============= EEZ Studio Action Handlers =============
// These are called by the generated UI

/**
 * Action handler for "10 second" button click
 */
void action_button_10s_pressed(lv_event_t * e);

#ifdef __cplusplus
}
#endif

#endif // APP_LOGIC_H
