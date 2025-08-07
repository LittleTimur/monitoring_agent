#include "agent_api.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <cpr/cpr.h>

#ifdef _WIN32
#include "windows_metrics_collector.hpp"
#endif

namespace agent {

// Command implementation
Command Command::from_json(const nlohmann::json& j) {
    Command cmd;
    if (j.contains("command")) cmd.command = j["command"];
    if (j.contains("data")) cmd.data = j["data"];
    if (j.contains("timestamp")) cmd.timestamp = j["timestamp"];
    return cmd;
}

nlohmann::json Command::to_json() const {
    nlohmann::json j;
    j["command"] = command;
    j["data"] = data;
    j["timestamp"] = timestamp;
    return j;
}

// CommandResponse implementation
nlohmann::json CommandResponse::to_json() const {
    nlohmann::json j;
    j["success"] = success;
    j["message"] = message;
    j["data"] = data;
    j["timestamp"] = timestamp;
    return j;
}

// AgentHttpServer implementation
AgentHttpServer::AgentHttpServer(const AgentConfig& config) : config_(config) {
    // Регистрируем стандартные обработчики команд
    register_command_handler("collect_metrics", [this](const Command& cmd) {
        return CommandResponse{true, "Metrics collection requested", {}, ""};
    });
    
    register_command_handler("update_config", [this](const Command& cmd) {
        return CommandResponse{true, "Configuration update requested", {}, ""};
    });
    
    register_command_handler("restart", [this](const Command& cmd) {
        return CommandResponse{true, "Restart requested", {}, ""};
    });
    
    register_command_handler("stop", [this](const Command& cmd) {
        return CommandResponse{true, "Stop requested", {}, ""};
    });
}

AgentHttpServer::~AgentHttpServer() {
    stop();
}

void AgentHttpServer::start() {
    if (running_) return;
    
    running_ = true;
    server_thread_ = std::thread(&AgentHttpServer::server_loop, this);
    std::cout << "Agent HTTP server started on port " << config_.command_server_port << std::endl;
}

void AgentHttpServer::stop() {
    if (!running_) return;
    
    running_ = false;
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    std::cout << "Agent HTTP server stopped" << std::endl;
}

void AgentHttpServer::register_command_handler(const std::string& command, CommandHandler handler) {
    command_handlers_[command] = handler;
}

void AgentHttpServer::server_loop() {
    // Простая реализация HTTP сервера через TCP сокеты
    // В реальной реализации здесь будет полноценный HTTP сервер
    
    std::cout << "HTTP server loop started (simplified implementation)" << std::endl;
    
    // Создаем простой TCP сервер для приема команд
    // Пока просто выводим сообщение о готовности
    std::cout << "Agent HTTP server ready on port " << config_.command_server_port << std::endl;
    std::cout << "Waiting for commands from server..." << std::endl;
    
    while (running_) {
        // Здесь будет код для приема HTTP запросов
        // Пока просто ждем
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Симуляция обработки команд (для тестирования)
        // static int counter = 0;
        // if (counter++ % 100 == 0) {  // Каждые 10 секунд
        //     std::cout << "Agent HTTP server heartbeat..." << std::endl;
        // }
    }
}

void AgentHttpServer::handle_request(const std::string& request, std::string& response) {
    try {
        // Парсим JSON запрос
        nlohmann::json request_json = nlohmann::json::parse(request);
        Command cmd = Command::from_json(request_json);
        
        // Ищем обработчик
        auto it = command_handlers_.find(cmd.command);
        if (it != command_handlers_.end()) {
            CommandResponse resp = it->second(cmd);
            response = resp.to_json().dump();
        } else {
            CommandResponse resp{false, "Unknown command: " + cmd.command, {}, ""};
            response = resp.to_json().dump();
        }
    } catch (const std::exception& e) {
        CommandResponse resp{false, "Error parsing request: " + std::string(e.what()), {}, ""};
        response = resp.to_json().dump();
    }
}

std::string AgentHttpServer::generate_response(int status_code, const std::string& content_type, const std::string& body) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_code << " OK\r\n";
    oss << "Content-Type: " << content_type << "\r\n";
    oss << "Content-Length: " << body.length() << "\r\n";
    oss << "\r\n";
    oss << body;
    return oss.str();
}

// MonitoringServerClient implementation
MonitoringServerClient::MonitoringServerClient(const AgentConfig& config) : config_(config) {
    // Автоматически определяем ID и имя, если не заданы
    config_.auto_detect_agent_info();
    
    agent_id_ = config_.agent_id;
    machine_name_ = config_.machine_name;
    
    std::cout << "Agent initialized with ID: " << agent_id_ << std::endl;
    std::cout << "Machine name: " << machine_name_ << std::endl;
}

bool MonitoringServerClient::send_metrics(const nlohmann::json& metrics) {
    try {
        // Добавляем информацию об агенте
        nlohmann::json data = metrics;
        data["agent_id"] = agent_id_;
        data["machine_name"] = machine_name_;
        
        nlohmann::json response;
        return make_request("/metrics", data, response);
    } catch (const std::exception& e) {
        std::cerr << "Error sending metrics: " << e.what() << std::endl;
        return false;
    }
}

