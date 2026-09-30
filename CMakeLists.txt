cmake_minimum_required(VERSION 3.16)
project(ipc_portable C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(ipc STATIC
    src/ipc_transport.c
    src/ipc_protocol.c
)

target_include_directories(ipc
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(WIN32)
    target_sources(ipc PRIVATE src/ipc_platform_win32.c)
else()
    target_sources(ipc PRIVATE src/ipc_platform_posix.c)
    target_link_libraries(ipc PUBLIC pthread rt)
endif()

add_executable(ipc_server examples/server.c)
add_executable(ipc_client examples/client.c)

target_include_directories(ipc_server PRIVATE examples)
target_include_directories(ipc_client PRIVATE examples)

target_link_libraries(ipc_server PRIVATE ipc)
target_link_libraries(ipc_client PRIVATE ipc)
