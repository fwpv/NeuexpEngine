# Выбираем один платформенный исходник и набор библиотек ОС.
# Остальной код использует один интерфейс Platform независимо от того,
# какая платформенная реализация выбрана здесь.
if(WIN32)
    # WIN32 — встроенная переменная CMake, истинная при сборке для Windows.
    # CMake определяет её во время конфигурации проекта.
    set(PLATFORM_SOURCE "${PROJECT_SOURCE_DIR}/platform/platform_windows.cpp")

    # Это имена системных библиотек Windows, которые потребуются линкеру.
    #
    # opengl32 — Windows OpenGL API и WGL;
    # gdi32    — необходимые функции GDI, в частности работа с HDC;
    # user32   — окна, сообщения и другие функции Win32;
    # winmm    — функции Windows Multimedia, используемые здесь для таймера.
    #
    # В отличие от find_package() ниже, здесь CMake не ищет эти библиотеки
    # как сторонние пакеты. Их имена передаются инструментам сборки и компоновщику Windows,
    # который знает стандартные системные библиотеки.
    set(PLATFORM_LIBRARIES opengl32 gdi32 user32 winmm)

elseif(UNIX AND NOT APPLE)
    # Ветка для Unix-подобных систем, кроме macOS.
    # В данном примере платформенным оконным API является X11.

    # Ищем установленный пакет X11.
    # REQUIRED означает, что отсутствие необходимой X11-инфраструктуры
    # является ошибкой конфигурации.
    find_package(X11 REQUIRED)

    # Ищем установленную OpenGL development-инфраструктуру.
    # REQUIRED означает, что без неё конфигурация также завершается ошибкой.
    #
    # Это не означает, что CMake выбирает конкретную версию OpenGL API,
    # например OpenGL 3.3 или 4.6.
    find_package(OpenGL REQUIRED)

    # В Linux используем реализацию Platform для X11.
    set(PLATFORM_SOURCE "${PROJECT_SOURCE_DIR}/platform/platform_x11.cpp")

    # X11::X11 и OpenGL::GL — импортированные цели CMake,
    # созданные найденными пакетами.
    #
    # Такая цель сборки содержит информацию, необходимую CMake для использования
    # библиотеки: например, какую библиотеку линковать и какие пути к заголовочным файлам
    # и зависимости могут потребоваться.
    set(PLATFORM_LIBRARIES X11::X11 OpenGL::GL)

else()
    # Неизвестная платформа не получает случайно неподходящую реализацию.
    message(FATAL_ERROR "This demo currently supports Windows and Linux/X11.")
endif()

if(WIN32)
    # WIN32 у add_executable() выбирает графическую подсистему Windows,
    # поэтому при запуске приложения Windows не создаёт ему автоматически
    # консольное окно.
    #
    # Исполняемый файл circle собирается из точки входа, игровой логики
    # и выбранной реализации Platform.
    add_executable(circle WIN32
        circle/main.cpp
        circle/game.cpp
        ${PLATFORM_SOURCE}
    )

else()
    # В Linux нет отдельного аналога Windows GUI/Console subsystem.
    # Обычная Linux-программа просто использует stdout/stderr того терминала,
    # из которого она была запущена.
    #
    # Если программа запущена из графического интерфейса без терминала,
    # терминал автоматически не создаётся.
    #
    # Используется та же точка входа и игровая логика, но X11-платформенный
    # исходник.
    add_executable(circle
        circle/main.cpp
        circle/game.cpp
        ${PLATFORM_SOURCE}
    )
endif()

# Связываем цель сборки circle с необходимыми платформенными библиотеками.
#
# add_executable() сообщает CMake, что мы собираем и из каких исходников.
# target_link_libraries() отдельно сообщает, с чем этот исполняемый файл
# должен быть связан на этапе линковки.
#
# PRIVATE означает, что эти библиотеки являются зависимостями самого circle
# и не должны автоматически передаваться целям сборки, которые впоследствии
# будут зависеть от circle.
target_link_libraries(circle PRIVATE ${PLATFORM_LIBRARIES} stb_image miniaudio)

# Общие заголовки движка доступны демо и платформенным реализациям.
# PRIVATE ограничивает путь включения исходниками цели circle.
target_include_directories(circle PRIVATE "${PROJECT_SOURCE_DIR}/engine")

if(MSVC)
    # MSVC — встроенная CMake-переменная, истинная, если используется
    # компилятор Microsoft Visual C++.
    #
    # /W4 передаётся MSVC как опция компиляции цели сборки circle
    # и включает высокий уровень предупреждений.
    target_compile_options(circle PRIVATE /W4)

else()
    # Для GCC/Clang:
    #
    # -Wall      — набор распространённых предупреждений;
    # -Wextra    — дополнительные предупреждения;
    # -Wpedantic — предупреждения о некоторых конструкциях,
    #              выходящих за рамки выбранного стандарта.
    #
    # -Wpedantic не заменяет CMAKE_CXX_EXTENSIONS OFF:
    # первое управляет диагностикой, второе — выбором диалекта компилятора.
    target_compile_options(circle PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Сохраняем путь build/circle (build/circle.exe в Windows) для существующих команд запуска.
# Без этого свойства add_subdirectory разместил бы программу в build/demos/.
set_target_properties(circle PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
