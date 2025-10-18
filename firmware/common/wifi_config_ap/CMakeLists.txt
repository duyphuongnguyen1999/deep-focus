idf_build_get_property(target IDF_TARGET)

file (GLOB WIFI_CONFIG_AP_SRC_FILES "${CMAKE_CURRENT_LIST_DIR}/src/*.c")

idf_component_register(
    SRCS ${WIFI_CONFIG_AP_SRC_FILES}
    INCLUDE_DIRS 
        "${CMAKE_CURRENT_LIST_DIR}/include"
    PRIV_REQUIRES esp_wifi esp_netif nvs_flash wifi_connect
)

