add_library(engine_math STATIC math2d.cpp geometry.cpp transform.cpp)
target_include_directories(engine_math PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(engine_math PRIVATE project_warnings)

add_library(engine_camera STATIC camera.cpp)
target_include_directories(engine_camera PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(engine_camera PUBLIC engine_math PRIVATE project_warnings)

add_library(engine_ui STATIC ui.cpp)
target_include_directories(engine_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(engine_ui PUBLIC engine_camera PRIVATE project_warnings)

add_library(engine_events STATIC events.cpp)
target_include_directories(engine_events PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(engine_events PRIVATE project_warnings)

add_library(engine_time STATIC frame_clock.cpp)
target_include_directories(engine_time PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(engine_time PRIVATE project_warnings)

add_library(engine_resources STATIC resources.cpp image.cpp ../platform/resource_path.cpp)
target_include_directories(engine_resources PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(engine_resources SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/third_party/stb")
target_link_libraries(engine_resources PRIVATE stb_image project_warnings)

if(BUILD_DEMOS OR BUILD_TESTING)
    # Точка входа включается напрямую: MinGW запрашивает WinMain после просмотра архивов.
    add_library(engine_entry_point OBJECT entry_point.cpp)
    target_include_directories(engine_entry_point PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(engine_entry_point PRIVATE project_warnings)

    add_library(engine_renderer STATIC renderer.cpp renderer_textures.cpp renderer_text.cpp font.cpp texture.cpp sprite.cpp animation.cpp atlas.cpp renderer_drawing.cpp detail/polygon.cpp)
    target_include_directories(engine_renderer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(engine_renderer PUBLIC engine_ui engine_resources PRIVATE project_warnings)
    if(WIN32)
        target_compile_definitions(engine_renderer PRIVATE NOMINMAX)
        target_link_libraries(engine_renderer PRIVATE opengl32)
    else()
        find_package(OpenGL REQUIRED)
        target_link_libraries(engine_renderer PRIVATE OpenGL::GL)
    endif()

    add_library(engine_application STATIC application.cpp)
    target_include_directories(engine_application PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(engine_application PUBLIC platform engine_time engine_renderer PRIVATE project_warnings)
endif()

if(BUILD_DEMOS OR BUILD_TESTING)
    add_library(engine_gui STATIC gui/events.cpp gui/element.cpp gui/context.cpp)
    target_include_directories(engine_gui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(engine_gui PUBLIC engine_renderer PRIVATE project_warnings)
endif()
