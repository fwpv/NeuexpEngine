# Движок собирается в одну статическую библиотеку: ядро, подсистемы и платформенные
# реализации из каталога platform. Точка входа остаётся объектной библиотекой:
# исполняемому файлу она нужна целиком, а не как архив.
add_library(neuexpengine STATIC
    math2d.cpp
    geometry.cpp
    transform.cpp
    camera.cpp
    ui.cpp
    events.cpp
    frame_clock.cpp
    resources.cpp
    image.cpp
    input.cpp
    detail/platform_input.cpp
    renderer.cpp
    renderer_textures.cpp
    renderer_text.cpp
    font.cpp
    texture.cpp
    sprite.cpp
    animation.cpp
    atlas.cpp
    object.cpp
    scene.cpp
    renderer_drawing.cpp
    detail/polygon.cpp
    application.cpp
    gui/events.cpp
    gui/element.cpp
    gui/context.cpp
)
target_include_directories(neuexpengine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(neuexpengine SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/third_party/stb")
target_link_libraries(neuexpengine PRIVATE stb_image project_warnings)

# Графический контекст и отрисовка используют OpenGL 1.1.
if(WIN32)
    target_compile_definitions(neuexpengine PRIVATE NOMINMAX)
    target_link_libraries(neuexpengine PUBLIC opengl32)
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(neuexpengine PUBLIC OpenGL::GL)
endif()

# Исходники платформы добавляет каталог platform: они зависят от целевой ОС.
#
# Точка входа включается напрямую: MinGW запрашивает WinMain после просмотра архивов.
add_library(neuexpengine_entry_point OBJECT entry_point.cpp)
target_include_directories(neuexpengine_entry_point PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(neuexpengine_entry_point PRIVATE project_warnings)
