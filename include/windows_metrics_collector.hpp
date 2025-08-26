#pragma once

#include "metrics_collector.hpp"
#include <windows.h>
#include <chrono>
#include <vector>
#include <pdh.h>

namespace monitoring {

class WindowsMetricsCollector : public MetricsCollector {
public:
    WindowsMetricsCollector();
    ~WindowsMetricsCollector() override;
    
    SystemMetrics collect() override;

private:
    CpuMetrics collect_cpu_metrics();
    MemoryMetrics collect_memory_metrics();
    DiskMetrics collect_disk_metrics();
    NetworkMetrics collect_network_metrics();
    GpuMetrics collect_gpu_metrics();
    HddMetrics collect_hdd_metrics();
    UserMetrics collect_user_metrics();

    bool is_initialized;
    size_t num_processors;
    
    // Переменные для расчета CPU usage
    uint64_t last_idle_time;
    uint64_t last_kernel_time;
    uint64_t last_user_time;
    std::chrono::steady_clock::time_point last_collection_time;

    // Для PDH (CPU по ядрам)
    PDH_HQUERY cpu_query;
    std::vector<PDH_HCOUNTER> core_counters;
};

} // namespace monitoring 