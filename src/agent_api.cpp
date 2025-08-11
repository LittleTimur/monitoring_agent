#include "agent_api.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <cpr/cpr.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include "windows_metrics_collector.hpp"
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
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
AgentHttpServer::AgentHttpServer(const AgentConfig& config, AgentManager* manager) : config_(config), manager_(manager) {
    // Регистрируем настоящие обработчики команд
    register_command_handler("collect_metrics", [this](const Command& cmd) {
        return manager_->handle_collect_metrics(cmd);
    });
    register_command_handler("update_config", [this](const Command& cmd) {
        return manager_->handle_update_config(cmd);
    });
    register_command_handler("restart", [this](const Command& cmd) {
        return manager_->handle_restart(cmd);
    });
    register_command_handler("stop", [this](const Command& cmd) {
        return manager_->handle_stop(cmd);
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
    std::cout << "HTTP server loop started" << std::endl;
    
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return;
    }
#endif

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return;
    }

    // Устанавливаем опцию переиспользования адреса
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(config_.command_server_port);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Bind failed" << std::endl;
        return;
    }

    if (listen(server_socket, 5) < 0) {
        std::cerr << "Listen failed" << std::endl;
        return;
    }

    std::cout << "Agent HTTP server ready on port " << config_.command_server_port << std::endl;
    std::cout << "Waiting for commands from server..." << std::endl;

    while (running_) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            if (running_) {
                std::cerr << "Accept failed" << std::endl;
            }
            continue;
        }

        // Обрабатываем запрос в отдельном потоке или синхронно
        std::thread([this, client_socket]() {
            this->handle_client_request(client_socket);
        }).detach();
    }

#ifdef _WIN32
    closesocket(server_socket);
    WSACleanup();
#else
    close(server_socket);
#endif
}

void AgentHttpServer::handle_client_request(int client_socket) {
    char buffer[4096];
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        
        // Парсим HTTP запрос
        std::string request(buffer);
        std::string response;
        
        std::cout << "[RECV] Received HTTP request:" << std::endl;
        std::cout << "   " << request.substr(0, request.find('\n')) << std::endl;
        
        if (request.find("POST /command") != std::string::npos) {
            // Извлекаем JSON из тела запроса
            size_t json_start = request.find("\r\n\r\n");
            if (json_start != std::string::npos) {
                std::string json_data = request.substr(json_start + 4);
                std::cout << "[JSON] JSON data: " << json_data << std::endl;
                
                // Обрабатываем команду
                CommandResponse cmd_response = handle_command_request(json_data);
                response = generate_response(200, "application/json", cmd_response.to_json().dump());
            } else {
                response = generate_response(400, "application/json", 
                    "{\"success\": false, \"message\": \"No JSON data found\"}");
            }
        } else {
            response = generate_response(404, "application/json", 
                "{\"success\": false, \"message\": \"Endpoint not found\"}");
        }
        
        send(client_socket, response.c_str(), response.length(), 0);
    }
    
#ifdef _WIN32
    closesocket(client_socket);
#else
    close(client_socket);
#endif
}

