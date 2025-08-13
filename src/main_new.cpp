/**
 * @file main_new.cpp
 * @brief Новая версия основного файла программы мониторинга системы
 * 
 * Использует AgentManager для управления агентом и взаимодействия с сервером
 */

#include <iostream>
#include <memory>
#include <csignal>
#include <chrono>
#include <thread>
#include <locale>
#include <clocale>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <linux/limits.h>
#endif

// Включение новых заголовочных файлов
#include "agent_config.hpp"
#include "agent_api.hpp"
#include "../include/metrics_collector.hpp"
#include <nlohmann/json.hpp>
#include <iomanip>
#include <sstream>

using json = nlohmann::json;

// Глобальная переменная для контроля работы программы
volatile bool g_running = true;

// Функция для конвертации метрик в JSON с правильной кодировкой
nlohmann::json metrics_to_json(const monitoring::SystemMetrics& metrics) {
    nlohmann::json j;
    
    // Используем безопасные методы для работы с числами
    j["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        metrics.timestamp.time_since_epoch()).count();
    j["machine_type"] = metrics.machine_type;
    
    // CPU метрики
    if (metrics.cpu.usage_percent >= 0) {
        j["cpu"]["usage_percent"] = static_cast<double>(metrics.cpu.usage_percent);
    }
    if (metrics.cpu.temperature >= 0) {
        j["cpu"]["temperature"] = static_cast<double>(metrics.cpu.temperature);
    }
    
    // Memory метрики
    if (metrics.memory.total_bytes > 0) {
        j["memory"]["total_bytes"] = static_cast<int64_t>(metrics.memory.total_bytes);
        j["memory"]["used_bytes"] = static_cast<int64_t>(metrics.memory.used_bytes);
        j["memory"]["free_bytes"] = static_cast<int64_t>(metrics.memory.free_bytes);
    }
    if (metrics.memory.usage_percent >= 0) {
        j["memory"]["usage_percent"] = static_cast<double>(metrics.memory.usage_percent);
    }
    
    // Disk метрики
    j["disk"]["partitions"] = nlohmann::json::array();
    for (const auto& part : metrics.disk.partitions) {
        nlohmann::json jp;
        jp["mount_point"] = part.mount_point;
        jp["filesystem"] = part.filesystem;
        jp["total_bytes"] = static_cast<int64_t>(part.total_bytes);
        jp["used_bytes"] = static_cast<int64_t>(part.used_bytes);
        jp["free_bytes"] = static_cast<int64_t>(part.free_bytes);
        if (part.usage_percent >= 0) {
            jp["usage_percent"] = static_cast<double>(part.usage_percent);
        }
        j["disk"]["partitions"].push_back(jp);
    }
    
    // Network метрики
    j["network"]["interfaces"] = nlohmann::json::array();
    for (const auto& iface : metrics.network.interfaces) {
        nlohmann::json ji;
        ji["name"] = iface.name;
        ji["bytes_sent"] = static_cast<int64_t>(iface.bytes_sent);
        ji["bytes_received"] = static_cast<int64_t>(iface.bytes_received);
        ji["packets_sent"] = static_cast<int64_t>(iface.packets_sent);
        ji["packets_received"] = static_cast<int64_t>(iface.packets_received);
        if (iface.bandwidth_sent >= 0) {
            ji["bandwidth_sent"] = static_cast<double>(iface.bandwidth_sent);
        }
        if (iface.bandwidth_received >= 0) {
            ji["bandwidth_received"] = static_cast<double>(iface.bandwidth_received);
        }
        j["network"]["interfaces"].push_back(ji);
    }
    
    // GPU метрики
    if (metrics.gpu.temperature >= 0) {
        j["gpu"]["temperature"] = static_cast<double>(metrics.gpu.temperature);
    }
    if (metrics.gpu.usage_percent >= 0) {
        j["gpu"]["usage_percent"] = static_cast<double>(metrics.gpu.usage_percent);
    }
    if (metrics.gpu.memory_used > 0) {
        j["gpu"]["memory_used"] = static_cast<int64_t>(metrics.gpu.memory_used);
    }
    if (metrics.gpu.memory_total > 0) {
        j["gpu"]["memory_total"] = static_cast<int64_t>(metrics.gpu.memory_total);
    }
    
    // Inventory метрики
    j["inventory"]["device_type"] = metrics.inventory.device_type;
    j["inventory"]["manufacturer"] = metrics.inventory.manufacturer;
    j["inventory"]["model"] = metrics.inventory.model;
    j["inventory"]["serial_number"] = metrics.inventory.serial_number;
    j["inventory"]["uuid"] = metrics.inventory.uuid;
    j["inventory"]["os_name"] = metrics.inventory.os_name;
    j["inventory"]["os_version"] = metrics.inventory.os_version;
    j["inventory"]["cpu_model"] = metrics.inventory.cpu_model;
    // cpu_frequency - это строка, не приводим к double
    j["inventory"]["cpu_frequency"] = metrics.inventory.cpu_frequency;
    j["inventory"]["memory_type"] = metrics.inventory.memory_type;
    j["inventory"]["disk_model"] = metrics.inventory.disk_model;
    j["inventory"]["disk_type"] = metrics.inventory.disk_type;
    // disk_total_bytes - это uint64_t, можно безопасно приводить
    j["inventory"]["disk_total_bytes"] = static_cast<int64_t>(metrics.inventory.disk_total_bytes);
    j["inventory"]["gpu_model"] = metrics.inventory.gpu_model;
    
    // Массивы
    j["inventory"]["mac_addresses"] = nlohmann::json::array();
    for (const auto& mac : metrics.inventory.mac_addresses) {
        j["inventory"]["mac_addresses"].push_back(mac);
    }
    
    j["inventory"]["ip_addresses"] = nlohmann::json::array();
    for (const auto& ip : metrics.inventory.ip_addresses) {
        j["inventory"]["ip_addresses"].push_back(ip);
    }
    
    j["inventory"]["installed_software"] = nlohmann::json::array();
    for (const auto& software : metrics.inventory.installed_software) {
        j["inventory"]["installed_software"].push_back(software);
    }
    
    return j;
}

// Обработчик сигналов для корректного завершения
void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        g_running = false;
    }
}

