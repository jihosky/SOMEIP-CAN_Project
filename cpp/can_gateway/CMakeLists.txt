add_library(can_frame_format STATIC src/frame_format.cpp)
target_include_directories(can_frame_format PUBLIC include)

add_executable(socketcan_receiver src/socketcan_receiver.cpp)
target_link_libraries(socketcan_receiver PRIVATE can_frame_format)

if(BUILD_TESTING)
    add_executable(can_frame_format_test ../../tests/cpp/frame_format_test.cpp)
    target_link_libraries(can_frame_format_test PRIVATE can_frame_format)
    add_test(NAME can_frame_format_test COMMAND can_frame_format_test)
endif()
