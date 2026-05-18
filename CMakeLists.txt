cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Trim build to only components your app actually uses.
idf_build_set_property(MINIMAL_BUILD ON)

# Make sdkconfig.defaults the source of truth
set(SDKCONFIG_DEFAULTS "sdkconfig.defaults")
project(Alarm_Clock_v3)
