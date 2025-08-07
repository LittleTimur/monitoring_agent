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
#endif

// Включение новых заголовочных файлов
#include "agent_config.hpp"
#include "agent_api.hpp"

// Глобальная переменная для контроля работы программы
volatile bool g_running = true;

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
    std::setlocale(LC_ALL, ".UTF8");
    std::locale::global(std::locale("ru_RU.UTF-8"));
    // Устанавливаем обработчик сигналов
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    try {
        std::cout << "Starting Monitoring Agent..." << std::endl;
        
        // Загружаем конфигурацию
        agent::AgentConfig config = agent::AgentConfig::load_from_file("agent_config.json");
        
        // Автоматически определяем ID и имя машины, если не заданы
        config.auto_detect_agent_info();
        
        // Сохраняем конфигурацию с автоматически определенными значениями
        config.save_to_file();
        
        std::cout << "Agent ID: " << config.agent_id << std::endl;
        std::cout << "Machine: " << config.machine_name << std::endl;
        std::cout << "Server URL: " << config.server_url << std::endl;
        // Заменить config.heartbeat_interval_seconds на config.update_frequency (или 60)
        std::cout << "Collecting metrics every " << config.update_frequency << " seconds" << std::endl;
        
        // Выводим информацию о включенных метриках
        std::cout << "Enabled metrics:" << std::endl;
        for (const auto& [metric, enabled] : config.enabled_metrics) {
            std::cout << "   " << (enabled ? "ENABLED" : "DISABLED") << " " << metric << std::endl;
        }
        
        // Создаем и запускаем менеджер агента
        auto agent_manager = std::make_unique<agent::AgentManager>(config);
        
        std::cout << "Starting agent manager..." << std::endl;
        agent_manager->start();
        
        std::cout << "Agent started successfully!" << std::endl;
        std::cout << "Listening for commands on port " << config.command_server_port << std::endl;
        std::cout << "Collecting metrics every " << config.update_frequency << " seconds" << std::endl;
        std::cout << "=" << std::string(50, '=') << std::endl;
        
        // Основной цикл
        while (g_running && agent_manager->is_running()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        std::cout << "\nStopping agent..." << std::endl;
        agent_manager->stop();
        
        std::cout << "Agent stopped successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 