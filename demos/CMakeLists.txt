add_executable(circle
    circle/game.cpp
    circle/entities.cpp
    circle/scene.cpp
)

if(WIN32)
    set_target_properties(circle PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_link_libraries(circle PRIVATE engine_entry_point engine_application engine_math project_warnings)

set_target_properties(circle PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

add_executable(input_demo input/main.cpp)
if(WIN32)
    set_target_properties(input_demo PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
target_link_libraries(input_demo PRIVATE engine_entry_point engine_application engine_events engine_math project_warnings)
set_target_properties(input_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

add_executable(math_demo math/main.cpp)
if(WIN32)
    set_target_properties(math_demo PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
target_link_libraries(math_demo PRIVATE engine_entry_point engine_application engine_math project_warnings)
set_target_properties(math_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

add_executable(cleanup_demo cleanup/main.cpp cleanup/game.cpp)
if(WIN32)
    set_target_properties(cleanup_demo PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
target_link_libraries(cleanup_demo PRIVATE engine_entry_point engine_application engine_camera project_warnings)
set_target_properties(cleanup_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

add_executable(dpi_demo dpi/main.cpp)
if(WIN32)
    set_target_properties(dpi_demo PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
target_link_libraries(dpi_demo PRIVATE engine_entry_point engine_application engine_ui project_warnings)
set_target_properties(dpi_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

add_executable(rendering_demo rendering/main.cpp)
if(WIN32)
    set_target_properties(rendering_demo PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
target_link_libraries(rendering_demo PRIVATE engine_entry_point engine_application engine_renderer project_warnings)
set_target_properties(rendering_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

set(DEMO_ASSET_DIRECTORY "${PROJECT_BINARY_DIR}/assets")
if(CMAKE_CONFIGURATION_TYPES)
    set(DEMO_ASSET_DIRECTORY "${PROJECT_BINARY_DIR}/$<CONFIG>/assets")
endif()
add_custom_target(demo_assets ALL
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${PROJECT_SOURCE_DIR}/assets" "${DEMO_ASSET_DIRECTORY}")
add_subdirectory(cat)

add_executable(sprites_demo sprites/main.cpp)
if(WIN32)
    set_target_properties(sprites_demo PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
target_link_libraries(sprites_demo PRIVATE engine_entry_point engine_application project_warnings)
set_target_properties(sprites_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

add_executable(fonts_demo fonts/main.cpp)
if(WIN32)
    set_target_properties(fonts_demo PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
target_link_libraries(fonts_demo PRIVATE engine_entry_point engine_application project_warnings)
set_target_properties(fonts_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
add_dependencies(fonts_demo demo_assets)

if(MSVC)
    target_compile_options(fonts_demo PRIVATE /utf-8)
endif()

add_executable(gui_demo gui/main.cpp gui/settings.cpp)
if(WIN32)
    set_target_properties(gui_demo PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
target_link_libraries(gui_demo PRIVATE engine_entry_point engine_application engine_gui project_warnings)
set_target_properties(gui_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
add_dependencies(gui_demo demo_assets)
if(MSVC)
    target_compile_options(gui_demo PRIVATE /utf-8)
endif()
