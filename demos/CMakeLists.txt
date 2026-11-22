# Демонстрационные приложения и игры на движке. Любая новая игра собирается так же:
# add_executable, линковка neuexpengine плюс neuexpengine_entry_point, вывод в каталог
# сборки и копия ресурсов из assets. Правила одинаковы для демо и будущих игр.
add_executable(circle
    circle/game.cpp
    circle/entities.cpp
    circle/scene.cpp
)

if(WIN32)
    set_target_properties(circle PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_link_libraries(circle PRIVATE neuexpengine_entry_point neuexpengine project_warnings)

set_target_properties(circle PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

add_executable(input_demo input/main.cpp)
if(WIN32)
    set_target_properties(input_demo PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
target_link_libraries(input_demo PRIVATE neuexpengine_entry_point neuexpengine project_warnings)
set_target_properties(input_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

add_executable(math_demo math/main.cpp)
if(WIN32)
    set_target_properties(math_demo PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
target_link_libraries(math_demo PRIVATE neuexpengine_entry_point neuexpengine project_warnings)
set_target_properties(math_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

add_executable(cleanup_demo cleanup/main.cpp cleanup/game.cpp)
if(WIN32)
    set_target_properties(cleanup_demo PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
target_link_libraries(cleanup_demo PRIVATE neuexpengine_entry_point neuexpengine project_warnings)
set_target_properties(cleanup_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

add_executable(dpi_demo dpi/main.cpp)
if(WIN32)
    set_target_properties(dpi_demo PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
target_link_libraries(dpi_demo PRIVATE neuexpengine_entry_point neuexpengine project_warnings)
set_target_properties(dpi_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

add_executable(rendering_demo rendering/main.cpp)
if(WIN32)
    set_target_properties(rendering_demo PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
target_link_libraries(rendering_demo PRIVATE neuexpengine_entry_point neuexpengine project_warnings)
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
target_link_libraries(sprites_demo PRIVATE neuexpengine_entry_point neuexpengine project_warnings)
set_target_properties(sprites_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

add_executable(fonts_demo fonts/main.cpp)
if(WIN32)
    set_target_properties(fonts_demo PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
target_link_libraries(fonts_demo PRIVATE neuexpengine_entry_point neuexpengine project_warnings)
set_target_properties(fonts_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
add_dependencies(fonts_demo demo_assets)

if(MSVC)
    target_compile_options(fonts_demo PRIVATE /utf-8)
endif()

add_executable(gui_demo gui/main.cpp gui/menu.cpp gui/scene.cpp gui/settings.cpp gui/ui_theme.cpp)
if(WIN32)
    set_target_properties(gui_demo PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
target_link_libraries(gui_demo PRIVATE neuexpengine_entry_point neuexpengine project_warnings)
set_target_properties(gui_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
add_dependencies(gui_demo demo_assets)
if(MSVC)
    target_compile_options(gui_demo PRIVATE /utf-8)
endif()

add_executable(scene_demo scene/main.cpp scene/world.cpp)
if(WIN32)
    set_target_properties(scene_demo PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
target_link_libraries(scene_demo PRIVATE neuexpengine_entry_point neuexpengine project_warnings)
set_target_properties(scene_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

add_executable(collision_demo collision/main.cpp collision/world.cpp)
if(WIN32)
    set_target_properties(collision_demo PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
target_link_libraries(collision_demo PRIVATE neuexpengine_entry_point neuexpengine project_warnings)
set_target_properties(collision_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

# Инструмент проверки демо: гоняет зоны без окна и печатает, что обнаружено.
add_executable(collision_checks ../utils/collision/check_demo.cpp collision/world.cpp)
target_include_directories(collision_checks PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/collision")
target_link_libraries(collision_checks PRIVATE neuexpengine project_warnings)
set_target_properties(collision_checks PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

add_executable(nehopio
    nehopio/main.cpp
    nehopio/game.cpp
    nehopio/level.cpp
    nehopio/tiles.cpp
    nehopio/player.cpp
    nehopio/assets.cpp
    nehopio/run.cpp
    nehopio/audio.cpp
)
if(WIN32)
    set_target_properties(nehopio PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
target_include_directories(nehopio SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/third_party/miniaudio")
target_link_libraries(nehopio PRIVATE neuexpengine_entry_point neuexpengine miniaudio project_warnings)
set_target_properties(nehopio PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
add_dependencies(nehopio demo_assets)
if(MSVC)
    target_compile_options(nehopio PRIVATE /utf-8)
endif()
