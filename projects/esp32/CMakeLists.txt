#
#   CMakeLists.txt - CMake file for the Ioto component on ESP32
#
cmake_minimum_required(VERSION 3.5)

file(GLOB lib
    "src/*.c"
    "src/cloud/*.c"
    "src/cmds/ioto.c"
    "lib/cryptLib.c"
    "lib/dbLib.c"
    "lib/jsonLib.c"
    "lib/mqttLib.c"
    "lib/openaiLib.c"
    "lib/rLib.c"
    "lib/uctxAssembly.S"
    "lib/uctxLib.c"
    "lib/urlLib.c"
    "lib/webLib.c"
    "lib/websockLib.c"
)

idf_component_register(SRCS "${lib}"
                       INCLUDE_DIRS "include"
                       REQUIRES pthread mbedtls joltwallet__littlefs esp_event esp_psram esp_wifi nvs_flash)

add_custom_target(ioto_custom_script
    COMMAND make config-esp32
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/components/ioto"
    COMMENT "Running make config-esp32"
)
add_dependencies(${COMPONENT_LIB} ioto_custom_script)

# Read APP from .app file, fallback to environment, then default to "blink"
if(EXISTS ".app")
    file(READ ".app" APP_NAME)
    string(STRIP "${APP_NAME}" APP_NAME)
elseif(DEFINED ENV{APP})
    set(APP_NAME $ENV{APP})
else()
    set(APP_NAME "blink")
endif()

target_compile_options(${COMPONENT_LIB} PUBLIC "-DME_COM_MBEDTLS=1" "-DME_COM_OPENSSL=0" "-DAPP=${APP_NAME}" "-Wno-unused-variable" "-Wno-empty-body" "-Wtype-limits" "-Wno-error=char-subscripts")
target_include_directories(${COMPONENT_LIB} PUBLIC "$ENV{IDF_PATH}/components/freertos/FreeRTOS-Kernel/include/freertos")