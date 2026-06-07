#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "APP_LOGIC";

// ============= Timer State =============
typedef struct {
    uint32_t duration_sec;       // Total duration in seconds
    uint32_t remaining_sec;      // Seconds remaining
    bool running;                // Is timer currently running?
    TimerHandle_t timer_handle;  // FreeRTOS timer handle
} pomodoro_state_t;

static pomodoro_state_t pomodoro = {
    .duration_sec = 0,
    .remaining_sec = 0,
    .running = false,
    .timer_handle = NULL
};

// Forward declarations
static void timer_callback(TimerHandle_t xTimer);
void stop_timer();

// ============= Variable Storage =============
static int32_t timer_arc_value = 0;
static char display_tim_str[16] = "25:00";
static char start_end_str[16] = "Start";

// Pomodoro period (duration to set), in seconds
static uint32_t pomo_tim_period_sec = 25 * 60;  // default 25 minutes

// Getter/Setter for timer_arc_value (called by generated UI)
int32_t get_var_timer_arc_value() {
    return timer_arc_value;
}

void set_var_timer_arc_value(int32_t value) {
    timer_arc_value = value;
}

const char *get_var_display_tim_str() {
    return display_tim_str;
}

void set_var_display_tim_str(const char *value) {
    if (value == NULL) {
        display_tim_str[0] = '\0';
        return;
    }
    strncpy(display_tim_str, value, sizeof(display_tim_str) - 1);
    display_tim_str[sizeof(display_tim_str) - 1] = '\0';
}

const char *get_var_start_end_str() {
    return start_end_str;
}

void set_var_start_end_str(const char *value) {
    if (value == NULL) {
        start_end_str[0] = '\0';
        return;
    }
    strncpy(start_end_str, value, sizeof(start_end_str) - 1);
    start_end_str[sizeof(start_end_str) - 1] = '\0';
}

static void update_pomo_period_display() {
    uint32_t mins = pomo_tim_period_sec / 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02u:00", (unsigned)mins);
    set_var_display_tim_str(buf);
}

// ============= Arc Update Helper =============
static void update_arc_display() {
    if (pomodoro.duration_sec == 0) {
        set_var_timer_arc_value(0);
        set_var_display_tim_str("00:00");
        return;
    }
    // Calculate arc value: 0-100 based on remaining time
    int32_t arc_val = (pomodoro.remaining_sec * 100) / pomodoro.duration_sec;
    if (arc_val < 0) arc_val = 0;
    if (arc_val > 100) arc_val = 100;
    set_var_timer_arc_value(arc_val);
    
    // Format remaining time as MM:SS and update display text
    uint32_t mins = pomodoro.remaining_sec / 60;
    uint32_t secs = pomodoro.remaining_sec % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)mins, (unsigned)secs);
    set_var_display_tim_str(buf);
}

// ============= FreeRTOS Timer Callback =============
static void timer_callback(TimerHandle_t xTimer) {
    (void)xTimer;
    
    if (!pomodoro.running) return;
    
    if (pomodoro.remaining_sec > 0) {
        pomodoro.remaining_sec--;
        update_arc_display();
        ESP_LOGI(TAG, "Timer: %u seconds remaining", pomodoro.remaining_sec);
    } else {
        // Timer finished
        pomodoro.running = false;
        xTimerStop(pomodoro.timer_handle, 0);
        ESP_LOGI(TAG, "Timer finished!");
        set_var_timer_arc_value(0);
        set_var_start_end_str("Start");
        update_pomo_period_display();
    }
}

// ============= Public API =============

/**
 * Start a pomodoro timer with the given duration (in seconds)
 * For testing: 10s button = 10 real seconds
 */
void start_timer(uint32_t duration_seconds) {
    if (pomodoro.running) {
        ESP_LOGW(TAG, "Timer already running, stopping it first");
        stop_timer();
    }
    
    pomodoro.duration_sec = duration_seconds;
    pomodoro.remaining_sec = duration_seconds;
    pomodoro.running = true;
    
    // Create a FreeRTOS software timer if not already created
    if (pomodoro.timer_handle == NULL) {
        pomodoro.timer_handle = xTimerCreate(
            "pomodoro_timer",
            pdMS_TO_TICKS(1000),  // 1 second callback interval
            pdTRUE,               // auto-reload
            NULL,                 // timer ID
            timer_callback        // callback function
        );
    }
    
    // Start the timer
    if (pomodoro.timer_handle != NULL) {
        xTimerStart(pomodoro.timer_handle, 0);
        update_arc_display();
        set_var_start_end_str("Stop");
        ESP_LOGI(TAG, "Timer started: %u seconds", duration_seconds);
    } else {
        ESP_LOGE(TAG, "Failed to create FreeRTOS timer");
        pomodoro.running = false;
    }
}

/**
 * Stop the running timer
 */
void stop_timer() {
    if (pomodoro.timer_handle != NULL) {
        xTimerStop(pomodoro.timer_handle, 0);
    }
    pomodoro.running = false;
    pomodoro.remaining_sec = 0;
    pomodoro.duration_sec = 0;
    set_var_timer_arc_value(0);
    set_var_start_end_str("Start");
    update_pomo_period_display();
    ESP_LOGI(TAG, "Timer stopped");
}

/**
 * Get current timer status
 */
bool is_timer_running() {
    return pomodoro.running;
}

uint32_t get_remaining_time() {
    return pomodoro.remaining_sec;
}

void app_logic_init() {
    // Initialize the period display on startup
    update_pomo_period_display();
}

// ============= EEZ Studio Action Handlers =============

/**
 * Increment the pomo period by 5 minutes (min 5 min, max 60 min)
 */
void action_button_plus_pressed(lv_event_t * e) {
    (void)e;  // unused
    
    if (pomodoro.running) {
        ESP_LOGW(TAG, "Cannot change period while timer is running");
        return;
    }
    
    if (pomo_tim_period_sec + 5 * 60 <= 60 * 60) {  // max 60 minutes
        pomo_tim_period_sec += 5 * 60;  // increment by 5 minutes
        update_pomo_period_display();
        ESP_LOGI(TAG, "Pomo period increased to %u minutes", pomo_tim_period_sec / 60);
    }
}

/**
 * Decrement the pomo period by 5 minutes (min 5 min, max 60 min)
 */
void action_button_minus_pressed(lv_event_t * e) {
    (void)e;  // unused
    
    if (pomodoro.running) {
        ESP_LOGW(TAG, "Cannot change period while timer is running");
        return;
    }
    
    if (pomo_tim_period_sec > 5 * 60) {  // min 5 minutes
        pomo_tim_period_sec -= 5 * 60;  // decrement by 5 minutes
        update_pomo_period_display();
        ESP_LOGI(TAG, "Pomo period decreased to %u minutes", pomo_tim_period_sec / 60);
    }
}

/**
 * Start/stop the pomodoro timer with the selected period
 */
void action_button_start_pomo_pressed(lv_event_t * e) {
    (void)e;  // unused
    
    if (pomodoro.running) {
        ESP_LOGI(TAG, "Stop pomodoro button pressed");
        stop_timer();
    } else {
        ESP_LOGI(TAG, "Start pomodoro button pressed; duration=%u seconds", pomo_tim_period_sec);
        start_timer(pomo_tim_period_sec);
    }
}
