# Требуем CMake версии 3.16 или новее.
# Кроме проверки версии, эта команда выбирает набор правил совместимости
# (policies), соответствующий указанной версии CMake.
cmake_minimum_required(VERSION 3.16)

# Имя проекта — OpenGLDemo.
# LANGUAGES CXX говорит CMake, что проект использует C++.
# CXX — принятое в CMake обозначение языка C++.
# При выполнении project() CMake определит доступный компилятор C++
# и проверит, что им можно собирать C++-код.
project(OpenGLDemo LANGUAGES CXX)

# Просим использовать стандарт C++17 для C++-исходников проекта.
# Это значение используется как стандартное для создаваемых целей сборки;
# отдельная цель сборки при необходимости может потребовать другой стандарт.
set(CMAKE_CXX_STANDARD 17)

# Требуем именно запрошенный стандарт.
# Если компилятор не поддерживает C++17, конфигурация должна завершиться
# ошибкой, а не перейти на более старый доступный стандарт.
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Не включаем расширенный диалект компилятора поверх C++17.
# Например, для GCC это означает предпочтение -std=c++17 вместо -std=gnu++17.
# Это не является полной проверкой исходного кода на соответствие стандарту;
# за дополнительную диагностику отвечают предупреждения компилятора ниже.
set(CMAKE_CXX_EXTENSIONS OFF)

# Демо и тесты включаются независимо друг от друга.
option(BUILD_DEMOS "Build demonstration applications" ON)
# include(CTest) создаёт BUILD_TESTING (по умолчанию ON) и включает регистрацию тестов.
include(CTest)

# Сторонние реализации собираются отдельно от кода приложения.
# Используются только локальные исходники; сетевых загрузок при сборке нет.
# EXCLUDE_FROM_ALL исключает сборку библиотеки, если ни одна выбранная цель её не использует.
add_library(stb_image STATIC EXCLUDE_FROM_ALL third_party/stb/stb_image_impl.cpp)
add_library(miniaudio STATIC EXCLUDE_FROM_ALL third_party/miniaudio/miniaudio_impl.cpp)

# Пути доступны только самим библиотекам, а не публичным интерфейсам движка.
target_include_directories(stb_image SYSTEM PRIVATE third_party/stb)
target_include_directories(miniaudio SYSTEM PRIVATE third_party/miniaudio)

if(UNIX AND NOT APPLE)
    # miniaudio использует потоки, динамическую загрузку аудиобэкендов и математику.
    set(THREADS_PREFER_PTHREAD_FLAG TRUE)
    find_package(Threads REQUIRED)
    target_link_libraries(miniaudio PRIVATE Threads::Threads ${CMAKE_DL_LIBS} m)
endif()

# INTERFACE-цель передаёт предупреждения только тем целям, которые её подключают.
# Сторонние библиотеки собираются со своими настройками.
add_library(project_warnings INTERFACE)
if(MSVC)
    target_compile_options(project_warnings INTERFACE /W4)
else()
    # -Wconversion проверяет неявные числовые преобразования;
    # -Wsign-conversion — преобразования между знаковыми и беззнаковыми типами.
    target_compile_options(project_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
endif()

add_subdirectory(engine)

# Платформенная библиотека нужна демо и платформенным тестам.
# При отключении обеих групп поиск оконных зависимостей не выполняется.
if(BUILD_DEMOS OR BUILD_TESTING)
    add_subdirectory(platform)
endif()

# add_subdirectory читает настройки целей из соответствующего CMakeLists.txt.
if(BUILD_DEMOS)
    add_subdirectory(demos)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
