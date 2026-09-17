add_library(freenamp_core SHARED
    src/main.cpp
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

target_include_directories(freenamp_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/backend/include
    ${COMPILE_LIBS_DIR}
    ${SDL2_ROOT}/include/SDL2
)

target_link_libraries(freenamp_core PRIVATE
    freenamp_backend
    SDL2
)

if(WIN32)
    target_link_libraries(freenamp_core PRIVATE
        ws2_32
        imm32
        winmm
        version
        setupapi
    )
endif()

# Native Win32 launcher executable (no console window)
add_executable(freenamp WIN32
    src/launcher.cpp
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
