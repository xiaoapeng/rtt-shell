cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

project(rtt-shell LANGUAGES C CXX)

# Windows平台特定的静态链接配置（只设置全局变量）
if(WIN32)
    # 设置静态链接Visual C++运行时库
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    
    # 设置全局链接器标志
    if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:libcmt")
    endif()
endif()

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

add_subdirectory(extern/cpp-terminal)

add_executable(${PROJECT_NAME} 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/jlink_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/jlink_find_lib.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/jlink_rtt.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/terminal_display_record.cpp
)

target_include_directories(${PROJECT_NAME} 
    PRIVATE 
${CMAKE_CURRENT_SOURCE_DIR}/src/inc)

target_compile_options(${PROJECT_NAME} PRIVATE ${TARGET_FLAGS})

if(WIN32)
    if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")

        set_target_properties(${PROJECT_NAME} PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )

        target_link_libraries(${PROJECT_NAME} PRIVATE
            libcmt$<$<CONFIG:Debug>:d>
            libvcruntime$<$<CONFIG:Debug>:d>
            libucrt$<$<CONFIG:Debug>:d>
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")

        target_link_libraries(${PROJECT_NAME} PRIVATE
            libcmt$<$<CONFIG:Debug>:d>
            libvcruntime$<$<CONFIG:Debug>:d>
            libucrt$<$<CONFIG:Debug>:d>
        )
    elseif(MINGW)

        target_link_libraries(${PROJECT_NAME} PRIVATE
            -static
            -static-libgcc
            -static-libstdc++
            -lwinpthread
        )
    endif()
    
    
    target_link_libraries(${PROJECT_NAME} PRIVATE
        kernel32
        user32
        advapi32
    )
    
    
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(STATUS "Building for x64 architecture")
    else()
        message(STATUS "Building for x86 architecture")
    endif()
endif()

if(UNIX AND NOT APPLE)

    target_link_options(${PROJECT_NAME} PRIVATE 
        -static-libgcc 
        -static-libstdc++
    )
    
    target_link_libraries(${PROJECT_NAME} PRIVATE 
        pthread 
        dl
        m
    )

endif()

target_link_libraries(${PROJECT_NAME} PRIVATE cpp-terminal::cpp-terminal)