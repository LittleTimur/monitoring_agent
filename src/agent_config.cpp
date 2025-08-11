#include "agent_config.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <random>

#ifdef _WIN32
#include <windows.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#else
#include <unistd.h>
#include <sys/utsname.h>
#endif

namespace agent {

nlohmann::json AgentConfig::to_json() const {
    nlohmann::json j;
    j["agent_id"] = agent_id;
    j["machine_name"] = machine_name;
    j["server_url"] = server_url;
    j["command_server_url"] = command_server_url;
    j["command_server_port"] = command_server_port;
    j["command_server_host"] = command_server_host;
    j["send_timeout_ms"] = send_timeout_ms;
    j["max_buffer_size"] = max_buffer_size;
    j["auto_detect_id"] = auto_detect_id;
    j["auto_detect_name"] = auto_detect_name;
    j["update_frequency"] = update_frequency;
    
    // Сохраняем метрики как объект с флагами
    nlohmann::json metrics_obj;
    for (const auto& [metric, enabled] : enabled_metrics) {
        metrics_obj[metric] = enabled;
    }
    j["enabled_metrics"] = metrics_obj;
    
    return j;
}

AgentConfig AgentConfig::from_json(const nlohmann::json& j) {
    AgentConfig config;
    
    if (j.contains("agent_id")) config.agent_id = j["agent_id"];
    if (j.contains("machine_name")) config.machine_name = j["machine_name"];
    if (j.contains("server_url")) config.server_url = j["server_url"];
    if (j.contains("command_server_url")) config.command_server_url = j["command_server_url"];
    if (j.contains("command_server_port")) config.command_server_port = j["command_server_port"];
    if (j.contains("command_server_host")) config.command_server_host = j["command_server_host"];
    if (j.contains("send_timeout_ms")) config.send_timeout_ms = j["send_timeout_ms"];
    if (j.contains("max_buffer_size")) config.max_buffer_size = j["max_buffer_size"];
    if (j.contains("auto_detect_id")) config.auto_detect_id = j["auto_detect_id"];
    if (j.contains("auto_detect_name")) config.auto_detect_name = j["auto_detect_name"];
    if (j.contains("update_frequency")) config.update_frequency = j["update_frequency"];
    
    // Загружаем метрики из объекта с флагами
    if (j.contains("enabled_metrics")) {
        const auto& metrics_obj = j["enabled_metrics"];
        if (metrics_obj.is_object()) {
            for (auto it = metrics_obj.begin(); it != metrics_obj.end(); ++it) {
                config.enabled_metrics[it.key()] = it.value().get<bool>();
            }
        }
    }
    
    return config;
}

AgentConfig AgentConfig::load_from_file(const std::string& filename) {
    AgentConfig config;
    
    try {
        std::ifstream file(filename);
        if (file.is_open()) {
            nlohmann::json j;
            file >> j;
            config = from_json(j);
            std::cout << "Configuration loaded from " << filename << std::endl;
        } else {
            std::cout << "Configuration file not found, using defaults" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error loading configuration: " << e.what() << std::endl;
    }
    
    return config;
}

void AgentConfig::save_to_file(const std::string& filename) const {
    try {
        std::ofstream file(filename);
        if (file.is_open()) {
            file << to_json().dump(2);
            std::cout << "Configuration saved to " << filename << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error saving configuration: " << e.what() << std::endl;
    }
}

void AgentConfig::update_from_json(const nlohmann::json& j) {
    if (j.contains("update_frequency")) {
        update_frequency = j["update_frequency"];
    }
    if (j.contains("enabled_metrics")) {
        const auto& metrics_obj = j["enabled_metrics"];
        if (metrics_obj.is_object()) {
            for (auto it = metrics_obj.begin(); it != metrics_obj.end(); ++it) {
                enabled_metrics[it.key()] = it.value().get<bool>();
            }
        }
    }
    if (j.contains("server_url")) server_url = j["server_url"];
    if (j.contains("agent_id")) agent_id = j["agent_id"];
    if (j.contains("machine_name")) machine_name = j["machine_name"];
    if (j.contains("update_frequency")) update_frequency = j["update_frequency"];
}

std::string AgentConfig::generate_agent_id() {
    // Генерируем уникальный ID на основе времени и случайного числа
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    
    std::ostringstream oss;
    oss << "agent_" << timestamp << "_" << dis(gen);
    return oss.str();
}

std::string AgentConfig::get_machine_name() {
#ifdef _WIN32
    char computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(computerName);
    if (GetComputerNameA(computerName, &size)) {
        return std::string(computerName);
    }
#else
    struct utsname uts;
    if (uname(&uts) == 0) {
        return std::string(uts.nodename);
    }
#endif
    return "Unknown-Machine";
}

void AgentConfig::auto_detect_agent_info() {
    if (auto_detect_id && agent_id.empty()) {
        agent_id = generate_agent_id();
        std::cout << "Auto-detected Agent ID: " << agent_id << std::endl;
    }
    
    if (auto_detect_name && machine_name.empty()) {
        machine_name = get_machine_name();
        std::cout << "Auto-detected Machine Name: " << machine_name << std::endl;
    }
}

bool AgentConfig::is_metric_enabled(const std::string& metric_name) const {
    auto it = enabled_metrics.find(metric_name);
    return it != enabled_metrics.end() && it->second;
}

void AgentConfig::set_metric_enabled(const std::string& metric_name, bool enabled) {
    enabled_metrics[metric_name] = enabled;
}

std::vector<std::string> AgentConfig::get_enabled_metrics_list() const {
    std::vector<std::string> result;
    for (const auto& [metric, enabled] : enabled_metrics) {
        if (enabled) {
            result.push_back(metric);
        }
    }
    return result;
}

} // namespace agent 