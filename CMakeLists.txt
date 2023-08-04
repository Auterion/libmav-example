cmake_minimum_required(VERSION 3.20)
project(libmav-example)

set(CMAKE_CXX_STANDARD 17)

add_executable(libmav-example main.cpp)
target_include_directories(libmav-example PRIVATE ${CMAKE_SOURCE_DIR}/libmav/include)
file(GLOB MAVLINK_XML ${CMAKE_CURRENT_SOURCE_DIR}/mavlink/message_definitions/v1.0/*.xml)
file(COPY ${MAVLINK_XML} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/mavlink)
