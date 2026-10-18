# Ресурсы общие для тестов обеих ОС и не зависят от расположения исходника теста.
set(TEST_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")

add_subdirectory(common)
if(WIN32)
    add_subdirectory(windows)
elseif(UNIX AND NOT APPLE)
    add_subdirectory(linux)
endif()
