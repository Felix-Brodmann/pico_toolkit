if(NOT TARGET pico_core)
    message(FATAL_ERROR
        "pico_net requires pico_core, but the CMake target 'pico_core' was not found. "
        "Add add_subdirectory(lib/pico_core) before add_subdirectory(lib/pico_net) "
        "in your root CMakeLists.txt."
    )
endif()

add_library(pico_net STATIC
    src/network.cpp
    src/tcp_connection.cpp
    src/udp_connection.cpp
    src/tcp_listener.cpp
    src/udp_listener.cpp
    src/dns_server.cpp
    src/dhcp_server.cpp
)

target_include_directories(pico_net PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/dhcp
)

target_link_libraries(pico_net PUBLIC
    pico_stdlib
    pico_cyw43_arch_lwip_poll
    pico_core
)