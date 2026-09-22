set(MILESTONE1_CONFIG ${PROJECT_SOURCE_DIR}/config/can/milestone1.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${MILESTONE1_CONFIG})
file(READ ${MILESTONE1_CONFIG} MILESTONE1_JSON)
string(JSON M1_CAN_ID GET "${MILESTONE1_JSON}" can_id)
string(JSON M1_DLC GET "${MILESTONE1_JSON}" dlc)
string(JSON M1_BYTE_ORDER GET "${MILESTONE1_JSON}" byte_order)
if(NOT M1_BYTE_ORDER STREQUAL "little")
    message(FATAL_ERROR "Milestone 1 decoder requires little-endian signal bytes")
endif()
string(JSON M1_SPEED_START GET "${MILESTONE1_JSON}" vehicle_speed start)
string(JSON M1_SPEED_LENGTH GET "${MILESTONE1_JSON}" vehicle_speed length)
string(JSON M1_SPEED_SCALE GET "${MILESTONE1_JSON}" vehicle_speed scale)
string(JSON M1_RPM_START GET "${MILESTONE1_JSON}" engine_rpm start)
string(JSON M1_RPM_LENGTH GET "${MILESTONE1_JSON}" engine_rpm length)
string(JSON M1_TEMP_START GET "${MILESTONE1_JSON}" coolant_temperature start)
string(JSON M1_TEMP_LENGTH GET "${MILESTONE1_JSON}" coolant_temperature length)
string(JSON M1_TEMP_OFFSET GET "${MILESTONE1_JSON}" coolant_temperature offset)
configure_file(
    ${PROJECT_SOURCE_DIR}/config/can/milestone1_definition.hpp.in
    ${PROJECT_BINARY_DIR}/generated/milestone1_definition.hpp
    @ONLY
)

add_library(can_frame_format STATIC src/frame_format.cpp)
target_include_directories(can_frame_format PUBLIC include)

add_library(signal_decoder STATIC src/signal_decoder.cpp)
target_include_directories(signal_decoder PUBLIC include ${PROJECT_BINARY_DIR}/generated)
target_link_libraries(signal_decoder PUBLIC vehicle_service)

add_executable(socketcan_receiver src/socketcan_receiver.cpp)
target_link_libraries(socketcan_receiver PRIVATE can_frame_format signal_decoder vehicle_service)

if(BUILD_TESTING)
    add_executable(can_frame_format_test ../../tests/cpp/frame_format_test.cpp)
    target_link_libraries(can_frame_format_test PRIVATE can_frame_format)
    add_test(NAME can_frame_format_test COMMAND can_frame_format_test)

    add_executable(signal_decoder_test ../../tests/cpp/signal_decoder_test.cpp)
    target_link_libraries(signal_decoder_test PRIVATE signal_decoder)
    add_test(NAME signal_decoder_test COMMAND signal_decoder_test)
endif()
