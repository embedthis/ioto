file(GLOB SOURCES *.c)

idf_component_register(SRCS "${SOURCES}"
    INCLUDE_DIRS "."
)

if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    littlefs_create_partition_image(storage ../state FLASH_IN_PROJECT)
else()
    fail_at_build_time(littlefs "Windows does not support LittleFS partition generation")
endif()