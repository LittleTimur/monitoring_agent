#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <nlohmann/json.hpp>

namespace agent {

struct AgentConfig {
    // Основные настройки
    std::string agent_id;
    std::string machine_name;
    std::string server_url = "http://localhost:8000";
    std::string command_server_url = "http://localhost:8081";
    
    // Настройки сбора метрик - теперь словарь с флагами
    std::map<std::string, bool> enabled_metrics = {
        {"cpu", true},
        {"memory", true},
        {"disk", true},
        {"network", true},
        {"gpu", false},
        {"hdd", false},
        {"inventory", true}
    };
    
    // Настройки HTTP сервера агента
    int command_server_port = 8081;
    std::string command_server_host = "0.0.0.0";
    
    // Настройки отправки
    int send_timeout_ms = 2000;
    int max_buffer_size = 10;
    int update_frequency = 60; // Metrics collection interval in seconds
    
    // Настройки автоматического определения
    bool auto_detect_id = true;
    bool auto_detect_name = true;
    
    // Методы для работы с JSON
    nlohmann::json to_json() const;
    static AgentConfig from_json(const nlohmann::json& j);
    
    // Загрузка/сохранение конфигурации
    static AgentConfig load_from_file(const std::string& filename = "agent_config.json");
    void save_to_file(const std::string& filename = "agent_config.json") const;
    
    // Обновление конфигурации
    void update_from_json(const nlohmann::json& j);
    
    // Автоматическое определение ID и имени
    void auto_detect_agent_info();
    std::string generate_agent_id();
    std::string get_machine_name();
    
    // Работа с метриками
    bool is_metric_enabled(const std::string& metric_name) const;
    void set_metric_enabled(const std::string& metric_name, bool enabled);
    std::vector<std::string> get_enabled_metrics_list() const;
};

} // namespace agent 