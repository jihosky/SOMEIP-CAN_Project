add_library(vehicle_someip_provider STATIC src/vehicle_data_provider.cpp)
target_include_directories(vehicle_someip_provider PUBLIC include)
target_link_libraries(vehicle_someip_provider PUBLIC vehicle_service vehicle_someip_common vsomeip3)

add_executable(vehicle_data_provider src/provider_main.cpp)
target_link_libraries(vehicle_data_provider PRIVATE vehicle_someip_provider)

if(BUILD_TESTING)
    add_executable(event_provider_fixture ../../../tests/cpp/event_provider_fixture.cpp)
    target_link_libraries(event_provider_fixture PRIVATE vehicle_someip_provider)
endif()
