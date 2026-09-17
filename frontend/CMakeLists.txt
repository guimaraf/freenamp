set(FRONTEND_COMMON_SRCS
    src/retro_font.cpp
    src/retro_widgets.cpp
    src/window_dock.cpp
    src/gui_engine.cpp
    src/views/main_view.cpp
    src/views/info_view.cpp
    src/views/eq_view.cpp
    src/views/playlist_view.cpp
    src/views/input_modal.cpp
)

if(WIN32)
    add_library(freenamp_core SHARED
        src/main.cpp
        ${FRONTEND_COMMON_SRCS}
    )

    target_include_directories(freenamp_core PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/backend/include
        ${COMPILE_LIBS_DIR}
        ${SDL2_ROOT}/include/SDL2
    )

    target_link_libraries(freenamp_core PRIVATE
        freenamp_backend
        SDL2
        ws2_32
        imm32
        winmm
        version
        setupapi
    )

    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/res/freenamp.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/freenamp.rc
        @ONLY
    )
    set(FREENAMP_RC ${CMAKE_CURRENT_BINARY_DIR}/freenamp.rc)

    target_link_options(freenamp_core PRIVATE
        -static-libgcc
        -static-libstdc++
        -Wl,-Bstatic,--whole-archive
        -lwinpthread
        -Wl,--no-whole-archive,-Bdynamic
    )

    # Native Win32 launcher executable (no console window) with embedded icon
    add_executable(freenamp WIN32
        src/launcher.c
        ${FREENAMP_RC}
    )

    target_link_options(freenamp PRIVATE
        -static
        -static-libgcc
    )
    target_link_libraries(freenamp PRIVATE
        user32
        kernel32
    )

    # Copy SDL2.dll, libmpv-2.dll and freenamp_core to core/ folder in output
    add_custom_command(TARGET freenamp_core POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:freenamp>/core"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL2_ROOT}/bin/SDL2.dll"
        "$<TARGET_FILE_DIR:freenamp>/core/SDL2.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${COMPILE_LIBS_DIR}/mpv/libmpv-2.dll"
        "$<TARGET_FILE_DIR:freenamp>/core/libmpv-2.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:freenamp_core>"
        "$<TARGET_FILE_DIR:freenamp>/core/$<TARGET_FILE_NAME:freenamp_core>"
    )

elseif(UNIX AND NOT APPLE)
    add_library(freenamp_core SHARED
        ${FRONTEND_COMMON_SRCS}
    )

    target_include_directories(freenamp_core PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/backend/include
        ${COMPILE_LIBS_DIR}
        ${SDL2_INCLUDE_DIRS}
        ${MPV_INCLUDE_DIRS}
    )

    target_link_libraries(freenamp_core PRIVATE
        freenamp_backend
        ${FREENAMP_SDL2_LIB}
        ${MPV_LIBRARIES}
        pthread
    )

    set_target_properties(freenamp_core PROPERTIES
        BUILD_RPATH "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
    )

    # Standalone ELF executable linking against freenamp_core and resolving from $ORIGIN/core
    add_executable(freenamp
        src/main.cpp
    )

    target_include_directories(freenamp PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/backend/include
        ${COMPILE_LIBS_DIR}
        ${SDL2_INCLUDE_DIRS}
    )

    target_link_libraries(freenamp PRIVATE
        freenamp_core
        freenamp_backend
        ${FREENAMP_SDL2_LIB}
        pthread
    )

    set_target_properties(freenamp PROPERTIES
        BUILD_RPATH "$ORIGIN/core:$ORIGIN"
        INSTALL_RPATH "$ORIGIN/core:$ORIGIN"
    )

    # Copy libfreenamp_core.so to core/ folder in output
    add_custom_command(TARGET freenamp_core POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:freenamp>/core"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:freenamp_core>"
        "$<TARGET_FILE_DIR:freenamp>/core/$<TARGET_FILE_NAME:freenamp_core>"
    )
endif()
