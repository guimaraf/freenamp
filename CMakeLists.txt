cmake_minimum_required(VERSION 3.20)
project(Freenamp VERSION 1.0.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

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

if(WIN32)
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
elseif(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(MPV REQUIRED mpv)
    find_package(SDL2 QUIET)
    if(NOT SDL2_FOUND)
        pkg_check_modules(SDL2 REQUIRED sdl2)
    endif()
    if(TARGET SDL2::SDL2)
        set(FREENAMP_SDL2_LIB SDL2::SDL2)
    elseif(SDL2_LIBRARIES)
        set(FREENAMP_SDL2_LIB ${SDL2_LIBRARIES})
    else()
        set(FREENAMP_SDL2_LIB SDL2)
    endif()

    include_directories(
        ${MPV_INCLUDE_DIRS}
        ${SDL2_INCLUDE_DIRS}
    )

    # RPATH settings for 100% portable execution ($ORIGIN/core:$ORIGIN)
    set(CMAKE_SKIP_BUILD_RPATH FALSE)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)
endif()

# Subdirectories
add_subdirectory(backend)
add_subdirectory(frontend)
