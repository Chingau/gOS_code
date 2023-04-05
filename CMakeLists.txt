cmake_minimum_required(VERSION 3.23)
project(gos C)

set(CMAKE_C_STANDARD 11)

add_executable(gos main.c tools/floppy.c tools/fileinfo.c)
