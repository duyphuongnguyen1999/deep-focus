file(GLOB SRC_FILES "${CMAKE_CURRENT_LIST_DIR}/../src/*.c")

idf_component_register(
    SRCS "main.c" ${SRC_FILES}
    INCLUDE_DIRS 
        "${CMAKE_CURRENT_LIST_DIR}/../include"
        "${EXTRA_COMPONENT_DIRS}/wifi_connect/include"
    REQUIRES esp_lcd esp_driver_i2c esp_timer lvgl esp_netif
)
