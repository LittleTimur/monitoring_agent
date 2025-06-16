/**
 * @file windows_metrics_collector.cpp
 * @brief Реализация сбора системных метрик для Windows
 * 
 * Этот файл содержит реализацию класса WindowsMetricsCollector, который собирает
 * различные системные метрики в Windows через Windows API, PDH (Performance Data Helper)
 * и WMI (Windows Management Instrumentation). Метрики включают CPU, память, диски,
 * сеть, GPU и HDD.
 */

#include "../include/windows_metrics_collector.hpp"
#include <windows.h>
#include <pdh.h>
#include <psapi.h>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <thread>
#include <iostream>

#pragma comment(lib, "pdh.lib")

namespace monitoring {

/**
 * @class WindowsMetricsCollector
 * @brief Класс для сбора системных метрик в Windows
 * 
 * Реализует интерфейс MetricsCollector для сбора различных системных метрик
 * в Windows через Windows API, PDH и WMI. Использует различные системные
 * интерфейсы для получения точных метрик производительности.
 */

WindowsMetricsCollector::WindowsMetricsCollector() : is_initialized(true) {
    // Получаем количество процессоров
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    num_processors = sysInfo.dwNumberOfProcessors;
    std::cerr << "Number of processors: " << num_processors << std::endl;
    
    // Инициализируем предыдущие значения времени
    last_idle_time = 0;
    last_kernel_time = 0;
    last_user_time = 0;
    last_collection_time = std::chrono::steady_clock::now();
    
    // Получаем начальные значения
    FILETIME idle_time, kernel_time, user_time;
    if (GetSystemTimes(&idle_time, &kernel_time, &user_time)) {
        last_idle_time = ((uint64_t)idle_time.dwHighDateTime << 32) | idle_time.dwLowDateTime;
        last_kernel_time = ((uint64_t)kernel_time.dwHighDateTime << 32) | kernel_time.dwLowDateTime;
        last_user_time = ((uint64_t)user_time.dwHighDateTime << 32) | user_time.dwLowDateTime;
    }
}

WindowsMetricsCollector::~WindowsMetricsCollector() {
    // Ничего не нужно освобождать
}

SystemMetrics WindowsMetricsCollector::collect() {
    SystemMetrics metrics;
    metrics.timestamp = std::chrono::system_clock::now();
    
    metrics.cpu = collect_cpu_metrics();
    metrics.memory = collect_memory_metrics();
    metrics.disk = collect_disk_metrics();
    metrics.network = collect_network_metrics();
    metrics.gpu = collect_gpu_metrics();
    metrics.hdd = collect_hdd_metrics();
    
    return metrics;
}

CpuMetrics WindowsMetricsCollector::collect_cpu_metrics() {
    CpuMetrics metrics;
    metrics.usage_percent = 0.0;
    metrics.temperature = 0.0;
    metrics.core_temperatures.resize(num_processors, 0.0);
    metrics.core_usage.resize(num_processors, 0.0);
    
    if (!is_initialized) {
        std::cerr << "Collector not initialized" << std::endl;
        return metrics;
    }

    // Получаем текущие значения времени
    FILETIME idle_time, kernel_time, user_time;
    if (!GetSystemTimes(&idle_time, &kernel_time, &user_time)) {
        std::cerr << "Failed to get system times. Error: " << GetLastError() << std::endl;
        return metrics;
    }

    // Конвертируем FILETIME в uint64_t
    uint64_t current_idle_time = ((uint64_t)idle_time.dwHighDateTime << 32) | idle_time.dwLowDateTime;
    uint64_t current_kernel_time = ((uint64_t)kernel_time.dwHighDateTime << 32) | kernel_time.dwLowDateTime;
    uint64_t current_user_time = ((uint64_t)user_time.dwHighDateTime << 32) | user_time.dwLowDateTime;

    // Вычисляем разницу во времени
    uint64_t idle_time_diff = current_idle_time - last_idle_time;
    uint64_t kernel_time_diff = current_kernel_time - last_kernel_time;
    uint64_t user_time_diff = current_user_time - last_user_time;

    // Вычисляем общее время процессора
    uint64_t total_time_diff = kernel_time_diff + user_time_diff;
    
    if (total_time_diff > 0) {
        // Вычисляем процент использования CPU
        double idle_percent = (double)idle_time_diff / total_time_diff * 100.0;
        metrics.usage_percent = 100.0 - idle_percent;
        
        // Для каждого ядра используем то же значение (GetSystemTimes не предоставляет информацию по ядрам)
        for (size_t i = 0; i < num_processors; ++i) {
            metrics.core_usage[i] = metrics.usage_percent;
        }
        
        std::cerr << "CPU usage: " << metrics.usage_percent << "%" << std::endl;
    }

    // Обновляем предыдущие значения
    last_idle_time = current_idle_time;
    last_kernel_time = current_kernel_time;
    last_user_time = current_user_time;
    last_collection_time = std::chrono::steady_clock::now();

    return metrics;
}

MemoryMetrics WindowsMetricsCollector::collect_memory_metrics() {
    MemoryMetrics metrics;
    
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        metrics.total_bytes = memInfo.ullTotalPhys;
        metrics.free_bytes = memInfo.ullAvailPhys;
        metrics.used_bytes = metrics.total_bytes - metrics.free_bytes;
        metrics.usage_percent = static_cast<double>(memInfo.dwMemoryLoad);
    } else {
        metrics.total_bytes = 0;
        metrics.free_bytes = 0;
        metrics.used_bytes = 0;
        metrics.usage_percent = 0.0;
    }
    
