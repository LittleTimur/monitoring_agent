/**
 * @file main.cpp
 * @brief Основной файл программы мониторинга системы
 * 
 * Этот файл содержит точку входа в программу и основной цикл
 * сбора и отображения системных метрик.
 */

// Включение заголовочных файлов
#ifdef _WIN32
#include <windows.h>
#include "../include/windows_metrics_collector.hpp"
#else
#include "../include/linux_metrics_collector.hpp"
#endif
#include "../include/metrics_collector.hpp"
#include <iostream>    // Для ввода/вывода
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
#include <deque>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <cstdlib> // для getenv
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
    j["machine_type"] = metrics.machine_type;
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
    // Добавляем сериализацию connections
    for (const auto& conn : metrics.network.connections) {
        json jc;
        jc["local_ip"] = conn.local_ip;
        jc["local_port"] = conn.local_port;
        jc["remote_ip"] = conn.remote_ip;
        jc["remote_port"] = conn.remote_port;
        jc["protocol"] = conn.protocol;
        j["network"]["connections"].push_back(jc);
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
    // Inventory
    const auto& inv = metrics.inventory;
    j["inventory"]["device_type"] = inv.device_type;
    j["inventory"]["manufacturer"] = inv.manufacturer;
    j["inventory"]["model"] = inv.model;
    j["inventory"]["serial_number"] = inv.serial_number;
    j["inventory"]["uuid"] = inv.uuid;
    j["inventory"]["os_name"] = inv.os_name;
    j["inventory"]["os_version"] = inv.os_version;
    j["inventory"]["cpu_model"] = inv.cpu_model;
    j["inventory"]["cpu_frequency"] = inv.cpu_frequency;
    j["inventory"]["memory_type"] = inv.memory_type;
    j["inventory"]["disk_model"] = inv.disk_model;
    j["inventory"]["disk_type"] = inv.disk_type;
    j["inventory"]["disk_total_bytes"] = inv.disk_total_bytes;
    j["inventory"]["gpu_model"] = inv.gpu_model;
    j["inventory"]["mac_addresses"] = inv.mac_addresses;
    j["inventory"]["ip_addresses"] = inv.ip_addresses;
    j["inventory"]["installed_software"] = inv.installed_software;
    return j;
}

std::string get_server_url() {
    // 1. Сначала пробуем переменную окружения
    if (const char* env_p = std::getenv("MONITORING_AGENT_URL")) {
        return std::string(env_p);
    }
    // 2. Потом пробуем config.json
    std::ifstream config("config.json");
    if (config) {
        nlohmann::json j;
        config >> j;
        if (j.contains("server_url")) {
            return j["server_url"].get<std::string>();
        }
    }
    // 3. По умолчанию
    return "http://localhost:8080/metrics";
}

// Отправка метрик на сервер по HTTP POST
void send_metrics_http(const nlohmann::json& j, const std::string& url) {
    try {
        auto response = cpr::Post(
            cpr::Url{url},
            cpr::Header{{"Content-Type", "application/json"}},
            cpr::Body{j.dump()},
            cpr::Timeout{2000}
        );
        // Можно раскомментировать для отладки, если запрос проходит:
        // if (response.status_code != 200) {
        //     std::cerr << "HTTP send warning: status " << response.status_code << std::endl;
        // }
    } catch (const std::exception& e) {
        // HTTP send error silently ignored
    }
}

constexpr size_t SEND_BUFFER_MAX_SIZE = 10;
std::deque<nlohmann::json> send_buffer;
std::mutex send_buffer_mutex;
std::condition_variable send_buffer_cv;
std::atomic<bool> sender_running{true};

void sender_thread_func() {
    std::string url = get_server_url();
    while (sender_running) {
        std::unique_lock<std::mutex> lock(send_buffer_mutex);
        send_buffer_cv.wait(lock, []{ return !send_buffer.empty() || !sender_running; });
        while (!send_buffer.empty()) {
            nlohmann::json j = send_buffer.front();
            lock.unlock();
            try {
                send_metrics_http(j, url);
                // Если отправка успешна, удаляем из очереди
                lock.lock();
                send_buffer.pop_front();
                lock.unlock();
            } catch (...) {
                // Если ошибка, не удаляем, попробуем позже
                lock.lock();
                break;
            }
            lock.lock();
        }
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
#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
#else
int main() {
#endif
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
        // Основной цикл сбора метрик
        std::thread sender_thread(sender_thread_func);
        while (g_running) {
            // Собираем метрики
            auto metrics = collector->collect();
            // Только отправка на сервер
            json j = metrics_to_json(metrics);
            {
                std::lock_guard<std::mutex> lock(send_buffer_mutex);
                if (send_buffer.size() >= SEND_BUFFER_MAX_SIZE) {
                    send_buffer.pop_front(); // Удаляем самую старую запись
                }
                send_buffer.push_back(j);
                send_buffer_cv.notify_one();
            }
            // Ждем 10 минут перед следующим сбором
            std::this_thread::sleep_for(std::chrono::minutes(10));
        }
        sender_running = false;
        send_buffer_cv.notify_all();
        if (sender_thread.joinable()) sender_thread.join();
    } catch (const std::exception& e) {
        return 1;
    }
    return 0;
} 