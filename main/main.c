#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"

#include "pinout.h"
#include "st7789.h"
#include "lvgl.h"
#include "ui.h"

static const char *TAG = "ST7789";

#define DISPLAY_TASK_STACK_SIZE  (10 * 1024)
#define DISPLAY_TASK_PRIORITY    2

static void display_task(void *arg);

#define CONFIG_WIDTH  240
#define CONFIG_HEIGHT 320
#define CONFIG_OFFSETX 0
#define CONFIG_OFFSETY 0

// --- I2C Touch Driver (CST816S / FT6236) ---
#define I2C_MASTER_NUM              0               /*!< I2C master i2c port number */
#define I2C_MASTER_FREQ_HZ          400000          /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0               /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0               /*!< I2C master doesn't need buffer */

#define FT6236_ADDR                 0x38            /*!< Slave address of FT6236 */

// I2C Bus 0
static i2c_master_bus_handle_t i2c_bus_touch_handle;
static i2c_master_dev_handle_t ft6236_handle;
static SemaphoreHandle_t touch_interrupt_sem = NULL;
static volatile bool touch_data_ready = false;
static bool g_touch_ready = false;

// I2C Bus for touch panel, I2C number 0
static esp_err_t i2c_master_init(void) {
    if (i2c_bus_touch_handle != NULL && ft6236_handle != NULL) {
        return ESP_OK;
    }

    i2c_master_bus_config_t touch_bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = TP_SDA,
        .scl_io_num = TP_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus(&touch_bus_config, &i2c_bus_touch_handle);
    if (err != ESP_OK) {
        return err;
    }

    i2c_device_config_t ft6236_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = FT6236_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    err = i2c_master_bus_add_device(i2c_bus_touch_handle, &ft6236_config, &ft6236_handle);
    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}

static bool tp_read_point(uint16_t *x, uint16_t *y) {
    if (!g_touch_ready || ft6236_handle == NULL) {
        return false;
    }

    uint8_t data[6];
    uint8_t start_reg = 0x02;
    
    // Read registers starting from 0x02 (Status, touch ID, X, Y)
    esp_err_t ret = i2c_master_transmit_receive(ft6236_handle, &start_reg, 1, data, sizeof(data), 10);

    if (ret != ESP_OK || data[0] == 0) { // If no touch points
        touch_data_ready = false;
        return false;
    }

    // Extract X and Y
    *x = ((data[1] & 0x0F) << 8) | data[2];
    *y = ((data[3] & 0x0F) << 8) | data[4];
    ESP_LOGI(TAG, "X: %u, Y: %u", *x, *y);
    touch_data_ready = false;
    
    return true;
}

// ISR handler for touch interrupt pin (TP_INT)
static void IRAM_ATTR tp_interrupt_handler(void *arg)
{
    (void)arg;
    touch_data_ready = true;
    if (touch_interrupt_sem != NULL) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(touch_interrupt_sem, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    }
}

