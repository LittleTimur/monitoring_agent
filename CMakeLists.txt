cmake_minimum_required(VERSION 3.10)
project(monitoring_agent)

set(CMAKE_CXX_STANDARD 17)

# Зависимости
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

# Исходники
file(GLOB SOURCES "src/*.cpp" "src/metrics/*.cpp" "src/transport/*.cpp")
add_executable(agent ${SOURCES})

# Подключаем библиотеки
target_include_directories(agent PRIVATE include)
target_link_libraries(agent PRIVATE 
    CURL::libcurl 
    nlohmann_json::nlohmann_json
)