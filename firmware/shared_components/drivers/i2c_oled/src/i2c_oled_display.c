#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/lock.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "i2c_oled_display.h"

// ====== Module Configuration ======
#define LVGL_TICK_MS 5
#define LVGL_TASK_STACK (4 * 1024)
#define LVGL_TASK_PRIO 2
#define LVGL_PALETTE_BYTES 8

static const char *TAG = "OLED";

// ====== Internal module resources ======
// LVGL is not thread-safe → need to lock around LVGL API calls
static _lock_t s_lvgl_lock;

// UI elements
static lv_obj_t *s_label = NULL;    // Label to display Temp/Hum
static lv_display_t *s_disp = NULL; // Display handle for updating UI

// OLED panel resources
// Buffer for 1-bit pixel data to send to panel (allocated after knowing width/height)
static uint8_t *s_oled_buffer = NULL;

// Width and height of the panel (for flush)
static int s_w = 0, s_h = 0;

// ====== Callback and task implementations ======
// ====== Callback when color transfer is done: call lv_display_flush_ready() ======
static bool on_color_trans_done(esp_lcd_panel_io_handle_t io,
                                esp_lcd_panel_io_event_data_t *edata,
                                void *user_ctx)
{
    lv_display_t *disp = (lv_display_t *)user_ctx;
    lv_display_flush_ready(disp); // Inform LVGL that flushing is done --> can continue now
    return false;                 // No need to yield from ISR
}

// ====== Flush function of LVGL: convert px_map to 1-bit buffer and push to panel ======
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    // Safety check
    if (!s_oled_buffer || !disp || !area || !px_map)
    {
        ESP_LOGE(TAG, "Flush cb: invalid parameters");
        return;
    }

    // Get panel handle stored in user_data of disp
    esp_lcd_panel_handle_t panel = lv_display_get_user_data(disp);

    // LVGL (I1) uses first 2x4 bytes of buffer as palette → skip it
    px_map += LVGL_PALETTE_BYTES;

    // Physical horizontal resolution of display
    uint16_t hor_res = lv_display_get_physical_horizontal_resolution(disp);

    // Area to flush
    const int x1 = area->x1, x2 = area->x2, y1 = area->y1, y2 = area->y2;

    // Iterate through each pixel in the area, convert from "row-major bit" of LVGL
    // to "column-major/8-pixel-chunk bit" required by SSD1306
    for (int y = y1; y <= y2; y++)
    {
        for (int x = x1; x <= x2; x++)
        {
            // Each byte in px_map contains 8 horizontal pixels (MSB is leftmost pixel)
            bool pixel_on = (px_map[(hor_res >> 3) * y + (x >> 3)] & (1 << (7 - (x & 7)))) != 0;

            // SSD1306: byte_offset = (width * (y / 8)) + x
            // Locate destination byte in buffer
            uint8_t *dst = s_oled_buffer + s_w * (y >> 3) + x;

            // Assume pixel_on = 1 means "black" (pixel lit)
            if (pixel_on)
            {
                (*dst) &= ~(1 << (y & 7)); // Set pixel (turn on)
            }
            else
            {
                (*dst) |= (1 << (y & 7)); // Clear pixel (turn off)
            }
        }
    }

    // Push the prepared buffer to the panel
    esp_lcd_panel_draw_bitmap(panel, x1, y1, x2 + 1, y2 + 1, s_oled_buffer);
}

// ====== Timer callback: increment LVGL tick ======
static void lvgl_tick_cb(void *arg)
{
    lv_tick_inc(LVGL_TICK_MS); // Tell LVGL that LVGL_TICK_MS ms have passed
}

// ====== LGVL task loop: handle timers, animations, events ======
static void lvgl_task(void *arg)
{
    ESP_LOGI(TAG, "LVGL loop start");
    while (1)
    {
        // LGVL is not thread-safe → need to lock
        _lock_acquire(&s_lvgl_lock);        // Lock before call API LVGL
        uint32_t wait = lv_timer_handler(); // Handle timers, animations, events, etc.
        _lock_release(&s_lvgl_lock);        // Unlock after done

        // Limited wait time to avoid WDT and for smoothness
        if (wait < 5)
            wait = 5;
        if (wait > 500)
            wait = 500;
        vTaskDelay(pdMS_TO_TICKS(wait));
    }
}

// ====== Public APIs ======

