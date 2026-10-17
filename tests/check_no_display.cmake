# Запускаем настоящий путь входа демо отдельным процессом с недоступным дисплеем.
# Окружение родительского процесса не меняется; тайм-аут защищает от зависания.
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "DISPLAY=" "${APPLICATION}"
    RESULT_VARIABLE exitCode
    OUTPUT_VARIABLE standardOutput
    ERROR_VARIABLE standardError
    TIMEOUT 10
)

# Проверяем именно штатный отказ, а не падение или ошибку запуска процесса.
if(NOT "${exitCode}" STREQUAL "1")
    message(FATAL_ERROR "Expected exit code 1, got: ${exitCode}\nstdout: ${standardOutput}\nstderr: ${standardError}")
endif()

# Системный текст может зависеть от окружения; проверяем только идентификатор операции.
string(FIND "${standardError}" "XOpenDisplay" operationPosition)
if(operationPosition EQUAL -1)
    message(FATAL_ERROR "Expected an XOpenDisplay error in stderr, got: ${standardError}")
endif()
