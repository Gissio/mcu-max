cmake_minimum_required (VERSION 3.0)

project (mcu-max)

set(CMAKE_C_STANDARD 99)

add_executable (mcu-max main.c mcu-max.c)