    return metrics;
}

DiskMetrics WindowsMetricsCollector::collect_disk_metrics() {
    DiskMetrics metrics;
    
    // Получаем список дисков
    DWORD drives = GetLogicalDrives();
    char driveLetter = 'A';
    
    while (drives) {
        if (drives & 1) {
            std::string rootPath = std::string(1, driveLetter) + ":\\";
            UINT driveType = GetDriveTypeA(rootPath.c_str());
            
            if (driveType == DRIVE_FIXED) {
                ULARGE_INTEGER freeBytesAvailable, totalBytes, totalFreeBytes;
                if (GetDiskFreeSpaceExA(rootPath.c_str(), &freeBytesAvailable, &totalBytes, &totalFreeBytes)) {
                    DiskPartition partition;
                    partition.mount_point = rootPath;
                    partition.filesystem = "NTFS"; // TODO: Получить реальный тип файловой системы
                    partition.total_bytes = totalBytes.QuadPart;
                    partition.free_bytes = freeBytesAvailable.QuadPart;
                    partition.used_bytes = totalBytes.QuadPart - freeBytesAvailable.QuadPart;
                    partition.usage_percent = (partition.used_bytes * 100.0) / partition.total_bytes;
                    
                    metrics.partitions.push_back(partition);
                }
            }
        }
        drives >>= 1;
        driveLetter++;
    }
    
    return metrics;
}

NetworkMetrics WindowsMetricsCollector::collect_network_metrics() {
    NetworkMetrics metrics;
    // TODO: Реализовать сбор сетевых метрик
    return metrics;
}

GpuMetrics WindowsMetricsCollector::collect_gpu_metrics() {
    GpuMetrics metrics;
    metrics.temperature = 0.0;
    metrics.usage_percent = 0.0;
    metrics.memory_used = 0;
    metrics.memory_total = 0;

    // Получаем информацию о GPU через WMI
    // TODO: Реализовать сбор метрик GPU через WMI или NVML
    // Пока возвращаем нулевые значения
    
    return metrics;
}

HddMetrics WindowsMetricsCollector::collect_hdd_metrics() {
    HddMetrics metrics;
    // TODO: Реализовать сбор метрик HDD
    return metrics;
}

} // namespace monitoring 