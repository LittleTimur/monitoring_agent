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
void print_metrics_to_stream(const monitoring::SystemMetrics& metrics, std::ostream& out);

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
        std::cout << "Bytes Sent: " << std::fixed << std::setprecision(1) << (iface.bytes_sent / (1024.0 * 1024.0)) << " MB\n";
        std::cout << "Bytes Received: " << std::fixed << std::setprecision(1) << (iface.bytes_received / (1024.0 * 1024.0)) << " MB\n";
        double sendRateMB = iface.bandwidth_sent / (1024.0 * 1024.0);
        double recvRateMB = iface.bandwidth_received / (1024.0 * 1024.0);
        if (sendRateMB < 1.0) {
            std::cout << "Current Send Rate: " << std::fixed << std::setprecision(2) << (iface.bandwidth_sent / 1024.0) << " KB/s\n";
        } else {
            std::cout << "Current Send Rate: " << std::fixed << std::setprecision(2) << sendRateMB << " MB/s\n";
        }
        if (recvRateMB < 1.0) {
            std::cout << "Current Receive Rate: " << std::fixed << std::setprecision(2) << (iface.bandwidth_received / 1024.0) << " KB/s\n";
        } else {
            std::cout << "Current Receive Rate: " << std::fixed << std::setprecision(2) << recvRateMB << " MB/s\n";
        }
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
        
        // Открываем файл для записи метрик
        std::ofstream metrics_file("metrics.log");
        if (!metrics_file.is_open()) {
            std::cerr << "Failed to open metrics.log for writing" << std::endl;
            return 1;
        }
        
        // Основной цикл сбора метрик
        while (g_running) {
            // Собираем метрики
            auto metrics = collector->collect();
            
            // Выводим метрики в файл
            print_metrics_to_stream(metrics, metrics_file);
            
            // Ждем 3 секунды перед следующим сбором
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
        
        std::cout << "\nMetrics collection stopped." << std::endl;
        metrics_file.close();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

// Добавляю функцию для вывода метрик в любой std::ostream
void print_metrics_to_stream(const monitoring::SystemMetrics& metrics, std::ostream& out) {
    // Получаем текущее время
    auto time = std::chrono::system_clock::to_time_t(metrics.timestamp);
    
    out << "\n=== System Metrics at " << std::ctime(&time);
    out << "===\n\n";

    // CPU Metrics
    out << "CPU Metrics:\n";
    if (!std::isnan(metrics.cpu.usage_percent)) {
        out << "Usage: " << std::fixed << std::setprecision(1) << metrics.cpu.usage_percent << "%\n";
    } else {
        out << "Usage: N/A\n";
    }
    
    if (metrics.cpu.temperature > 0) {
        out << "Temperature: " << std::fixed << std::setprecision(1) << metrics.cpu.temperature << "°C\n";
    } else {
        out << "Temperature: N/A\n";
    }
    
    out << "Core Temperatures: ";
    for (size_t i = 0; i < metrics.cpu.core_temperatures.size(); ++i) {
        if (metrics.cpu.core_temperatures[i] > 0) {
            out << std::fixed << std::setprecision(1) << metrics.cpu.core_temperatures[i] << "°C";
        } else {
            out << "N/A";
        }
        if (i < metrics.cpu.core_temperatures.size() - 1) out << " ";
    }
    out << "\n";
    
    out << "Core Usage: ";
    for (size_t i = 0; i < metrics.cpu.core_usage.size(); ++i) {
        if (!std::isnan(metrics.cpu.core_usage[i])) {
            out << std::fixed << std::setprecision(1) << metrics.cpu.core_usage[i] << "%";
        } else {
            out << "N/A";
        }
        if (i < metrics.cpu.core_usage.size() - 1) out << " ";
    }
    out << "\n\n";

    // Memory Metrics
    out << "Memory Metrics:\n";
    const double GB = 1024.0 * 1024.0 * 1024.0;
    out << "Total: " << std::fixed << std::setprecision(1) << (metrics.memory.total_bytes / GB) << " GB\n";
    out << "Used: " << std::fixed << std::setprecision(1) << (metrics.memory.used_bytes / GB) << " GB\n";
    out << "Free: " << std::fixed << std::setprecision(1) << (metrics.memory.free_bytes / GB) << " GB\n";
    out << "Usage: " << std::fixed << std::setprecision(1) << metrics.memory.usage_percent << "%\n\n";

    // Disk Metrics
    out << "Disk Metrics:\n";
    for (const auto& partition : metrics.disk.partitions) {
        out << "\nPartition: " << partition.mount_point << " (" << partition.filesystem << ")\n";
        out << "Total: " << std::fixed << std::setprecision(1) << (partition.total_bytes / GB) << " GB\n";
        out << "Used: " << std::fixed << std::setprecision(1) << (partition.used_bytes / GB) << " GB\n";
        out << "Free: " << std::fixed << std::setprecision(1) << (partition.free_bytes / GB) << " GB\n";
        out << "Usage: " << std::fixed << std::setprecision(1) << partition.usage_percent << "%\n";
    }
    out << "\n";

    // Network Metrics
    out << "Network Metrics:\n";
    for (const auto& iface : metrics.network.interfaces) {
        out << "\nInterface: " << iface.name << "\n";
        out << "Bytes Sent: " << std::fixed << std::setprecision(1) << (iface.bytes_sent / (1024.0 * 1024.0)) << " MB\n";
        out << "Bytes Received: " << std::fixed << std::setprecision(1) << (iface.bytes_received / (1024.0 * 1024.0)) << " MB\n";
        double sendRateMB = iface.bandwidth_sent / (1024.0 * 1024.0);
        double recvRateMB = iface.bandwidth_received / (1024.0 * 1024.0);
        if (sendRateMB < 1.0) {
            out << "Current Send Rate: " << std::fixed << std::setprecision(2) << (iface.bandwidth_sent / 1024.0) << " KB/s\n";
        } else {
            out << "Current Send Rate: " << std::fixed << std::setprecision(2) << sendRateMB << " MB/s\n";
        }
        if (recvRateMB < 1.0) {
            out << "Current Receive Rate: " << std::fixed << std::setprecision(2) << (iface.bandwidth_received / 1024.0) << " KB/s\n";
        } else {
            out << "Current Receive Rate: " << std::fixed << std::setprecision(2) << recvRateMB << " MB/s\n";
        }
    }
    if (metrics.network.interfaces.empty()) {
        out << "No network interfaces found\n";
    }
    out << "\n";

    // GPU Metrics
    out << "GPU Metrics:\n";
    if (metrics.gpu.temperature > 0) {
        out << "Temperature: " << std::fixed << std::setprecision(1) << metrics.gpu.temperature << "°C\n";
    } else {
        out << "Temperature: N/A\n";
    }
    
    if (!std::isnan(metrics.gpu.usage_percent)) {
        out << "Usage: " << std::fixed << std::setprecision(1) << metrics.gpu.usage_percent << "%\n";
    } else {
        out << "Usage: N/A\n";
    }
    
    if (metrics.gpu.memory_total > 0) {
        out << "Memory Used: " << std::fixed << std::setprecision(1) << (metrics.gpu.memory_used / GB) << " GB\n";
        out << "Memory Total: " << std::fixed << std::setprecision(1) << (metrics.gpu.memory_total / GB) << " GB\n";
    } else {
        out << "Memory: N/A\n";
    }
    out << "\n";

    // HDD Metrics
    out << "HDD Metrics:\n";
    for (const auto& drive : metrics.hdd.drives) {
        out << "\nDrive: " << drive.name << "\n";
        if (drive.temperature > 0) {
            out << "Temperature: " << std::fixed << std::setprecision(1) << drive.temperature << "°C\n";
        } else {
            out << "Temperature: N/A\n";
        }
        out << "Power On Hours: " << drive.power_on_hours << "\n";
        out << "Health Status: " << drive.health_status << "\n";
    }
    if (metrics.hdd.drives.empty()) {
        out << "No HDD drives found\n";
    }
    out << "\n";

    out << "================================\n";
} 