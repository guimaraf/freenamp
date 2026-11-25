add_library(freenamp_backend STATIC
    src/yt_resolver.cpp
    src/audio_engine.cpp
    src/playlist_manager.cpp
    src/core_controller.cpp
)

target_include_directories(freenamp_backend PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${COMPILE_LIBS_DIR}
    ${COMPILE_LIBS_DIR}/mpv/include
)

set_target_properties(freenamp_backend PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

if(WIN32)
    target_link_libraries(freenamp_backend PUBLIC
        mpv
        ws2_32
        ole32
        wininet
    )

    # Copy libmpv-2.dll to binary dir
    add_custom_command(TARGET freenamp_backend POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${COMPILE_LIBS_DIR}/mpv/libmpv-2.dll"
        "${CMAKE_CURRENT_BINARY_DIR}/libmpv-2.dll"
    )
else()
    target_include_directories(freenamp_backend PUBLIC
        ${MPV_INCLUDE_DIRS}
    )
    target_link_libraries(freenamp_backend PUBLIC
        ${MPV_LIBRARIES}
        pthread
    )
endif()

# Test runner for Phase 2 (YT resolver test)
add_executable(test_yt_resolver
    tests/test_yt_resolver.cpp
)

target_link_libraries(test_yt_resolver PRIVATE
    freenamp_backend
)

# Test runner for Phases 3 & 4 (Audio, DSP, Playlist & Prefetch)
add_executable(test_audio_playlist
    tests/test_audio_playlist.cpp
)

target_link_libraries(test_audio_playlist PRIVATE
    freenamp_backend
)

add_executable(test_url_flow
    tests/test_url_flow.cpp
)

target_link_libraries(test_url_flow PRIVATE
    freenamp_backend
)

if(BUILD_TESTING)
    foreach(test_target IN ITEMS test_yt_resolver test_audio_playlist test_url_flow)
        # Keep assert-based checks active in CI's Release configuration.
        if(MSVC)
            target_compile_options(${test_target} PRIVATE /UNDEBUG)
        else()
            target_compile_options(${test_target} PRIVATE -UNDEBUG)
        endif()
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${test_target}_data")
    endforeach()
    add_test(NAME resolver_offline COMMAND test_yt_resolver --offline)
    add_test(NAME audio_playlist_offline COMMAND test_audio_playlist --offline)
    add_test(NAME url_flow_offline COMMAND test_url_flow "$<TARGET_FILE:test_url_flow>")
    set_tests_properties(resolver_offline PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test_yt_resolver_data")
    set_tests_properties(audio_playlist_offline PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test_audio_playlist_data")
    set_tests_properties(url_flow_offline PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test_url_flow_data")
    set_tests_properties(resolver_offline audio_playlist_offline url_flow_offline PROPERTIES TIMEOUT 30)
endif()

if(WIN32)
    # Copy dlls to test target output
    add_custom_command(TARGET test_audio_playlist POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${COMPILE_LIBS_DIR}/mpv/libmpv-2.dll"
        "$<TARGET_FILE_DIR:test_audio_playlist>/libmpv-2.dll"
    )
else()
    set_target_properties(test_yt_resolver test_audio_playlist test_url_flow PROPERTIES
        BUILD_RPATH "$ORIGIN:$ORIGIN/core:$ORIGIN/../frontend/core"
        INSTALL_RPATH "$ORIGIN:$ORIGIN/core"
    )
endif()
