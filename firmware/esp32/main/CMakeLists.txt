file(GLOB SRC_FILES "${CMAKE_CURRENT_LIST_DIR}/../src/*.c")

idf_component_register(
    SRCS "main.c" ${SRC_FILES}
    INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/../include"
    REQUIRES
)
