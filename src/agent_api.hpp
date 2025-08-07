#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <functional>
#include <nlohmann/json.hpp>
#include "agent_config.hpp"
#include "../include/metrics_collector.hpp"

namespace agent {

// Структуры для команд
struct Command {
    std::string command;
    nlohmann::json data;
    std::string timestamp;
    
    static Command from_json(const nlohmann::json& j);
    nlohmann::json to_json() const;
};

struct CommandResponse {
    bool success;
    std::string message;
    nlohmann::json data;
    std::string timestamp;
    
    nlohmann::json to_json() const;
};

// Класс для HTTP сервера агента
class AgentHttpServer {
public:
    AgentHttpServer(const AgentConfig& config);
    ~AgentHttpServer();
    
    // Запуск/остановка сервера
    void start();
    void stop();
    bool is_running() const { return running_; }
    
    // Обработчики команд
    using CommandHandler = std::function<CommandResponse(const Command&)>;
    void register_command_handler(const std::string& command, CommandHandler handler);
    
private:
    AgentConfig config_;
    std::atomic<bool> running_{false};
    std::thread server_thread_;
    std::map<std::string, CommandHandler> command_handlers_;
    
    void server_loop();
    void handle_request(const std::string& request, std::string& response);
    std::string generate_response(int status_code, const std::string& content_type, const std::string& body);
};

// Класс для взаимодействия с сервером мониторинга
class MonitoringServerClient {
public:
    MonitoringServerClient(const AgentConfig& config);
    
    // Отправка метрик
    bool send_metrics(const nlohmann::json& metrics);
    
    // Регистрация агента
    bool register_agent();
    
    // Получение конфигурации с сервера
    bool update_config_from_server();
    
private:
    AgentConfig config_;
    std::string agent_id_;
    std::string machine_name_;
    
    bool make_request(const std::string& endpoint, const nlohmann::json& data, nlohmann::json& response);
};

// Класс для управления агентом
class AgentManager {
public:
    AgentManager(const AgentConfig& config);
    ~AgentManager();
    
    // Основные методы
    void start();
    void stop();
    bool is_running() const { return running_; }
    
    // Обработка команд
    CommandResponse handle_collect_metrics(const Command& cmd);
    CommandResponse handle_update_config(const Command& cmd);
    CommandResponse handle_restart(const Command& cmd);
    CommandResponse handle_stop(const Command& cmd);
    
    // Сбор метрик
    nlohmann::json collect_metrics(const std::vector<std::string>& requested_metrics = {});
    
private:
    AgentConfig config_;
    std::atomic<bool> running_{false};
    std::unique_ptr<monitoring::MetricsCollector> metrics_collector_;
    std::unique_ptr<AgentHttpServer> http_server_;
    std::unique_ptr<MonitoringServerClient> server_client_;
    std::thread metrics_thread_;
    
    void metrics_loop();
    void initialize_metrics_collector();
};

} // namespace agent 