#pragma once

#include "metrics_collector.hpp"
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <filesystem>

namespace monitoring {

/**
 * @class LinuxMetricsCollector
 * @brief Класс для сбора системных метрик в Linux
 * 
 * Реализует интерфейс MetricsCollector для сбора различных системных метрик
 * в Linux через чтение системных файлов (/proc, /sys) и использование системных вызовов.
 */
class LinuxMetricsCollector : public MetricsCollector {
public:
    /**
     * @brief Конструктор класса
     * @throw std::runtime_error если недоступны необходимые системные файлы
     */
    LinuxMetricsCollector();
    ~LinuxMetricsCollector() override = default;

    /**
     * @brief Сбор всех системных метрик
     * @return SystemMetrics структура, содержащая все собранные метрики
     */
    SystemMetrics collect() override;

private:
    CpuMetrics collect_cpu_metrics();
    MemoryMetrics collect_memory_metrics();
    void collect_disk_metrics(DiskMetrics& disk_metrics);
    void collect_network_metrics(NetworkMetrics& network_metrics);
    GpuMetrics collect_gpu_metrics();
    void collect_hdd_metrics(HddMetrics& hdd_metrics);
    UserMetrics collect_user_metrics();

    // For stateful CPU usage calculation
    std::map<std::string, std::pair<uint64_t, uint64_t>> last_cpu_times;

    // For stateful network bandwidth calculation
    std::map<std::string, std::pair<uint64_t, uint64_t>> last_network_stats;
    std::chrono::steady_clock::time_point last_network_collection_time;
};

} // namespace monitoring 