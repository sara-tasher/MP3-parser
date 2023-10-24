cmake_minimum_required(VERSION 3.20)
project(MP3_parser)

set(CMAKE_CXX_STANDARD 17)

add_executable(MP3_parser main.cpp parser.h parser.cpp)
