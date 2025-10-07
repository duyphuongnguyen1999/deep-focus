idf_build_get_property(target IDF_TARGET)

file (GLOB SRC_FILES "src/*.c")

idf_component_register(SRCS ${SRC_FILES}
                       INCLUDE_DIRS include
                       REQUIRES esp_wifi esp_netif nvs_flash)

