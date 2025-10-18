idf_build_get_property(target IDF_TARGET)

file (GLOB WIFI_NVS_SRC_FILES "${CMAKE_CURRENT_LIST_DIR}/src/*.c")

idf_component_register(
    SRCS ${WIFI_SRC_FILES}
    INCLUDE_DIRS 
        "${CMAKE_CURRENT_LIST_DIR}/include"
    REQUIRES nvs_flash 
)

