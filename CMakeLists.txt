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
)

if(WIN32)
    list(APPEND SOURCES src/windows_metrics_collector.cpp)
else()
    list(APPEND SOURCES src/linux_metrics_collector.cpp)
endif()

# Создаем исполняемый файл
add_executable(${PROJECT_NAME} ${SOURCES})

# Для Windows: собирать как GUI-приложение без консольного окна
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# Устанавливаем выходную директорию
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
)

# Добавляем cpr через FetchContent
include(FetchContent)
FetchContent_Declare(
  cpr
  GIT_REPOSITORY https://github.com/libcpr/cpr.git
  GIT_TAG 1.10.5
)
FetchContent_MakeAvailable(cpr)

# Линкуем необходимые библиотеки для Windows и Linux
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        cpr::cpr
        pdh
        iphlpapi
        ws2_32
    )
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE cpr::cpr)
endif()

# Автоматическое копирование DLL для Windows (после FetchContent)
if(WIN32)
    function(copy_dependency_dll_to_output main_target dependency_target)
        add_custom_command(TARGET ${main_target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:${dependency_target}>"
                "$<TARGET_FILE_DIR:${main_target}>"
            COMMENT "Copying ${dependency_target} DLL to output directory"
        )
    endfunction()

    copy_dependency_dll_to_output(${PROJECT_NAME} cpr::cpr)
    copy_dependency_dll_to_output(${PROJECT_NAME} CURL::libcurl_shared)
    copy_dependency_dll_to_output(${PROJECT_NAME} zlib)
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
if(WIN32)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
endif()