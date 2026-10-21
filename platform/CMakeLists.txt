# Одна реализация платформы и один набор системных зависимостей для всех целей.
if(WIN32)
    add_library(platform STATIC platform_windows.cpp)
    # opengl32 — OpenGL/WGL, gdi32 — HDC, user32 — окна и сообщения,
    # winmm — запрос точности системного таймера.
    target_link_libraries(platform PUBLIC opengl32 gdi32 user32 winmm)
elseif(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    find_package(OpenGL REQUIRED)
    add_library(platform STATIC platform_x11.cpp)
    # Импортированные цели содержат пути к заголовкам и системным библиотекам.
    target_link_libraries(platform PUBLIC X11::X11 OpenGL::GL)
else()
    message(FATAL_ERROR "This project currently supports Windows and Linux/X11.")
endif()

# PUBLIC делает общий интерфейс доступным приложениям, подключившим platform.
add_library(engine_input STATIC ../engine/input.cpp ../engine/platform_input.cpp)
target_include_directories(engine_input PUBLIC "${PROJECT_SOURCE_DIR}/engine")
target_link_libraries(engine_input PRIVATE project_warnings)
target_link_libraries(platform PUBLIC engine_input)
target_include_directories(platform PUBLIC "${PROJECT_SOURCE_DIR}/engine")
# Предупреждения проверяют реализацию платформы, но не становятся её публичным API.
target_link_libraries(platform PRIVATE project_warnings)
