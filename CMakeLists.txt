project(uCHAT C)
cmake_minimum_required(VERSION 2.5)

set(CURSES_NEED_NCURSES TRUE)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)

find_package(Curses REQUIRED)
find_package(Threads REQUIRED)

include_directories(${CURSES_INCLUDE_DIR})

add_executable(uchat uchat.c tty_layer.c packet_layer.c crc32.c)
target_link_libraries(uchat ${CMAKE_THREAD_LIBS_INIT} ${CURSES_LIBRARIES})