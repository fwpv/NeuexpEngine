add_executable(circle
    circle/game.cpp
)

if(WIN32)
    # Графическая подсистема Windows использует WinMain и не создаёт консольное окно.
    set_target_properties(circle PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# platform передаёт общий заголовок движка и системные зависимости.
target_link_libraries(circle PRIVATE platform project_warnings stb_image miniaudio)

# Сохраняем путь build/circle (build/circle.exe в Windows) для существующих команд запуска.
set_target_properties(circle PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
