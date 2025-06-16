/**
 * @file main.cpp
 * @brief Основной файл программы мониторинга системы
 * 
 * Этот файл содержит точку входа в программу и основной цикл
 * сбора и отображения системных метрик.
 */

// Включение заголовочных файлов
#include "../include/metrics_collector.hpp"  // Наш заголовочный файл с определениями структур и классов
#include "../include/windows_metrics_collector.hpp"  // Добавляем включение заголовочного файла
#include <iostream>    // Для ввода/вывода (std::cout, std::cerr)
#include <memory>     // Для умных указателей (std::unique_ptr)
#include <thread>     // Для работы с потоками и sleep_for
#include <chrono>     // Для работы со временем
#include <csignal>    // Для обработки сигналов (SIGINT, SIGTERM)
#include <fstream>    // Для работы с файлами
#include <ctime>      // Для работы со временем
#include <filesystem> // Для работы с файловой системой
#include <iomanip>
#include <cmath>

// Глобальная переменная для контроля работы программы
volatile bool g_running = true;

// Обработчик сигналов для корректного завершения
void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        g_running = false;
    }
}

// Функция для вывода метрик в консоль
void print_metrics(const monitoring::SystemMetrics& metrics) {
    // Получаем текущее время
    auto time = std::chrono::system_clock::to_time_t(metrics.timestamp);
    
    std::cout << "\n=== System Metrics at " << std::ctime(&time);
    std::cout << "===\n\n";

    // CPU Metrics
    std::cout << "CPU Metrics:\n";
    if (!std::isnan(metrics.cpu.usage_percent)) {
        std::cout << "Usage: " << std::fixed << std::setprecision(1) << metrics.cpu.usage_percent << "%\n";
    } else {
        std::cout << "Usage: N/A\n";
    }
    
    if (metrics.cpu.temperature > 0) {
        std::cout << "Temperature: " << std::fixed << std::setprecision(1) << metrics.cpu.temperature << "°C\n";
    } else {
        std::cout << "Temperature: N/A\n";
    }
    
    std::cout << "Core Temperatures: ";
    for (size_t i = 0; i < metrics.cpu.core_temperatures.size(); ++i) {
        if (metrics.cpu.core_temperatures[i] > 0) {
            std::cout << std::fixed << std::setprecision(1) << metrics.cpu.core_temperatures[i] << "°C";
        } else {
            std::cout << "N/A";
        }
        if (i < metrics.cpu.core_temperatures.size() - 1) std::cout << " ";
    }
    std::cout << "\n";
    
    std::cout << "Core Usage: ";
    for (size_t i = 0; i < metrics.cpu.core_usage.size(); ++i) {
        if (!std::isnan(metrics.cpu.core_usage[i])) {
            std::cout << std::fixed << std::setprecision(1) << metrics.cpu.core_usage[i] << "%";
        } else {
            std::cout << "N/A";
        }
        if (i < metrics.cpu.core_usage.size() - 1) std::cout << " ";
    }
    std::cout << "\n\n";

    // Memory Metrics
    std::cout << "Memory Metrics:\n";
    const double GB = 1024.0 * 1024.0 * 1024.0;
    std::cout << "Total: " << std::fixed << std::setprecision(1) << (metrics.memory.total_bytes / GB) << " GB\n";
    std::cout << "Used: " << std::fixed << std::setprecision(1) << (metrics.memory.used_bytes / GB) << " GB\n";
    std::cout << "Free: " << std::fixed << std::setprecision(1) << (metrics.memory.free_bytes / GB) << " GB\n";
    std::cout << "Usage: " << std::fixed << std::setprecision(1) << metrics.memory.usage_percent << "%\n\n";

    // Disk Metrics
    std::cout << "Disk Metrics:\n";
    for (const auto& partition : metrics.disk.partitions) {
        std::cout << "\nPartition: " << partition.mount_point << " (" << partition.filesystem << ")\n";
        std::cout << "Total: " << std::fixed << std::setprecision(1) << (partition.total_bytes / GB) << " GB\n";
        std::cout << "Used: " << std::fixed << std::setprecision(1) << (partition.used_bytes / GB) << " GB\n";
        std::cout << "Free: " << std::fixed << std::setprecision(1) << (partition.free_bytes / GB) << " GB\n";
        std::cout << "Usage: " << std::fixed << std::setprecision(1) << partition.usage_percent << "%\n";
    }
    std::cout << "\n";

    // Network Metrics
    std::cout << "Network Metrics:\n";
    for (const auto& iface : metrics.network.interfaces) {
        std::cout << "\nInterface: " << iface.name << "\n";
        std::cout << "Bytes Sent: " << (iface.bytes_sent / (1024.0 * 1024.0)) << " MB\n";
        std::cout << "Bytes Received: " << (iface.bytes_received / (1024.0 * 1024.0)) << " MB\n";
        std::cout << "Current Send Rate: " << (iface.bandwidth_sent / (1024.0 * 1024.0)) << " MB/s\n";
        std::cout << "Current Receive Rate: " << (iface.bandwidth_received / (1024.0 * 1024.0)) << " MB/s\n";
    }
    if (metrics.network.interfaces.empty()) {
        std::cout << "No network interfaces found\n";
    }
    std::cout << "\n";

    // GPU Metrics
    std::cout << "GPU Metrics:\n";
    if (metrics.gpu.temperature > 0) {
        std::cout << "Temperature: " << std::fixed << std::setprecision(1) << metrics.gpu.temperature << "°C\n";
    } else {
        std::cout << "Temperature: N/A\n";
    }
    
    if (!std::isnan(metrics.gpu.usage_percent)) {
        std::cout << "Usage: " << std::fixed << std::setprecision(1) << metrics.gpu.usage_percent << "%\n";
    } else {
        std::cout << "Usage: N/A\n";
    }
    
    if (metrics.gpu.memory_total > 0) {
        std::cout << "Memory Used: " << std::fixed << std::setprecision(1) << (metrics.gpu.memory_used / GB) << " GB\n";
        std::cout << "Memory Total: " << std::fixed << std::setprecision(1) << (metrics.gpu.memory_total / GB) << " GB\n";
    } else {
        std::cout << "Memory: N/A\n";
    }
    std::cout << "\n";

    // HDD Metrics
    std::cout << "HDD Metrics:\n";
    for (const auto& drive : metrics.hdd.drives) {
        std::cout << "\nDrive: " << drive.name << "\n";
        if (drive.temperature > 0) {
            std::cout << "Temperature: " << std::fixed << std::setprecision(1) << drive.temperature << "°C\n";
        } else {
            std::cout << "Temperature: N/A\n";
        }
        std::cout << "Power On Hours: " << drive.power_on_hours << "\n";
        std::cout << "Health Status: " << drive.health_status << "\n";
    }
    if (metrics.hdd.drives.empty()) {
        std::cout << "No HDD drives found\n";
    }
    std::cout << "\n";

    std::cout << "================================\n";
}

/**
 * @brief Точка входа в программу
 * @return 0 при успешном завершении, 1 при ошибке
 * 
 * Основная функция программы, которая:
 * 1. Инициализирует обработчики сигналов
 * 2. Создает коллектор метрик для текущей ОС
 * 3. В бесконечном цикле собирает и отображает метрики
 * 4. Обрабатывает ошибки и корректно завершает работу
 */
int main() {
    // Устанавливаем обработчик сигналов
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    try {
        // Создаем коллектор метрик
        std::unique_ptr<monitoring::MetricsCollector> collector = std::make_unique<monitoring::WindowsMetricsCollector>();
        
        std::cout << "Starting metrics collection. Press Ctrl+C to stop." << std::endl;
        
        // Основной цикл сбора метрик
        while (g_running) {
            // Собираем метрики
            auto metrics = collector->collect();
            
            // Выводим метрики в консоль
            print_metrics(metrics);
            
            // Ждем 1 секунду перед следующим сбором
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        std::cout << "\nMetrics collection stopped." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 