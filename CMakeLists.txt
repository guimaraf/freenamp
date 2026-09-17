cmake_minimum_required(VERSION 3.20)
project(Freenamp VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Directories
set(COMPILE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/compile")
set(COMPILE_LIBS_DIR "${COMPILE_DIR}/libs")
set(COMPILE_BIN_DIR "${COMPILE_DIR}/bin")

# Global includes
include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/backend/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/frontend/include"
    "${COMPILE_LIBS_DIR}"
    "${COMPILE_LIBS_DIR}/mpv/include"
)

# libmpv Local MinGW Setup
if(EXISTS "${COMPILE_LIBS_DIR}/mpv")
    link_directories("${COMPILE_LIBS_DIR}/mpv")
endif()

# SDL2 Local MinGW Setup
if(EXISTS "${COMPILE_LIBS_DIR}/SDL2-2.30.12/x86_64-w64-mingw32")
    set(SDL2_ROOT "${COMPILE_LIBS_DIR}/SDL2-2.30.12/x86_64-w64-mingw32")
    include_directories("${SDL2_ROOT}/include/SDL2")
    link_directories("${SDL2_ROOT}/lib")
endif()

# Subdirectories
add_subdirectory(backend)
add_subdirectory(frontend)
