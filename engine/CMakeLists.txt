add_library(neuexpengine_math STATIC math2d.cpp geometry.cpp transform.cpp)
target_include_directories(neuexpengine_math PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(neuexpengine_math PRIVATE project_warnings)

add_library(neuexpengine_camera STATIC camera.cpp)
target_include_directories(neuexpengine_camera PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(neuexpengine_camera PUBLIC neuexpengine_math PRIVATE project_warnings)

add_library(neuexpengine_ui STATIC ui.cpp)
target_include_directories(neuexpengine_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(neuexpengine_ui PUBLIC neuexpengine_camera PRIVATE project_warnings)

add_library(neuexpengine_events STATIC events.cpp)
target_include_directories(neuexpengine_events PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(neuexpengine_events PRIVATE project_warnings)

add_library(neuexpengine_time STATIC frame_clock.cpp)
target_include_directories(neuexpengine_time PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(neuexpengine_time PRIVATE project_warnings)

add_library(neuexpengine_resources STATIC resources.cpp image.cpp ../platform/resource_path.cpp)
target_include_directories(neuexpengine_resources PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(neuexpengine_resources SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/third_party/stb")
target_link_libraries(neuexpengine_resources PRIVATE stb_image project_warnings)

if(BUILD_DEMOS OR BUILD_TESTING)
    # Точка входа включается напрямую: MinGW запрашивает WinMain после просмотра архивов.
    add_library(neuexpengine_entry_point OBJECT entry_point.cpp)
    target_include_directories(neuexpengine_entry_point PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(neuexpengine_entry_point PRIVATE project_warnings)

    add_library(neuexpengine_renderer STATIC renderer.cpp renderer_textures.cpp renderer_text.cpp font.cpp texture.cpp sprite.cpp animation.cpp atlas.cpp renderer_drawing.cpp detail/polygon.cpp)
    target_include_directories(neuexpengine_renderer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(neuexpengine_renderer PUBLIC neuexpengine_ui neuexpengine_resources PRIVATE project_warnings)
    if(WIN32)
        target_compile_definitions(neuexpengine_renderer PRIVATE NOMINMAX)
        target_link_libraries(neuexpengine_renderer PRIVATE opengl32)
    else()
        find_package(OpenGL REQUIRED)
        target_link_libraries(neuexpengine_renderer PRIVATE OpenGL::GL)
    endif()

    add_library(neuexpengine_application STATIC application.cpp)
    target_include_directories(neuexpengine_application PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(neuexpengine_application PUBLIC platform neuexpengine_time neuexpengine_renderer PRIVATE project_warnings)
endif()

if(BUILD_DEMOS OR BUILD_TESTING)
    add_library(neuexpengine_gui STATIC gui/events.cpp gui/element.cpp gui/context.cpp)
    target_include_directories(neuexpengine_gui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(neuexpengine_gui PUBLIC neuexpengine_renderer PRIVATE project_warnings)
endif()
