cmake_minimum_required(VERSION 3.15)
project(monitoring_agent VERSION 1.0.0 LANGUAGES CXX)

# Устанавливаем стандарт C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Опции сборки
option(BUILD_TESTS "Build tests" OFF)

# Добавляем директорию с заголовочными файлами
include_directories(${PROJECT_SOURCE_DIR}/include)

# Список исходных файлов
set(SOURCES
    src/main.cpp
    src/windows_metrics_collector.cpp
)

# Создаем исполняемый файл
add_executable(${PROJECT_NAME} ${SOURCES})

# Устанавливаем выходную директорию
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
)

# Линкуем необходимые библиотеки для Windows
if(WIN32)
    target_link_libraries(${PROJECT_NAME} 
        pdh
        iphlpapi
        ws2_32
    )
endif()

# Если включены тесты, добавляем их
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Устанавливаем флаги компиляции для Windows
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _WIN32_WINNT=0x0601  # Windows 7 или выше
        NOMINMAX             # Отключаем макросы min/max
        WIN32_LEAN_AND_MEAN  # Уменьшаем размер заголовочных файлов Windows
    )
endif()

# Устанавливаем флаги компиляции для Release
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")

# Копируем необходимые DLL в выходную директорию
if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:${PROJECT_NAME}>
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
endif()