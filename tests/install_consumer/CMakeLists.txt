cmake_minimum_required(VERSION 3.14)
project(event_hub_install_consumer LANGUAGES CXX)

find_package(event-hub-cpp CONFIG REQUIRED)

if(NOT TARGET event_hub::event_hub)
    message(FATAL_ERROR "event_hub::event_hub target was not exported")
endif()
if(NOT TARGET event-hub-cpp::event-hub-cpp)
    message(FATAL_ERROR "event-hub-cpp::event-hub-cpp alias target was not exported")
endif()

add_executable(event_hub_install_consumer main.cpp)
target_link_libraries(event_hub_install_consumer PRIVATE event_hub::event_hub)

if(MSVC)
    target_compile_options(event_hub_install_consumer PRIVATE /W4)
else()
    target_compile_options(event_hub_install_consumer PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME event_hub_install_consumer COMMAND event_hub_install_consumer)
set_tests_properties(event_hub_install_consumer PROPERTIES TIMEOUT 15)