bool MonitoringServerClient::register_agent() {
    try {
        nlohmann::json data;
        data["agent_id"] = agent_id_;
        data["machine_name"] = machine_name_;
        data["machine_type"] = "Windows"; // TODO: определить автоматически
        data["ip_address"] = "127.0.0.1"; // TODO: получить реальный IP
        
        nlohmann::json response;
        bool success = make_request("/api/agents/" + agent_id_ + "/register", data, response);
        
        if (success) {
            std::cout << "Agent registered successfully on server" << std::endl;
        } else {
            std::cout << "Agent registration failed" << std::endl;
        }
        
        return success;
    } catch (const std::exception& e) {
        std::cerr << "Error registering agent: " << e.what() << std::endl;
        return false;
    }
}

bool MonitoringServerClient::update_config_from_server() {
    try {
        nlohmann::json response;
        if (make_request("/api/agents/" + agent_id_ + "/config", {}, response)) {
            // Обновляем конфигурацию
            config_.update_from_json(response);
            std::cout << "Configuration updated from server" << std::endl;
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Error updating config from server: " << e.what() << std::endl;
        return false;
    }
}

bool MonitoringServerClient::make_request(const std::string& endpoint, const nlohmann::json& data, nlohmann::json& response) {
    try {
        std::string url = config_.server_url;
        if (endpoint != "/metrics") {
            // Для всех запросов кроме метрик используем базовый URL сервера
            url = config_.server_url.substr(0, config_.server_url.find("/metrics"));
        }
        url += endpoint;
        
        auto cpr_response = cpr::Post(
            cpr::Url{url},
            cpr::Header{{"Content-Type", "application/json"}},
            cpr::Body{data.dump()},
            cpr::Timeout{config_.send_timeout_ms}
        );
        
        if (cpr_response.status_code == 200) {
            if (!cpr_response.text.empty()) {
                response = nlohmann::json::parse(cpr_response.text);
            }
            return true;
        } else {
            std::cerr << "HTTP request failed: " << cpr_response.status_code << " - " << cpr_response.text << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error making HTTP request: " << e.what() << std::endl;
        return false;
    }
}

// AgentManager implementation
AgentManager::AgentManager(const AgentConfig& config) : config_(config) {
    initialize_metrics_collector();
    
    http_server_ = std::make_unique<AgentHttpServer>(config_);
    server_client_ = std::make_unique<MonitoringServerClient>(config_);
}

AgentManager::~AgentManager() {
    stop();
}

void AgentManager::start() {
    if (running_) return;
    
    running_ = true;
    
    // Запускаем HTTP сервер
    http_server_->start();
    
    // Регистрируемся на сервере
    server_client_->register_agent();
    
    // Запускаем потоки
    metrics_thread_ = std::thread(&AgentManager::metrics_loop, this);
    
    std::cout << "Agent manager started" << std::endl;
}

void AgentManager::stop() {
    if (!running_) return;
    
    running_ = false;
    
    // Останавливаем HTTP сервер
    if (http_server_) {
        http_server_->stop();
    }
    
    // Ждем завершения потоков
    if (metrics_thread_.joinable()) {
        metrics_thread_.join();
    }
    
    std::cout << "Agent manager stopped" << std::endl;
}

CommandResponse AgentManager::handle_collect_metrics(const Command& cmd) {
    try {
        std::vector<std::string> requested_metrics;
        if (cmd.data.contains("metrics")) {
            // Поддерживаем как старый формат (массив строк), так и новый (объект с флагами)
            if (cmd.data["metrics"].is_array()) {
                requested_metrics = cmd.data["metrics"].get<std::vector<std::string>>();
            } else if (cmd.data["metrics"].is_object()) {
                // Новый формат: объект с флагами
                for (auto it = cmd.data["metrics"].begin(); it != cmd.data["metrics"].end(); ++it) {
                    if (it.value().get<bool>()) {
                        requested_metrics.push_back(it.key());
                    }
                }
            }
        }
        
        auto metrics = collect_metrics(requested_metrics);
        server_client_->send_metrics(metrics);
        
        return CommandResponse{true, "Metrics collected and sent", metrics, ""};
    } catch (const std::exception& e) {
        return CommandResponse{false, "Error collecting metrics: " + std::string(e.what()), {}, ""};
    }
}

CommandResponse AgentManager::handle_update_config(const Command& cmd) {
    try {
        config_.update_from_json(cmd.data);
        config_.save_to_file();
        
        std::cout << "Configuration updated from server command" << std::endl;
        
        return CommandResponse{true, "Configuration updated", config_.to_json(), ""};
    } catch (const std::exception& e) {
        return CommandResponse{false, "Error updating config: " + std::string(e.what()), {}, ""};
    }
}

CommandResponse AgentManager::handle_restart(const Command& cmd) {
    // В реальной реализации здесь будет перезапуск агента
    return CommandResponse{true, "Restart command received", {}, ""};
}

CommandResponse AgentManager::handle_stop(const Command& cmd) {
    // Останавливаем агент
    stop();
    return CommandResponse{true, "Stop command received", {}, ""};
}

nlohmann::json AgentManager::collect_metrics(const std::vector<std::string>& requested_metrics) {
    if (!metrics_collector_) {
        throw std::runtime_error("Metrics collector not initialized");
    }
    
    auto metrics = metrics_collector_->collect();
    
    // Определяем, какие метрики собирать
    std::vector<std::string> enabled_metrics = requested_metrics.empty() ? 
        config_.get_enabled_metrics_list() : requested_metrics;
    
    // Конвертируем в JSON
    nlohmann::json j;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(metrics.timestamp.time_since_epoch()).count();
    j["machine_type"] = metrics.machine_type;
    j["agent_id"] = config_.agent_id;
    j["machine_name"] = config_.machine_name;
    
    // Добавляем метрики согласно enabled_metrics
    for (const auto& metric_type : enabled_metrics) {
        if (metric_type == "cpu" && config_.is_metric_enabled("cpu")) {
            j["cpu"]["usage_percent"] = metrics.cpu.usage_percent;
            j["cpu"]["temperature"] = metrics.cpu.temperature;
            j["cpu"]["core_temperatures"] = metrics.cpu.core_temperatures;
            j["cpu"]["core_usage"] = metrics.cpu.core_usage;
        } else if (metric_type == "memory" && config_.is_metric_enabled("memory")) {
            j["memory"]["total_bytes"] = metrics.memory.total_bytes;
            j["memory"]["used_bytes"] = metrics.memory.used_bytes;
            j["memory"]["free_bytes"] = metrics.memory.free_bytes;
            j["memory"]["usage_percent"] = metrics.memory.usage_percent;
        } else if (metric_type == "disk" && config_.is_metric_enabled("disk")) {
            // TODO: Добавить обработку дисков
            j["disk"]["partitions"] = nlohmann::json::array();
        } else if (metric_type == "network" && config_.is_metric_enabled("network")) {
            // TODO: Добавить обработку сети
            j["network"]["interfaces"] = nlohmann::json::array();
        } else if (metric_type == "gpu" && config_.is_metric_enabled("gpu")) {
            j["gpu"]["temperature"] = metrics.gpu.temperature;
            j["gpu"]["usage_percent"] = metrics.gpu.usage_percent;
            j["gpu"]["memory_used"] = metrics.gpu.memory_used;
            j["gpu"]["memory_total"] = metrics.gpu.memory_total;
        } else if (metric_type == "hdd" && config_.is_metric_enabled("hdd")) {
            // TODO: Добавить обработку HDD
            j["hdd"]["drives"] = nlohmann::json::array();
        } else if (metric_type == "inventory" && config_.is_metric_enabled("inventory")) {
            j["inventory"]["device_type"] = metrics.inventory.device_type;
            j["inventory"]["manufacturer"] = metrics.inventory.manufacturer;
            j["inventory"]["model"] = metrics.inventory.model;
            j["inventory"]["serial_number"] = metrics.inventory.serial_number;
            j["inventory"]["uuid"] = metrics.inventory.uuid;
            j["inventory"]["os_name"] = metrics.inventory.os_name;
            j["inventory"]["os_version"] = metrics.inventory.os_version;
            j["inventory"]["cpu_model"] = metrics.inventory.cpu_model;
            j["inventory"]["cpu_frequency"] = metrics.inventory.cpu_frequency;
            j["inventory"]["memory_type"] = metrics.inventory.memory_type;
            j["inventory"]["disk_model"] = metrics.inventory.disk_model;
            j["inventory"]["disk_type"] = metrics.inventory.disk_type;
            j["inventory"]["disk_total_bytes"] = metrics.inventory.disk_total_bytes;
            j["inventory"]["gpu_model"] = metrics.inventory.gpu_model;
            j["inventory"]["mac_addresses"] = metrics.inventory.mac_addresses;
            j["inventory"]["ip_addresses"] = metrics.inventory.ip_addresses;
            j["inventory"]["installed_software"] = metrics.inventory.installed_software;
        }
    }
    
    return j;
}

void AgentManager::metrics_loop() {
    while (running_) {
        try {
            auto metrics = collect_metrics();
            server_client_->send_metrics(metrics);
        } catch (const std::exception& e) {
            std::cerr << "Error in metrics loop: " << e.what() << std::endl;
        }
        
        // Используем heartbeat_interval_seconds как интервал сбора метрик
        std::this_thread::sleep_for(std::chrono::seconds(config_.update_frequency));
    }
}

void AgentManager::initialize_metrics_collector() {
#ifdef _WIN32
    metrics_collector_ = std::make_unique<monitoring::WindowsMetricsCollector>();
#else
    metrics_collector_ = monitoring::create_metrics_collector();
#endif
}

} // namespace agent 