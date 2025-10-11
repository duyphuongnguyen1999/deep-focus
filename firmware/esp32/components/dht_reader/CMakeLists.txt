idf_build_get_property(target IDF_TARGET)

file (GLOB DHT_SRC_FILES "${CMAKE_CURRENT_LIST_DIR}/src/*.c")

idf_component_register(
    SRCS ${DHT_SRC_FILES}
    INCLUDE_DIRS 
        include
    REQUIRES esp_driver_gpio dht global)