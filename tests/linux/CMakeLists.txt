add_executable(platform_startup_rollback_test platform_startup_rollback_test.cpp)
target_link_libraries(platform_startup_rollback_test PRIVATE platform)
target_link_options(platform_startup_rollback_test PRIVATE "-Wl,--wrap=glXMakeCurrent"
    "-Wl,--wrap=XGetICValues" "-Wl,--wrap=XDestroyIC" "-Wl,--wrap=XCloseIM"
    "-Wl,--wrap=XMapWindow")
add_test(NAME platform_startup_rollback COMMAND platform_startup_rollback_test)
set_tests_properties(platform_startup_rollback PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30 LABELS "display" RUN_SERIAL TRUE)

add_executable(platform_no_display_test platform_no_display_test.cpp)
target_link_libraries(platform_no_display_test PRIVATE platform)
add_test(NAME platform_no_display COMMAND "${CMAKE_COMMAND}" -E env
    "DISPLAY=" "$<TARGET_FILE:platform_no_display_test>")
set_tests_properties(platform_no_display PROPERTIES TIMEOUT 15)

add_executable(platform_external_destroy_test platform_external_destroy_test.cpp)
target_link_libraries(platform_external_destroy_test PRIVATE platform)
target_link_options(platform_external_destroy_test PRIVATE "-Wl,--wrap=XDestroyWindow")
add_test(NAME platform_external_destroy COMMAND platform_external_destroy_test)
set_tests_properties(platform_external_destroy PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30 LABELS "display" RUN_SERIAL TRUE)

set(TEST_TARGETS platform_startup_rollback_test platform_no_display_test platform_external_destroy_test)
foreach(testTarget IN LISTS TEST_TARGETS)
    target_link_libraries(${testTarget} PRIVATE project_warnings)
endforeach()
