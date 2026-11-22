# Движок собирается в одну статическую библиотеку: ядро, подсистемы и платформенные
# реализации из каталога platform. Точка входа остаётся объектной библиотекой:
# исполняемому файлу она нужна целиком, а не как архив.
add_library(neuexpengine STATIC
    core/math2d.cpp
    core/geometry.cpp
    core/transform.cpp
    core/camera.cpp
    core/resources.cpp
    core/image.cpp
    core/texture.cpp
    app/events.cpp
    app/frame_clock.cpp
    app/input.cpp
    app/application.cpp
    app/entry_point.cpp
    graphics/renderer.cpp
    graphics/renderer_textures.cpp
    graphics/renderer_text.cpp
    graphics/renderer_drawing.cpp
    graphics/ui_view.cpp
    graphics/font.cpp
    graphics/sprite.cpp
    graphics/atlas.cpp
    graphics/animation.cpp
    scene/object.cpp
    scene/scene.cpp
    collision/shape.cpp
    collision/query.cpp
    gui/events.cpp
    gui/element.cpp
    gui/context.cpp
    detail/platform_input.cpp
    detail/polygon.cpp
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
add_library(neuexpengine_entry_point OBJECT app/entry_point.cpp)
target_include_directories(neuexpengine_entry_point PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(neuexpengine_entry_point PRIVATE project_warnings)
