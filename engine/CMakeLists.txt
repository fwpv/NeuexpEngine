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

if(BUILD_DEMOS OR BUILD_TESTING)
    add_library(engine_application STATIC application.cpp)
    target_include_directories(engine_application PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(engine_application PUBLIC platform engine_time PRIVATE project_warnings)
endif()
