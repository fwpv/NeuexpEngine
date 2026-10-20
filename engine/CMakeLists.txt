# Подписки подключаются явно; платформе и прямому разбору ввода они не нужны.
add_library(engine_events STATIC events.cpp)
target_include_directories(engine_events PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(engine_events PRIVATE project_warnings)