CommandResponse AgentHttpServer::handle_command_request(const std::string& json_data) {
    try {
        std::cout << "[CMD] Processing command..." << std::endl;
        
        // Парсим JSON запрос
        nlohmann::json request_json = nlohmann::json::parse(json_data);
        Command cmd = Command::from_json(request_json);
        
        std::cout << "[CMD] Command: " << cmd.command << std::endl;
        std::cout << "[CMD] Data: " << cmd.data.dump() << std::endl;
        
        // Ищем обработчик
        auto it = command_handlers_.find(cmd.command);
        if (it != command_handlers_.end()) {
            std::cout << "[CMD] Found handler for command: " << cmd.command << std::endl;
            CommandResponse resp = it->second(cmd);
            std::cout << "[RESP] Response: " << resp.to_json().dump() << std::endl;
            return resp;
        } else {
            std::cout << "[ERROR] Unknown command: " << cmd.command << std::endl;
            return CommandResponse{false, "Unknown command: " + cmd.command, {}, ""};
        }
    } catch (const std::exception& e) {
        std::cout << "[ERROR] Request parsing error: " << e.what() << std::endl;
        return CommandResponse{false, "Error parsing request: " + std::string(e.what()), {}, ""};
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
    
    std::cout << "[INIT] Agent initialized with ID: " << agent_id_ << std::endl;
    std::cout << "[INIT] Machine name: " << machine_name_ << std::endl;
    std::cout << "[INIT] Server URL: " << config_.server_url << std::endl;
    std::cout << "[INIT] Command server port: " << config_.command_server_port << std::endl;
}

bool MonitoringServerClient::send_metrics(const nlohmann::json& metrics) {
    try {
        // Добавляем информацию об агенте
        nlohmann::json data = metrics;
        data["agent_id"] = agent_id_;
        data["machine_name"] = machine_name_;
        
        // Проверяем корректность JSON перед отправкой
        std::string json_str = data.dump();
        std::cout << "[SEND] Sending metrics to server:" << std::endl;
        std::cout << "   JSON size: " << json_str.length() << " bytes" << std::endl;
        std::cout << "   First 100 chars: " << json_str.substr(0, 100) << std::endl;
        
        nlohmann::json response;
        bool success = make_request("/metrics", data, response);
        
        if (success) {
            std::cout << "[SUCCESS] Metrics sent successfully" << std::endl;
        } else {
            std::cout << "[ERROR] Failed to send metrics" << std::endl;
        }
        
        return success;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Error sending metrics: " << e.what() << std::endl;
        return false;
    }
}

bool MonitoringServerClient::register_agent() {
    try {
        // Agent registration happens automatically when sending metrics
        // The server creates agent records when receiving metrics
        std::cout << "[REG] Agent registration not required - will register automatically with first metrics" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Agent registration error: " << e.what() << std::endl;
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
        std::cout << "[DEBUG] config_.server_url = " << config_.server_url << std::endl;
        std::cout << "[DEBUG] endpoint = " << endpoint << std::endl;
        
        std::string url;
        // Всегда используем базовый URL + endpoint
        url = config_.server_url + endpoint;
        std::cout << "[DEBUG] Using base URL + endpoint: " << url << std::endl;
        
        // Проверяем корректность JSON данных
        std::string json_body;
        try {
            json_body = data.dump();
                    std::cout << "[HTTP] Sending request to: " << url << std::endl;
        std::cout << "   Request body: " << json_body.substr(0, 200) << "..." << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] JSON serialization error: " << e.what() << std::endl;
            return false;
        }
        
        auto cpr_response = cpr::Post(
            cpr::Url{url},
            cpr::Header{{"Content-Type", "application/json; charset=utf-8"}},
            cpr::Body{json_body},
            cpr::Timeout{config_.send_timeout_ms}
        );
        
        if (cpr_response.status_code == 200) {
            if (!cpr_response.text.empty()) {
                try {
                    response = nlohmann::json::parse(cpr_response.text);
                } catch (const std::exception& e) {
                                    std::cerr << "[ERROR] Server response parsing error: " << e.what() << std::endl;
                std::cerr << "   Server response: " << cpr_response.text.substr(0, 200) << std::endl;
                }
            }
            return true;
        } else {
            std::cerr << "[ERROR] HTTP request failed: " << cpr_response.status_code << " - " << cpr_response.text << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] HTTP request error: " << e.what() << std::endl;
        return false;
    }
}

// AgentManager implementation
AgentManager::AgentManager(const AgentConfig& config) : config_(config) {
    initialize_metrics_collector();
    http_server_ = std::make_unique<AgentHttpServer>(config_, this);
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
        
        // ✅ Добавляем текущее время в ответ
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        
        return CommandResponse{true, "Metrics collected and sent", metrics, std::to_string(timestamp)};
    } catch (const std::exception& e) {
        return CommandResponse{false, "Error collecting metrics: " + std::string(e.what()), {}, ""};
    }
}

CommandResponse AgentManager::handle_update_config(const Command& cmd) {
    try {
        config_.update_from_json(cmd.data);
        config_.save_to_file();
        
        std::cout << "Configuration updated from server command" << std::endl;
        
        // ✅ Добавляем текущее время в ответ
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        
        return CommandResponse{true, "Configuration updated", config_.to_json(), std::to_string(timestamp)};
    } catch (const std::exception& e) {
        return CommandResponse{false, "Error updating config: " + std::string(e.what()), {}, ""};
    }
}

CommandResponse AgentManager::handle_restart(const Command& cmd) {
    // В реальной реализации здесь будет перезапуск агента
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
    
    return CommandResponse{true, "Restart command received", {}, std::to_string(timestamp)};
}

CommandResponse AgentManager::handle_stop(const Command& cmd) {
    // Останавливаем агент
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
    
    stop();
    return CommandResponse{true, "Stop command received", {}, std::to_string(timestamp)};
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
        
        // Используем update_frequency как интервал сбора метрик
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