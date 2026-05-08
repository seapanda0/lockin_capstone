#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "ui.h"

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

// Getter/Setter for timer_arc_value (called by generated UI)
int32_t get_var_timer_arc_value() {
    return timer_arc_value;
}

void set_var_timer_arc_value(int32_t value) {
    timer_arc_value = value;
}

// ============= Arc Update Helper =============
static void update_arc_display() {
    if (pomodoro.duration_sec == 0) {
        set_var_timer_arc_value(0);
        return;
    }
    // Calculate arc value: 0-100 based on remaining time
    int32_t arc_val = (pomodoro.remaining_sec * 100) / pomodoro.duration_sec;
    if (arc_val < 0) arc_val = 0;
    if (arc_val > 100) arc_val = 100;
    set_var_timer_arc_value(arc_val);
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

// ============= EEZ Studio Action Handlers =============

/**
 * Action handler for "10 second" button click
 * Called by EEZ Studio-generated UI when the 10s button is pressed
 */
void action_button_10s_pressed(lv_event_t * e) {
    (void)e;  // unused
    
    ESP_LOGI(TAG, "10 second button pressed");
    
    // Start a 10-second timer
    start_timer(10);
}

// Note: Implement other button handlers (15 min, 25 min, etc.) similarly when needed
// void action_button_15m_pressed(lv_event_t * e) { start_timer(15 * 60); }
// void action_button_25m_pressed(lv_event_t * e) { start_timer(25 * 60); }
