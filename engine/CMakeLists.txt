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
