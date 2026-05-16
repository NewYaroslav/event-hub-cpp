cmake_minimum_required(VERSION 3.14)
project(event_hub_install_consumer LANGUAGES CXX)

option(EVENT_HUB_CPP_INSTALL_CONSUMER_USE_CALENDAR
    "Build install consumer calendar scheduler check"
    OFF
)

find_package(event-hub-cpp CONFIG REQUIRED)

if(NOT TARGET event_hub::event_hub)
    message(FATAL_ERROR "event_hub::event_hub target was not exported")
endif()
if(NOT TARGET event-hub-cpp::event-hub-cpp)
    message(FATAL_ERROR "event-hub-cpp::event-hub-cpp alias target was not exported")
endif()

add_executable(event_hub_install_consumer main.cpp)
target_link_libraries(event_hub_install_consumer PRIVATE event_hub::event_hub)

if(EVENT_HUB_CPP_INSTALL_CONSUMER_USE_CALENDAR)
    add_executable(event_hub_install_consumer_calendar calendar.cpp)
    target_link_libraries(event_hub_install_consumer_calendar PRIVATE event_hub::event_hub)
endif()

foreach(target_name IN ITEMS event_hub_install_consumer event_hub_install_consumer_calendar)
    if(TARGET ${target_name})
        if(MSVC)
            target_compile_options(${target_name} PRIVATE /W4)
        else()
            target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    endif()
endforeach()

enable_testing()
add_test(NAME event_hub_install_consumer COMMAND event_hub_install_consumer)
set_tests_properties(event_hub_install_consumer PROPERTIES TIMEOUT 15)

if(EVENT_HUB_CPP_INSTALL_CONSUMER_USE_CALENDAR)
    add_test(NAME event_hub_install_consumer_calendar
             COMMAND event_hub_install_consumer_calendar)
    set_tests_properties(event_hub_install_consumer_calendar PROPERTIES TIMEOUT 15)
endif()
