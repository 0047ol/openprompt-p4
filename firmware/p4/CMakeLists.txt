cmake_minimum_required(VERSION 3.16)

set(PROJECT_VER "20261122.8")
set(COMPONENTS main)
include(cmake/exfat.cmake)
include(cmake/spi-safety.cmake)
if(TP_SD_BOUNDARY_TEST)
    set(PROJECT_VER "20261122.8-sdtest")
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(teleprompter_p4)

idf_component_get_property(tp_lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_options(${tp_lvgl_lib} PRIVATE -O2)

# The application initializes Hosted from its network task after USB/display.
# Hosted 2.x otherwise invokes it unconditionally from a C constructor.
idf_component_get_property(tp_hosted_lib espressif__esp_hosted COMPONENT_LIB)
get_target_property(tp_hosted_sources ${tp_hosted_lib} SOURCES)
list(FILTER tp_hosted_sources EXCLUDE REGEX "port_esp_hosted_host_init\\.c$")
set_property(TARGET ${tp_hosted_lib} PROPERTY SOURCES ${tp_hosted_sources})
