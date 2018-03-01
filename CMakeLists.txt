cmake_minimum_required(VERSION 3.8)
project(Lab2)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES src/kernel_start.c include/kernel.h src/init.c src/kernel_handlers.c src/kernel_syscall.c src/idle.c src/kernel_memory.c)
add_executable(Lab2 ${SOURCE_FILES})

include_directories(ref)