add_executable(circle
    circle/game.cpp
)

if(WIN32)
    set_target_properties(circle PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_link_libraries(circle PRIVATE engine_application project_warnings stb_image miniaudio)

set_target_properties(circle PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

add_executable(input_demo input/main.cpp)
if(WIN32)
    set_target_properties(input_demo PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
target_link_libraries(input_demo PRIVATE engine_application engine_events project_warnings)
set_target_properties(input_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