static void tp_init(void) {
    g_touch_ready = false;

    if (touch_interrupt_sem == NULL) {
        touch_interrupt_sem = xSemaphoreCreateBinary();
        if (touch_interrupt_sem == NULL) {
            ESP_LOGE(TAG, "Failed to create touch interrupt semaphore");
        }
    }

    gpio_install_isr_service(0);

    gpio_reset_pin(TP_SCL);
    gpio_reset_pin(TP_SDA);
    gpio_reset_pin(TP_INT);
    gpio_reset_pin(TP_RST);
    
    // Hardware Reset Pin
    gpio_config_t out_conf = {
        .pin_bit_mask = (1ULL << TP_RST),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&out_conf);
    
    // Interrupt Pin - configured for falling edge (active low)
    gpio_config_t in_conf = {
        .pin_bit_mask = (1ULL << TP_INT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    gpio_config(&in_conf);

    // Reset Sequence
    gpio_set_level(TP_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(TP_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    // Init I2C bus
    if (i2c_master_init() == ESP_OK) {
        ESP_LOGI(TAG, "I2C Touch Panel Initialized.");

        esp_err_t ret = i2c_master_probe(i2c_bus_touch_handle, FT6236_ADDR, 10);
        if (ret == ESP_OK) {
            g_touch_ready = true;
            ESP_LOGI(TAG, "Touch panel detected at address 0x%02x", FT6236_ADDR);
            
            gpio_isr_handler_add(TP_INT, tp_interrupt_handler, NULL);
            gpio_intr_enable(TP_INT);
            ESP_LOGI(TAG, "Touch interrupt enabled on GPIO %d", TP_INT);
        } else {
            ESP_LOGW(TAG, "Touch panel not detected at address 0x%02x (err=%s)", FT6236_ADDR, esp_err_to_name(ret));
        }
    } else {
        ESP_LOGE(TAG, "I2C Init Failed.");
    }
}

static void my_flush_cb(lv_display_t * display, const lv_area_t * area, uint8_t * px_map) {
    TFT_t *dev = (TFT_t *)lv_display_get_user_data(display);
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    uint32_t size = w * h;

    uint16_t _x1 = area->x1 + dev->_offsetx;
    uint16_t _x2 = area->x2 + dev->_offsetx;
    uint16_t _y1 = area->y1 + dev->_offsety;
    uint16_t _y2 = area->y2 + dev->_offsety;

    // Send high-speed hardware window bounds
    spi_master_write_command(dev, 0x2A);
    spi_master_write_addr(dev, _x1, _x2);
    spi_master_write_command(dev, 0x2B);
    spi_master_write_addr(dev, _y1, _y2);
    spi_master_write_command(dev, 0x2C);

    // LVGL stores RGB565 in native order; the LCD expects big-endian bytes.
    lv_draw_sw_rgb565_swap(px_map, size);

    // Blast the swapped LVGL color buffer over SPI
    gpio_set_level(dev->_dc, 1);
    spi_master_write_byte(dev->_SPIHandle, px_map, size * 2);

    lv_display_flush_ready(display);
}

static void my_touchpad_read(lv_indev_t * indev, lv_indev_data_t * data) {
    (void)indev;

    if (!g_touch_ready) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }

    if (!touch_data_ready) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }

    uint16_t x_raw, y_raw;
    bool touched = tp_read_point(&x_raw, &y_raw);
    if(touched) {
        data->state = LV_INDEV_STATE_PR;
        
        // Proper Matrix: X correlates to the physical long-edge (y_raw)
        // Y correlates to the physical short-edge (x_raw)
        data->point.x = 320 - y_raw;
        data->point.y = x_raw;
        
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

static TFT_t s_dev;

static void display_task(void *arg)
{
    (void)arg;

    // Give the serial monitor time to connect over USB before printing important logs
    vTaskDelay(pdMS_TO_TICKS(3000));

    // Initialize Touch Controller
    tp_init();

    // Change SPI Clock Frequency
    spi_clock_speed(40000000); // 40MHz

    // Initialize SPI Hardware Pins
    spi_master_init(&s_dev, LCD_SDA, LCD_SCK, LCD_CS, LCD_WR, LCD_RESET, LCD_BL);

    // Initialize the display using horizontal dimensions (320x240)
    lcdInit(&s_dev, 320, 240, CONFIG_OFFSETX, CONFIG_OFFSETY);

    // Force Hardware Landscape Rotation (MADCTL) with RGB mapping
    spi_master_write_command(&s_dev, 0x36);
    spi_master_write_data_byte(&s_dev, 0xA0);

    // Force Color Inversion OFF
    lcdInversionOff(&s_dev);

    lv_init();

    // Create display
    lv_display_t *display = lv_display_create(320, 240);
    lv_display_set_default(display);
    lv_display_set_user_data(display, &s_dev);

    // Allocate and set draw buffers
    static lv_color_t *buf1 = NULL;
    if (!buf1) {
        buf1 = heap_caps_malloc(320 * 40 * sizeof(lv_color_t), MALLOC_CAP_DMA);
        assert(buf1 != NULL);
    }
    lv_display_set_buffers(display, buf1, NULL, 320 * 40 * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);

    // Set color format and flush callback
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(display, my_flush_cb);

    // Create and register touch input device
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(indev, display);
    lv_indev_set_read_cb(indev, my_touchpad_read);

    // Start the EEZ Studio UI
    ui_init();

    // Main LVGL loop
    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
        lv_tick_inc(10);
    }
}


void app_main(void)
{
    xTaskCreate(display_task, "display_task", DISPLAY_TASK_STACK_SIZE, NULL, DISPLAY_TASK_PRIORITY, NULL);
    vTaskDelete(NULL);
}
