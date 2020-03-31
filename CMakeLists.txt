cmake_minimum_required(VERSION 3.15)
project(project2_Wang_Ji_Zhang)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(project2_Wang_Ji_Zhang
        recvfile.cc
        sendfile.cc
        utils.h)
