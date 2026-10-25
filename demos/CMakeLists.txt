add_executable(circle
    circle/game.cpp
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

add_executable(cat cat/main.cpp cat/game.cpp)
if(WIN32)
    set_target_properties(cat PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
target_link_libraries(cat PRIVATE engine_entry_point engine_application engine_renderer project_warnings)
set_target_properties(cat PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

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
add_dependencies(cat demo_assets)
