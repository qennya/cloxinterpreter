cmake_minimum_required(VERSION 4.1)
project(clox C)

set(CMAKE_C_STANDARD 11)

add_executable(clox
        main.c
        chunk.c
        debug.c
        value.c
        memory.c
)
