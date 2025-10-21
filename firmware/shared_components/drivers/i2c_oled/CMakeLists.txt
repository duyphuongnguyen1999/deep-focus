idf_build_get_property(target IDF_TARGET)

file (GLOB OLED_SRC_FILES "${CMAKE_CURRENT_LIST_DIR}/src/*.c")

idf_component_register(SRCS ${OLED_SRC_FILES}
                       INCLUDE_DIRS include
                       REQUIRES esp_lcd esp_driver_i2c esp_timer lvgl)