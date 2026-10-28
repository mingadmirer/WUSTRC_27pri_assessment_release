# 本项目只使用 GCC（Windows 下为 MinGW g++）。
#
# 配置与构建（在 TESK1 目录执行）：
#   cmake -S . -B build -G "MinGW Makefiles"
#   cmake --build build
#
# 也可以：
#   mkdir build && cd build
#   cmake .. -G "MinGW Makefiles"
#   cmake --build .


# 该cmake文件作为参考，你当然可以自己编写该文件，或使用其他简单方式编译。不过对于长期来说，现在了解cmake是必要的
cmake_minimum_required(VERSION 3.20)

# Windows 下强制使用 MinGW g++，避免 CMake 自动选中 MSVC / Clang。
if(WIN32)
    find_program(MERLIN_GXX NAMES g++.exe g++ HINTS "C:/mingw64/bin")
    if(NOT MERLIN_GXX)
        message(FATAL_ERROR "找不到 g++，请安装 MinGW-w64 并把它加入 PATH。")
    endif()
    set(CMAKE_CXX_COMPILER "${MERLIN_GXX}" CACHE FILEPATH "C++ compiler" FORCE)
endif()

project(merlin_r2_referee LANGUAGES CXX)

# 只保留 GCC 工具链
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR
        "只支持 GCC 工具链。请使用 MinGW g++ 重新配置，例如：\n"
        "  cmake -S . -B build -G \"MinGW Makefiles\"")
endif()

# 使用 C++23。
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 供 IDE / clangd / IntelliSense 使用。
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(referee
    main.cpp
)

target_include_directories(referee PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# 让桥接程序在任意工作目录启动时都能找到 index.html。
target_compile_definitions(referee PRIVATE
    MERLIN_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_options(referee PRIVATE -Wall -Wextra -Wpedantic)

if(WIN32)
    target_link_libraries(referee PRIVATE ws2_32)
endif()

# MinGW 下静态链接 GCC 运行库，避免 PATH 中其他 libstdc++/libgcc 版本
# 与当前编译器不匹配导致可执行文件无法启动。
if(MINGW)
    target_link_options(referee PRIVATE -static)
endif()

# 同时复制一份到可执行文件旁边，便于直接运行 / 调试。
add_custom_command(TARGET referee POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/index.html"
        "$<TARGET_FILE_DIR:referee>/index.html"
    COMMENT "Copying index.html next to referee"
    VERBATIM
)
