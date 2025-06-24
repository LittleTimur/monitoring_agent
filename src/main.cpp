/**
 * @file main.cpp
 * @brief Основной файл программы мониторинга системы
 * 
 * Этот файл содержит точку входа в программу и основной цикл
 * сбора и отображения системных метрик.
 */

// Включение заголовочных файлов
#ifdef _WIN32
#include "../include/windows_metrics_collector.hpp"
#else
#include "../include/linux_metrics_collector.hpp"
#endif
#include "../include/metrics_collector.hpp"
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
#include <nlohmann/json.hpp>
#include <cpr/cpr.h>
using nlohmann::json;

// Глобальная переменная для контроля работы программы
volatile bool g_running = true;

// Обработчик сигналов для корректного завершения
void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        g_running = false;
    }
}
void print_metrics_to_stream(const monitoring::SystemMetrics& metrics, std::ostream& out);

// Сериализация SystemMetrics в JSON
json metrics_to_json(const monitoring::SystemMetrics& metrics) {
    json j;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(metrics.timestamp.time_since_epoch()).count();
    // CPU
    j["cpu"]["usage_percent"] = metrics.cpu.usage_percent;
    j["cpu"]["temperature"] = metrics.cpu.temperature;
    j["cpu"]["core_temperatures"] = metrics.cpu.core_temperatures;
    j["cpu"]["core_usage"] = metrics.cpu.core_usage;
    // Memory
    j["memory"]["total_bytes"] = metrics.memory.total_bytes;
    j["memory"]["used_bytes"] = metrics.memory.used_bytes;
    j["memory"]["free_bytes"] = metrics.memory.free_bytes;
    j["memory"]["usage_percent"] = metrics.memory.usage_percent;
    // Disk
    for (const auto& part : metrics.disk.partitions) {
        json jp;
        jp["mount_point"] = part.mount_point;
        jp["filesystem"] = part.filesystem;
        jp["total_bytes"] = part.total_bytes;
        jp["used_bytes"] = part.used_bytes;
        jp["free_bytes"] = part.free_bytes;
        jp["usage_percent"] = part.usage_percent;
        j["disk"]["partitions"].push_back(jp);
    }
    // Network
    for (const auto& iface : metrics.network.interfaces) {
        json ji;
        ji["name"] = iface.name;
        ji["bytes_sent"] = iface.bytes_sent;
        ji["bytes_received"] = iface.bytes_received;
        ji["packets_sent"] = iface.packets_sent;
        ji["packets_received"] = iface.packets_received;
        ji["bandwidth_sent"] = iface.bandwidth_sent;
        ji["bandwidth_received"] = iface.bandwidth_received;
        j["network"]["interfaces"].push_back(ji);
    }
    // GPU
    j["gpu"]["temperature"] = metrics.gpu.temperature;
    j["gpu"]["usage_percent"] = metrics.gpu.usage_percent;
    j["gpu"]["memory_used"] = metrics.gpu.memory_used;
    j["gpu"]["memory_total"] = metrics.gpu.memory_total;
    // HDD
    for (const auto& drive : metrics.hdd.drives) {
        json jd;
        jd["name"] = drive.name;
        jd["temperature"] = drive.temperature;
        jd["power_on_hours"] = drive.power_on_hours;
        jd["health_status"] = drive.health_status;
        j["hdd"]["drives"].push_back(jd);
    }
    return j;
}

// Отправка метрик на сервер по HTTP POST
void send_metrics_http(const nlohmann::json& j) {
    try {
        auto response = cpr::Post(
            cpr::Url{"http://localhost:8080/metrics"},
            cpr::Header{{"Content-Type", "application/json"}},
            cpr::Body{j.dump()},
            cpr::Timeout{2000}
        );
        // Можно раскомментировать для отладки, если запрос проходит:
        // if (response.status_code != 200) {
        //     std::cerr << "HTTP send warning: status " << response.status_code << std::endl;
        // }
    } catch (const std::exception& e) {
        std::cerr << "HTTP send error: " << e.what() << std::endl;
    }
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
        std::unique_ptr<monitoring::MetricsCollector> collector;
#ifdef _WIN32
        collector = std::make_unique<monitoring::WindowsMetricsCollector>();
#else
        collector = monitoring::create_metrics_collector();
#endif
        std::cout << "Starting metrics collection. Press Ctrl+C to stop." << std::endl;
        
        // Открываем файл для записи метрик
        std::ofstream metrics_file("metrics.log");
        if (!metrics_file.is_open()) {
            std::cerr << "Failed to open metrics.log for writing" << std::endl;
            return 1;
        }
        
        // Открываем файл для JSON-метрик
        std::ofstream json_file("metrics.json");
        if (!json_file.is_open()) {
            std::cerr << "Failed to open metrics.json for writing" << std::endl;
            return 1;
        }
        
        // Основной цикл сбора метрик
        while (g_running) {
            // Собираем метрики
            auto metrics = collector->collect();
            
            // Выводим метрики в файл (человекочитаемый вид)
            print_metrics_to_stream(metrics, metrics_file);
            
            // Сохраняем метрики в JSON
            json j = metrics_to_json(metrics);
            json_file << j.dump() << std::endl;
            json_file.flush();
            
            // Отправляем метрики на сервер
            send_metrics_http(j);
            
            // Ждем 3 секунды перед следующим сбором
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
        
        std::cout << "\nMetrics collection stopped." << std::endl;
        metrics_file.close();
        json_file.close();
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