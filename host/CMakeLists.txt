cmake_minimum_required(VERSION 3.10)
project(SerialC++ VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Ensure pthread is found and linked
find_package(Threads REQUIRED)

add_library(serial_lib STATIC
    src/PortEnumerator.cpp
    src/SerialPort.cpp
    src/AsyncSerialReceiver.cpp
    src/MessageBufferObserver.cpp
    src/SerialClient.cpp
    src/Stm32Bridge.cpp
)

target_include_directories(serial_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(serial_lib PRIVATE Threads::Threads)

# Example executables
add_executable(serial_example example/main.cpp)
target_link_libraries(serial_example PRIVATE serial_lib)

add_executable(bridge_example example/demo_bridge.cpp)
target_link_libraries(bridge_example PRIVATE serial_lib)

