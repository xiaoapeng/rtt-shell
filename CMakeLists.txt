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
set(CPPTERMINAL_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CPPTERMINAL_ENABLE_INSTALL  OFF CACHE BOOL "" FORCE)
set(CPPTERMINAL_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(CPPTERMINAL_ENABLE_DOCS OFF CACHE BOOL "" FORCE)

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

# Windows平台特定的静态链接配置（在target创建后设置）
if(WIN32)
    if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        # 针对不同配置的链接器标志
        set_target_properties(${PROJECT_NAME} PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        
        # 添加静态链接的特定库（x64和x86使用相同的库名）
        target_link_libraries(${PROJECT_NAME} PRIVATE
            libcmt$<$<CONFIG:Debug>:d>
            libvcruntime$<$<CONFIG:Debug>:d>
            libucrt$<$<CONFIG:Debug>:d>
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # 对于Clang，使用更简单的静态链接方法
        # 直接链接静态库，让链接器自动处理依赖关系
        target_link_libraries(${PROJECT_NAME} PRIVATE
            libcmt$<$<CONFIG:Debug>:d>
            libvcruntime$<$<CONFIG:Debug>:d>
            libucrt$<$<CONFIG:Debug>:d>
        )
    elseif(MINGW)
        # MinGW/GCC静态链接配置
        target_link_libraries(${PROJECT_NAME} PRIVATE
            -static
            -static-libgcc
            -static-libstdc++
            -lwinpthread
        )
    endif()
    
    # 添加Windows API库（这些通常是系统自带的，但确保链接）
    target_link_libraries(${PROJECT_NAME} PRIVATE
        kernel32
        user32
        advapi32
    )
    
    # 显示架构信息
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(STATUS "Building for x64 architecture")
    else()
        message(STATUS "Building for x86 architecture")
    endif()
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE cpp-terminal::cpp-terminal)