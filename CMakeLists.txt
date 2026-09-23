cmake_minimum_required(VERSION 3.20)
project(SOMEIP_CAN_Project LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

find_package(vsomeip3 QUIET CONFIG)
if(NOT vsomeip3_FOUND OR NOT TARGET vsomeip3)
    message(FATAL_ERROR "vSomeIP 3 was not found. Install its development package or set vsomeip3_DIR to the directory containing vsomeip3Config.cmake.")
endif()

add_subdirectory(cpp/vehicle_service)
add_subdirectory(cpp/can_gateway)
add_subdirectory(cpp/someip/common)
add_subdirectory(cpp/someip/provider)
add_subdirectory(cpp/someip/client)
add_subdirectory(cpp/apps)
