#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C"
{
#endif

    lv_display_t *oled_display_init(int i2c_port, int sda_io, int scl_io,
                                    uint8_t i2c_addr, int width, int height);

    void oled_display_create_basic_ui(lv_display_t *disp);

    void oled_display_update(float temp_c, float hum_pct);

#ifdef __cplusplus
}
#endif

#endif // OLED_DISPLAY_H
