if(NOT TARGET pico_core)
    message(FATAL_ERROR
        "pico_mqtt requires pico_core, but the CMake target 'pico_core' was not found. "
        "Add add_subdirectory(lib/pico_core) before add_subdirectory(lib/pico_mqtt) "
        "in your root CMakeLists.txt."
    )
endif()

if(NOT TARGET pico_net)
    message(FATAL_ERROR
        "pico_mqtt requires pico_net, but the CMake target 'pico_net' was not found. "
        "Add add_subdirectory(lib/pico_net) before add_subdirectory(lib/pico_mqtt) "
        "in your root CMakeLists.txt."
    )
endif()

add_library(pico_mqtt STATIC
    src/mqtt_client.cpp
    src/mqtt_encoder.cpp
    src/mqtt_parser.cpp
)

target_include_directories(pico_mqtt PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(pico_mqtt PUBLIC
    pico_stdlib
    pico_core
    pico_net
)