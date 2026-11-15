if(WIN32)
    add_executable(platform_cleanup_errors_test windows_cleanup_errors_test.cpp
        "${PROJECT_SOURCE_DIR}/platform/platform_windows.cpp"
        "${PROJECT_SOURCE_DIR}/platform/window_validation.cpp")
    target_include_directories(platform_cleanup_errors_test PRIVATE "${PROJECT_SOURCE_DIR}/engine")
    target_compile_definitions(platform_cleanup_errors_test PRIVATE NOMINMAX)
    target_link_libraries(platform_cleanup_errors_test PRIVATE neuexpengine_input project_warnings opengl32 gdi32 user32 winmm)

    set(CLEANUP_OVERRIDE "${CMAKE_CURRENT_SOURCE_DIR}/windows_cleanup_override.h")
    if(MSVC)
        target_compile_options(platform_cleanup_errors_test PRIVATE "/FI${CLEANUP_OVERRIDE}")
    else()
        target_compile_options(platform_cleanup_errors_test PRIVATE -include "${CLEANUP_OVERRIDE}")
    endif()
    add_test(NAME platform_cleanup_errors COMMAND platform_cleanup_errors_test)
    set_tests_properties(platform_cleanup_errors PROPERTIES TIMEOUT 30 LABELS "display" RUN_SERIAL TRUE)
elseif(UNIX AND NOT APPLE)
    add_executable(platform_locale_errors_test linux_locale_errors_test.cpp)
    target_link_libraries(platform_locale_errors_test PRIVATE platform project_warnings)
    target_link_options(platform_locale_errors_test PRIVATE
        "-Wl,--wrap=setlocale" "-Wl,--wrap=XSetLocaleModifiers" "-Wl,--wrap=XOpenIM")
    add_test(NAME platform_locale_errors COMMAND platform_locale_errors_test)
    add_test(NAME platform_locale_errors_modifiers COMMAND platform_locale_errors_test)
    set_tests_properties(platform_locale_errors_modifiers PROPERTIES ENVIRONMENT "XMODIFIERS=@im=none")
    set_tests_properties(platform_locale_errors platform_locale_errors_modifiers PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 30 LABELS "display" RUN_SERIAL TRUE)
endif()
