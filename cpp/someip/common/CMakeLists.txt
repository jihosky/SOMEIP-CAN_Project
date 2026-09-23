add_library(vehicle_someip_common STATIC src/payload_codec.cpp)
target_include_directories(vehicle_someip_common PUBLIC include)
target_link_libraries(vehicle_someip_common PUBLIC vehicle_service)

if(BUILD_TESTING)
    add_executable(someip_payload_test ../../../tests/cpp/someip_payload_test.cpp)
    target_link_libraries(someip_payload_test PRIVATE vehicle_someip_common)
    add_test(NAME someip_payload_test COMMAND someip_payload_test)
endif()
