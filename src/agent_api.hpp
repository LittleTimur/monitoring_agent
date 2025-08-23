#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <map>
#include <vector>
#include <unordered_map>
#include <filesystem>
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

// Структура для фоновых задач
struct BackgroundJobInfo {
    std::string job_id;
    std::atomic<bool> completed{false};
    std::atomic<bool> timed_out{false};
    std::atomic<bool> cancel_requested{false};
    std::atomic<int> exit_code{-1};
    std::string output;
    std::atomic<bool> truncated{false};
    int64_t duration_ms = 0;
    int64_t started_at_sec = 0;
    int64_t completed_at_sec = 0;
};

// Forward declaration
class AgentManager;

// Класс для HTTP сервера агента
class AgentHttpServer {
public:
    AgentHttpServer(const AgentConfig& config, AgentManager* manager);
    ~AgentHttpServer();
    
    // Запуск/остановка сервера
    void start();
    void stop();
    bool is_running() const { return running_; }
    
    // Обработчики команд
    using CommandHandler = std::function<CommandResponse(const Command&)>;
    void register_command_handler(const std::string& command, CommandHandler handler);
    
    // Обработка команд
    CommandResponse handle_command_request(const std::string& json_data);
    
private:
    AgentConfig config_;
    AgentManager* manager_;
    std::atomic<bool> running_{false};
    std::thread server_thread_;
    std::map<std::string, CommandHandler> command_handlers_;
    
    void server_loop();
    void handle_client_request(int client_socket);
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
    AgentManager(const AgentConfig& config, const std::string& config_path = "");
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
    CommandResponse handle_run_script(const Command& cmd);
    CommandResponse handle_get_job_output(const Command& cmd);
    CommandResponse handle_kill_job(const Command& cmd);
    CommandResponse handle_list_jobs(const Command& cmd);
    CommandResponse handle_push_script(const Command& cmd);
    CommandResponse handle_list_scripts(const Command& cmd);
    CommandResponse handle_delete_script(const Command& cmd);
    
    // Сбор метрик
    nlohmann::json collect_metrics(const std::vector<std::string>& requested_metrics = {});
    
    // Управление задачами
    std::string generate_job_id();
    void purge_old_jobs();
    
    // Дружественные классы
    friend class AgentHttpServer;
    
private:
    AgentConfig config_;
    std::string config_path_;
    std::atomic<bool> running_{false};
    std::unique_ptr<monitoring::MetricsCollector> metrics_collector_;
    std::unique_ptr<AgentHttpServer> http_server_;
    std::unique_ptr<MonitoringServerClient> server_client_;
    std::thread metrics_thread_;
    
    // Управление задачами
    std::unordered_map<std::string, std::shared_ptr<BackgroundJobInfo>> jobs_;
    mutable std::mutex jobs_mutex_;
    
    void metrics_loop();
    void initialize_metrics_collector();
};

// Структура для результатов выполнения процессов
struct ProcessResult {
    int exit_code = -1;
    std::string stdout_output;
    std::string stderr_output;
    std::string combined_output;
    bool timed_out = false;
    bool truncated = false;
};

// Функции для выполнения процессов (без значений по умолчанию в заголовке)
ProcessResult run_process_windows(const std::vector<std::string>& argv,
                                  const std::unordered_map<std::string, std::string>& env,
                                  const std::string& working_dir,
                                  int timeout_sec,
                                  int max_output_bytes,
                                  const std::function<bool()>& is_cancelled);

ProcessResult run_process_posix(const std::vector<std::string>& argv,
                                const std::unordered_map<std::string, std::string>& env,
                                const std::string& working_dir,
                                int timeout_sec,
                                int max_output_bytes,
                                const std::function<bool()>& is_cancelled);

} // namespace agent 