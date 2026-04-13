cmake_minimum_required(VERSION 4.1)
project(clox C)

set(CMAKE_C_STANDARD 11)

# The shared source files (everything except main entry points)
set(CLOX_SOURCES
        chunk.c
        debug.c
        value.c
        memory.c
        vm.c
        compiler.c
        scanner.c
        object.c
        table.c
)

# Original clox interpreter
add_executable(clox
        main.c
        ${CLOX_SOURCES}
)

# Benchmark executable
add_executable(bench
        bench.c
        ${CLOX_SOURCES}
)