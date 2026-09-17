add_executable(freenamp WIN32
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

target_include_directories(freenamp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/backend/include
    ${COMPILE_LIBS_DIR}
    ${SDL2_ROOT}/include/SDL2
)

target_link_libraries(freenamp PRIVATE
    freenamp_backend
    SDL2
    SDL2main
)

if(WIN32)
    target_link_libraries(freenamp PRIVATE
        ws2_32
        imm32
        winmm
        version
        setupapi
    )
endif()

# Copy SDL2.dll and libmpv-2.dll to the executable folder
add_custom_command(TARGET freenamp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${SDL2_ROOT}/bin/SDL2.dll"
    "$<TARGET_FILE_DIR:freenamp>/SDL2.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${COMPILE_LIBS_DIR}/mpv/libmpv-2.dll"
    "$<TARGET_FILE_DIR:freenamp>/libmpv-2.dll"
)
