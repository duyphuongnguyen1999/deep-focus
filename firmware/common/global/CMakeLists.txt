idf_build_get_property(target IDF_TARGET)

file (GLOB GLOBAL_SRC_FILES "${CMAKE_CURRENT_LIST_DIR}/src/*.c")

idf_component_register(SRCS ${GLOBAL_SRC_FILES}
                       INCLUDE_DIRS ${CMAKE_CURRENT_LIST_DIR}/include
                       REQUIRES )