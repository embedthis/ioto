#
#   CMakeLists.txt - CMake file for the Ioto component on ESP32
#
cmake_minimum_required(VERSION 3.5)

set(lib
    "lib/iotoLib.c"
    "lib/cryptLib.c"
    "lib/dbLib.c"
    "lib/uctxLib.c"
    "lib/uctxAssembly.S"
    "lib/jsonLib.c"
    "lib/mqttLib.c"
    "lib/rLib.c"
    "lib/urlLib.c"
    "lib/webLib.c"
    )

idf_component_register(SRCS "${lib}"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES pthread mbedtls joltwallet__littlefs esp_event esp_psram esp_wifi nvs_flash)

add_custom_target(ioto_custom_script
    COMMAND make config-esp32
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/components/ioto"
    COMMENT "Running make config-esp32"
)
add_dependencies(${COMPONENT_LIB} ioto_custom_script)

target_compile_options(${COMPONENT_LIB} PUBLIC "-DUCTX_NAME=\"ioto\"" "-Wno-unused-variable" "-Wno-empty-body" "-Wtype-limits")