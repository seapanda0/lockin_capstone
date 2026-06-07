#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Screens

enum ScreensEnum {
    _SCREEN_ID_FIRST = 1,
    SCREEN_ID_MAIN = 1,
    SCREEN_ID_CLOCK = 2,
    SCREEN_ID_ALARM = 3,
    SCREEN_ID_SETTINGS = 4,
    _SCREEN_ID_LAST = 4
};

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *clock;
    lv_obj_t *alarm;
    lv_obj_t *settings;
    lv_obj_t *button_10min;
    lv_obj_t *button_15min;
    lv_obj_t *button_25min;
    lv_obj_t *button_10s;
    lv_obj_t *obj0;
    lv_obj_t *time_text;
} objects_t;

extern objects_t objects;

void create_screen_main();
void tick_screen_main();

void create_screen_clock();
void tick_screen_clock();

void create_screen_alarm();
void tick_screen_alarm();

void create_screen_settings();
void tick_screen_settings();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/