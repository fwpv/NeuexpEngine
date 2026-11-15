add_executable(platform_external_destroy_test platform_external_destroy_test.cpp)
target_link_libraries(platform_external_destroy_test PRIVATE neuexpengine)
target_link_options(platform_external_destroy_test PRIVATE "-Wl,--wrap=XDestroyWindow")
add_test(NAME platform_external_destroy COMMAND platform_external_destroy_test)
set_tests_properties(platform_external_destroy PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30 LABELS "display" RUN_SERIAL TRUE)

target_link_libraries(platform_external_destroy_test PRIVATE project_warnings)

add_executable(platform_no_display_test platform_no_display_test.cpp)
target_link_libraries(platform_no_display_test PRIVATE neuexpengine project_warnings)
add_test(NAME platform_no_display COMMAND platform_no_display_test)
set_tests_properties(platform_no_display PROPERTIES ENVIRONMENT "DISPLAY=" TIMEOUT 15)

add_executable(platform_startup_rollback_test platform_startup_rollback_test.cpp)
target_link_libraries(platform_startup_rollback_test PRIVATE neuexpengine project_warnings)
target_link_options(platform_startup_rollback_test PRIVATE
    "-Wl,--wrap=glXMakeCurrent" "-Wl,--wrap=XGetICValues" "-Wl,--wrap=XDestroyIC"
    "-Wl,--wrap=XCloseIM" "-Wl,--wrap=XMapWindow")
add_test(NAME platform_startup_rollback COMMAND platform_startup_rollback_test)

add_executable(platform_errors_test platform_errors_test.cpp)
target_link_libraries(platform_errors_test PRIVATE neuexpengine project_warnings)
target_link_options(platform_errors_test PRIVATE
    "-Wl,--wrap=XOpenDisplay" "-Wl,--wrap=XkbQueryExtension" "-Wl,--wrap=XkbSelectEventDetails"
    "-Wl,--wrap=XkbGetKeyboard" "-Wl,--wrap=glXMakeCurrent" "-Wl,--wrap=XDestroyIC" "-Wl,--wrap=XCloseIM")
add_test(NAME platform_errors COMMAND platform_errors_test)
set_tests_properties(platform_startup_rollback platform_errors PROPERTIES
    SKIP_RETURN_CODE 77 TIMEOUT 30 LABELS "display" RUN_SERIAL TRUE)