/**
 * @brief Точка входа в программу
 * @return 0 при успешном завершении, 1 при ошибке
 */
int main() {
    // Set console to UTF-8 for Windows
    #ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    #endif
    // Устанавливаем обработчик сигналов
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    try {
        std::cout << "[START] Starting Monitoring Agent..." << std::endl;
        
        // Определяем путь к конфигурационному файлу рядом с исполняемым файлом
        std::string config_path = agent::AgentConfig::get_config_path("agent_config.json");
        
        std::cout << "Config path: " << config_path << std::endl;
        
        // Загружаем конфигурацию
        agent::AgentConfig config = agent::AgentConfig::load_from_file(config_path);
        
        // Автоматически определяем ID и имя машины, если не заданы
        config.auto_detect_agent_info();
        
        // Сохраняем конфигурацию с автоматически определенными значениями
        config.save_to_file(config_path);
        
        std::cout << "Agent ID: " << config.agent_id << std::endl;
        std::cout << "Machine: " << config.machine_name << std::endl;
        std::cout << "Server URL: " << config.server_url << std::endl;
        // Используем config.update_frequency для интервала сбора метрик
        std::cout << "Collecting metrics every " << config.update_frequency << " seconds" << std::endl;
        
        // Выводим информацию о включенных метриках
        std::cout << "Enabled metrics:" << std::endl;
        for (const auto& [metric, enabled] : config.enabled_metrics) {
            std::cout << "   " << (enabled ? "ENABLED" : "DISABLED") << " " << metric << std::endl;
        }
        
        // Создаем и запускаем менеджер агента
        auto agent_manager = std::make_unique<agent::AgentManager>(config, config_path);
        
        std::cout << "Starting agent manager..." << std::endl;
        agent_manager->start();
        
        std::cout << "Agent started successfully!" << std::endl;
        std::cout << "Listening for commands on port " << config.command_server_port << std::endl;
        std::cout << "Collecting metrics every " << config.update_frequency << " seconds" << std::endl;
        std::cout << "=" << std::string(50, '=') << std::endl;
        
        // Main loop
        while (g_running && agent_manager->is_running()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        std::cout << "\nStopping agent..." << std::endl;
        agent_manager->stop();
        
        std::cout << "Agent stopped successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 