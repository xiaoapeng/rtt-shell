cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

project(rtt-shell LANGUAGES C CXX)

set(TARGET_FLAGS 
    "-Wall"
    "-Wextra"
    "-Wconversion"
    "-Wsign-conversion"
    "-Wno-psabi"
    "$<$<CONFIG:Debug>:-g;-O0>"
    "$<$<CONFIG:Release>:-g;-O3>"
    "$<$<CONFIG:MinSizeRel>:-g;-Os>"
    "$<$<CONFIG:RelWithDebInfo>:-g;-Os>"
)

add_executable(${PROJECT_NAME} 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/jlink_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/jlink_find_lib.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/jlink_rtt.cpp
)

target_include_directories(${PROJECT_NAME} 
    PRIVATE 
${CMAKE_CURRENT_SOURCE_DIR}/src/inc)

target_compile_options(${PROJECT_NAME} PRIVATE ${TARGET_FLAGS})

# 准备链接jlink库
