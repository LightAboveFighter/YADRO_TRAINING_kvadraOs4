cmake_minimum_required(VERSION 3.10.0)
project(YADRO_TRAINING_kvadraOs4 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    main.cpp 
    sys_classes.cpp 
    sys_classes.hpp
)

add_executable(YADRO_TRAINING_kvadraOs4 ${SOURCES})

