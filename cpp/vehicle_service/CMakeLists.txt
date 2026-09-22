add_library(vehicle_service STATIC src/vehicle_service.cpp src/vehicle_data_format.cpp)
target_include_directories(vehicle_service PUBLIC include)

if(BUILD_TESTING)
    add_executable(vehicle_service_test ../../tests/cpp/vehicle_service_test.cpp)
    target_link_libraries(vehicle_service_test PRIVATE vehicle_service)
    add_test(NAME vehicle_service_test COMMAND vehicle_service_test)
endif()
