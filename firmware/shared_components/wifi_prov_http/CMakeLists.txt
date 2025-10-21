idf_build_get_property(target IDF_TARGET)

file (GLOB WIFI_HTTP_SRC_FILES 
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"
)

idf_component_register(
    SRCS ${WIFI_HTTP_SRC_FILES}
    INCLUDE_DIRS 
        "${CMAKE_CURRENT_LIST_DIR}/include"
    REQUIRES wifi_nvs esp_http_server nvs_flash
)