lv_display_t *oled_display_init(int i2c_port, int sda_io, int scl_io,
                                uint8_t i2c_addr, int width, int height)
{
    // Store width/height for flush use
    s_w = width;
    s_h = height;

    // 1. Configure I2C bus
    i2c_master_bus_handle_t i2c_bus = NULL;
    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT, // Use default clock source
        .glitch_ignore_cnt = 7,            // Ignore glitches < 7 cycles
        .i2c_port = i2c_port,              // I2C port number
        .sda_io_num = sda_io,              // GPIO SDA
        .scl_io_num = scl_io,              // GPIO SCL
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &i2c_bus));

    // 2. Configure panel IO for SSD1306
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_i2c_config_t io_cfg = {
        .dev_addr = i2c_addr,
        .scl_speed_hz = 400 * 1000, // 400kHz
        .control_phase_bytes = 1,   // According to datasheet SSD1306
        .lcd_cmd_bits = 8,          // 8-bit Command
        .lcd_param_bits = 8,        // 8-bit Parameter
        .dc_bit_offset = 6,         // According datasheet SSD1306
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &io_cfg, &io));

    // 3. Configure panel for SSD1306
    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_dev_config_t panel_cfg = {
        .bits_per_pixel = 1,  // 1 bit per pixel (monochrome)
        .reset_gpio_num = -1, // if not used
    };
    esp_lcd_panel_ssd1306_config_t vendor_cfg = {
        .height = height, // 64 or 32
    };
    panel_cfg.vendor_config = &vendor_cfg;

    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io, &panel_cfg, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));             // Reset panel
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));              // Init panel
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true)); // Turn on display

    // 4. Initialize LVGL
    lv_init(); // Initialize LVGL library

    // Allocate buffer for panel (1 bit/pixel) for flush (outside LVGL buffer)
    size_t oled_buf_sz = (size_t)width * (size_t)height / 8;
    s_oled_buffer = heap_caps_calloc(1, oled_buf_sz, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    assert(s_oled_buffer);

    // Create LVGL display and configure it
    lv_display_t *disp = lv_display_create(width, height);
    s_disp = disp;                         // Store for UI update use
    lv_display_set_user_data(disp, panel); // Store panel handle in user_data of disp

    // LVGL use 1-bit I1 format → need extra 8-byte palette in LVGL buffer
    size_t lv_buf_sz = width * height / 8 + LVGL_PALETTE_BYTES;
    void *lv_buf = heap_caps_calloc(1, lv_buf_sz, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    assert(lv_buf);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_I1); // Set color format to 1-bit I1
    lv_display_set_buffers(disp, lv_buf, NULL, lv_buf_sz, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(disp, lvgl_flush_cb); // Set flush function

    // Register callback for "transfer done" to call lv_display_flush_ready()
    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = on_color_trans_done,
    };
    esp_lcd_panel_io_register_event_callbacks(io, &cbs, disp);

    // 5. Create timer to increment LVGL tick
    const esp_timer_create_args_t tick_args = {
        .callback = lvgl_tick_cb,
        .name = "lvgl_tick"};
    esp_timer_handle_t tick_tmr = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick_tmr));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_tmr, LVGL_TICK_MS * 1000)); // us

    // 6. Create loop task to handle LVGL
    xTaskCreate(lvgl_task, "lvgl", LVGL_TASK_STACK, NULL, LVGL_TASK_PRIO, NULL);

    ESP_LOGI(TAG, "OLED/LVGL initialized");
    return disp; // Return display handle for creating UI
}

void oled_display_create_basic_ui(lv_display_t *disp)
{
    // Store display for UI update use
    _lock_acquire(&s_lvgl_lock);                              // Lock before call API LVGL
    lv_obj_t *scr = lv_display_get_screen_active(disp);       // Get active screen
    s_label = lv_label_create(scr);                           // Create label on screen
    lv_obj_align(s_label, LV_ALIGN_TOP_LEFT, 0, 0);           // Align to top-left
    lv_label_set_text(s_label, "Temp: --.- C\nHum : --.- %"); // Initial text
    _lock_release(&s_lvgl_lock);
}

void oled_display_update(float temp_c, float hum_pct)
{
    if (!s_disp || !s_label)
        return; // Not initialized yet → ignore

    char buf[48];
    snprintf(buf, sizeof(buf), "Temp: %.1f C\nHum : %.1f %%", temp_c, hum_pct);

    _lock_acquire(&s_lvgl_lock);     // Lock before call API LVGL
    lv_label_set_text(s_label, buf); // Update label text
    _lock_release(&s_lvgl_lock);
}
