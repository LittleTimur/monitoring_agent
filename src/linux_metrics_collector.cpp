/**
 * @file linux_metrics_collector.cpp
 * @brief Реализация сбора системных метрик для Linux
 * 
 * Этот файл содержит реализацию класса LinuxMetricsCollector, который собирает
 * различные системные метрики в Linux через чтение системных файлов и использование
 * системных вызовов. Метрики включают CPU, память, диски, сеть, GPU и HDD.
 */

#include "../include/metrics_collector.hpp"
#include "../include/nlohmann/json.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <cstdio>
#include <array>
#include <map>
#include <thread>
#include <limits>
#include <chrono>

#ifdef __linux__
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <dirent.h>
#include <cstring>
#endif

using namespace monitoring;

namespace monitoring {

/**
 * @class LinuxMetricsCollector
 * @brief Класс для сбора системных метрик в Linux
 * 
 * Реализует интерфейс MetricsCollector для сбора различных системных метрик
 * в Linux через чтение системных файлов (/proc, /sys) и использование системных вызовов.
 */
#ifdef __linux__
class LinuxMetricsCollector : public MetricsCollector {
private:
    std::chrono::steady_clock::time_point last_network_collection_time;
    std::map<std::string, std::pair<uint64_t, uint64_t>> last_cpu_times;
    std::map<std::string, std::pair<uint64_t, uint64_t>> last_network_stats;

public:
    /**
     * @brief Конструктор класса
     * @throw std::runtime_error если недоступны необходимые системные файлы
     * 
     * Проверяет наличие и доступность критически важных системных файлов
     * для сбора метрик.
     */
    LinuxMetricsCollector() {
        if (!std::filesystem::exists("/proc/stat") || !std::filesystem::exists("/proc/meminfo")) {
            throw std::runtime_error("Cannot access /proc/stat or /proc/meminfo");
        }
        // Prime the stats for stateful calculation
        last_network_collection_time = std::chrono::steady_clock::now();
        collect_cpu_metrics(); 
        NetworkMetrics dummy_net_metrics;
        collect_network_metrics(dummy_net_metrics);
    }

    /**
     * @brief Сбор всех системных метрик
     * @return SystemMetrics структура, содержащая все собранные метрики
     * 
     * Собирает метрики CPU, памяти, дисков, сети, GPU и HDD,
     * сохраняя их в единую структуру SystemMetrics.
     */
    SystemMetrics collect() override {
        SystemMetrics metrics;
        metrics.timestamp = std::chrono::system_clock::now();

        // CPU metrics
        metrics.cpu = collect_cpu_metrics();

        // Memory metrics
        metrics.memory = collect_memory_metrics();

        // Disk metrics
        collect_disk_metrics(metrics.disk);

        // Network metrics
        collect_network_metrics(metrics.network);

        // GPU metrics (если доступно)
        metrics.gpu = collect_gpu_metrics();

        // HDD metrics
        collect_hdd_metrics(metrics.hdd);

        return metrics;
    }


private:
    /**
     * @brief Сбор метрик CPU
     * @param metrics ссылка на структуру для сохранения метрик CPU
     * 
     * Собирает информацию о:
     * - Загрузке CPU (из /proc/stat)
     * - Температуре CPU (из /sys/class/thermal)
     */
    CpuMetrics collect_cpu_metrics() {
        CpuMetrics metrics{};
        std::ifstream file("/proc/stat");
        std::string line;
        
        std::map<std::string, std::pair<uint64_t, uint64_t>> current_cpu_times;
        
        while (std::getline(file, line)) {
            if (line.rfind("cpu", 0) == 0) {
                std::istringstream ss(line);
                std::string cpu_label;
                ss >> cpu_label;
                
                uint64_t user, nice, system, idle, iowait, irq, softirq, steal;
                ss >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
                
                uint64_t idle_time = idle + iowait;
                uint64_t total_time = user + nice + system + idle + iowait + irq + softirq + steal;
                
                current_cpu_times[cpu_label] = {total_time, idle_time};
            }
        }
        
        if (!last_cpu_times.empty()) {
            if (current_cpu_times.count("cpu") && last_cpu_times.count("cpu")) {
                auto& current = current_cpu_times["cpu"];
                auto& last = last_cpu_times["cpu"];
                
                uint64_t total_diff = current.first - last.first;
                uint64_t idle_diff = current.second - last.second;
                
                if (total_diff > 0) {
                    metrics.usage_percent = static_cast<double>(total_diff - idle_diff) * 100.0 / total_diff;
                }
            }

            for (auto const& [key, val] : current_cpu_times) {
                if (key.rfind("cpu", 0) == 0 && key != "cpu") {
                     if (last_cpu_times.count(key)) {
                        auto& current = val;
                        auto& last = last_cpu_times[key];
                        
                        uint64_t total_diff = current.first - last.first;
                        uint64_t idle_diff = current.second - last.second;
                        
                        if (total_diff > 0) {
                            metrics.core_usage.push_back(static_cast<double>(total_diff - idle_diff) * 100.0 / total_diff);
                        } else {
                            metrics.core_usage.push_back(0.0);
                        }
                    }
                }
            }
        }
        
        last_cpu_times = current_cpu_times;

        try {
            double max_temp = 0.0;
            for (const auto& entry : std::filesystem::directory_iterator("/sys/class/thermal/")) {
                if (entry.is_directory() && entry.path().filename().string().substr(0, 11) == "thermal_zone") {
                    std::ifstream temp_file(entry.path() / "temp");
                    if (temp_file.is_open()) {
                        double temp;
                        temp_file >> temp;
                        temp /= 1000.0; // From millidegrees
                        if (temp > max_temp) max_temp = temp;
                    }
                }
            }
            metrics.temperature = max_temp;
        } catch (...) {}

        try {
            for (const auto& hwmon_entry : std::filesystem::directory_iterator("/sys/class/hwmon")) {
                for (const auto& file_entry : std::filesystem::directory_iterator(hwmon_entry.path())) {
                    std::string fname = file_entry.path().filename().string();
                    if (fname.find("temp") == 0 && fname.find("_input") != std::string::npos) {
                        std::ifstream temp_ifs(file_entry.path());
                        double temp_val = 0;
                        if(temp_ifs >> temp_val) {
                            metrics.core_temperatures.push_back(temp_val / 1000.0);
                        }
                    }
                }
            }
        } catch (...) {}

        return metrics;
    }

    /**
     * @brief Сбор метрик памяти
     * @param metrics ссылка на структуру для сохранения метрик памяти
     * 
     * Собирает информацию о:
     * - Общем объеме памяти
     * - Свободной памяти
     * - Использованной памяти
     * - Проценте использования
     * 
     * Данные берутся из /proc/meminfo
     */
    MemoryMetrics collect_memory_metrics() {
        MemoryMetrics metrics{};
        std::ifstream file("/proc/meminfo");
        std::string line;
        std::map<std::string, uint64_t> mem_info;

        while (std::getline(file, line)) {
            std::istringstream ss(line);
            std::string key;
            uint64_t value;
            ss >> key >> value;
            if (!key.empty()) mem_info[key.substr(0, key.size() - 1)] = value * 1024; // From kB to Bytes
        }

        metrics.total_bytes = mem_info["MemTotal"];
        metrics.free_bytes = mem_info["MemAvailable"]; // More accurate than MemFree + Buffers + Cached
        metrics.used_bytes = metrics.total_bytes - metrics.free_bytes;
        if (metrics.total_bytes > 0) {
            metrics.usage_percent = static_cast<double>(metrics.used_bytes) * 100.0 / metrics.total_bytes;
        }

        return metrics;
    }

    /**
     * @brief Сбор метрик дисков
     * @param metrics ссылка на структуру для сохранения метрик дисков
     * 
     * Собирает информацию о:
     * - Смонтированных разделах
     * - Типах файловых систем
     * - Общем и свободном пространстве
     * - Проценте использования
     * 
     * Данные берутся из /etc/mtab и statvfs
     */
    void collect_disk_metrics(DiskMetrics& metrics) {
        metrics.partitions.clear();
        std::ifstream mounts("/proc/mounts");
        std::string line;
        while (std::getline(mounts, line)) {
            std::istringstream ss(line);
            std::string device, mount_point, filesystem;
            ss >> device >> mount_point >> filesystem;
            
            if (device.rfind("/dev/", 0) != 0) continue;

            struct statvfs buf;
            if (statvfs(mount_point.c_str(), &buf) == 0) {
                DiskPartition partition;
                partition.mount_point = mount_point;
                partition.filesystem = filesystem;
                partition.total_bytes = buf.f_blocks * buf.f_frsize;
                partition.free_bytes = buf.f_bavail * buf.f_frsize;
                partition.used_bytes = partition.total_bytes - partition.free_bytes;
                if (partition.total_bytes > 0) {
                    partition.usage_percent = static_cast<double>(partition.used_bytes) * 100.0 / partition.total_bytes;
                }
                metrics.partitions.push_back(partition);
            }
        }
    }

    /**
     * @brief Сбор сетевых метрик
     * @param metrics ссылка на структуру для сохранения сетевых метрик
     * 
     * Собирает информацию о:
     * - Сетевых интерфейсах
     * - Отправленных и полученных байтах
     * - Отправленных и полученных пакетах
     * 
     * Данные берутся из /proc/net/dev
     */
    void collect_network_metrics(NetworkMetrics& metrics) {
        metrics.interfaces.clear();
        std::map<std::string, std::pair<uint64_t, uint64_t>> current_stats;

        std::ifstream file("/proc/net/dev");
        if (!file.is_open()) return;

        std::string line;
        std::getline(file, line); // Skip header
        std::getline(file, line); // Skip header

        auto now = std::chrono::steady_clock::now();
        double time_delta = std::chrono::duration_cast<std::chrono::duration<double>>(now - last_network_collection_time).count();
        
        while (std::getline(file, line)) {
            std::istringstream ss(line);
            std::string if_name;
            ss >> if_name;
            if (if_name.empty() || if_name == "lo:") continue;
            if (if_name.back() == ':') if_name.pop_back();

            uint64_t recv_bytes, recv_packets, sent_bytes, sent_packets;
            ss >> recv_bytes >> recv_packets;
            for(int i=0; i<6; ++i) ss.ignore(std::numeric_limits<std::streamsize>::max(), ' '); // Skip to sent bytes
            ss >> sent_bytes >> sent_packets;
            
            current_stats[if_name] = {sent_bytes, recv_bytes};

            NetworkInterface netif;
            netif.name = if_name;
            netif.bytes_sent = sent_bytes;
            netif.bytes_received = recv_bytes;
            netif.packets_sent = sent_packets;
            netif.packets_received = recv_packets;
            netif.bandwidth_sent = 0;
            netif.bandwidth_received = 0;

            if (last_network_stats.count(if_name) && time_delta > 0) {
                auto& last = last_network_stats[if_name];
                netif.bandwidth_sent = (sent_bytes > last.first) ? (sent_bytes - last.first) / time_delta : 0;
                netif.bandwidth_received = (recv_bytes > last.second) ? (recv_bytes - last.second) / time_delta : 0;
            }
            metrics.interfaces.push_back(netif);
        }

        last_network_stats = current_stats;
        last_network_collection_time = now;
    }

    /**
     * @brief Сбор метрик GPU
     * @param metrics ссылка на структуру для сохранения метрик GPU
     * 
     * Собирает информацию о:
     * - Температуре GPU
     * - Проценте использования GPU
     * - Использованной памяти GPU
     * - Общем объеме памяти GPU
     * 
     * Данные берутся из вывода команды nvidia-smi
     */
    GpuMetrics collect_gpu_metrics() {
        GpuMetrics metrics{};
        metrics.usage_percent = -1.0; 
        
        auto execute = [](const char* cmd) {
            std::array<char, 256> buffer;
            std::string result;
            std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
            if (pipe) {
                while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                    result += buffer.data();
                }
            }
            return result;
        };
        
        // 1. NVIDIA
        std::string nvidia_result = execute("nvidia-smi --query-gpu=temperature.gpu,utilization.gpu,memory.used,memory.total --format=csv,noheader,nounits 2>/dev/null");
        if (!nvidia_result.empty()) {
            std::istringstream iss(nvidia_result);
            double temp = 0, usage = 0, mem_used = 0, mem_total = 0;
            char comma;
            iss >> temp >> comma >> usage >> comma >> mem_used >> comma >> mem_total;
            metrics.temperature = temp;
            metrics.usage_percent = usage;
            metrics.memory_used = static_cast<uint64_t>(mem_used) * 1024 * 1024;
            metrics.memory_total = static_cast<uint64_t>(mem_total) * 1024 * 1024;
            return metrics;
        }

        // 2. AMD
        std::string amd_result = execute("rocm-smi --showtemp --showuse --showmemuse --json 2>/dev/null");
        if (!amd_result.empty()) {
            try {
                auto json = nlohmann::json::parse(amd_result);
                if (!json.empty()) {
                    // rocm-smi returns a json object with cardX keys
                    const auto& gpu = json.begin().value();
                    if(gpu.contains("Temperature (Sensor 0)")) metrics.temperature = std::stod(gpu["Temperature (Sensor 0)"].get<std::string>());
                    if(gpu.contains("GPU use (%)")) metrics.usage_percent = std::stod(gpu["GPU use (%)"].get<std::string>());
                    if(gpu.contains("VRAM used (B)")) metrics.memory_used = gpu["VRAM used (B)"].get<uint64_t>();
                    if(gpu.contains("VRAM total (B)")) metrics.memory_total = gpu["VRAM total (B)"].get<uint64_t>();
                    return metrics;
                }
            } catch (...) {}
        }
        
        return metrics;
    }

    /**
     * @brief Сбор метрик HDD
     * @param metrics ссылка на структуру для сохранения метрик HDD
     * 
     * Собирает информацию о:
     * - Температуре HDD/SSD
     * - Часе работы HDD/SSD
     * - Статусе HDD/SSD
     * 
     * Данные берутся из вывода команды smartctl
     */
    void collect_hdd_metrics(HddMetrics& metrics) {
        metrics.drives.clear();
        std::vector<std::string> devices;
        try {
            for (const auto& entry : std::filesystem::directory_iterator("/dev")) {
                std::string name = entry.path().filename().string();
                if ((name.rfind("sd", 0) == 0 && name.length() == 3) || (name.rfind("nvme", 0) == 0 && isdigit(name.back()))) {
                    devices.push_back(entry.path().string());
                }
            }
        } catch (...) { return; }

        for (const auto& dev : devices) {
            HddDrive drive;
            drive.name = dev;

            std::string cmd = "smartctl -A -H " + dev + " 2>/dev/null";
            std::string output;
            std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
            if (pipe) {
                std::array<char, 256> buffer;
                while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                    output += buffer.data();
                }
            } else {
                continue;
            }

            std::istringstream iss(output);
            std::string line;
            while (std::getline(iss, line)) {
                if (line.find("Temperature_Celsius") != std::string::npos || line.find("Temperature Sensor") != std::string::npos) {
                    std::istringstream lss(line);
                    std::string tmp;
                    while (lss >> tmp);
                    try { drive.temperature = std::stod(tmp); } catch (...) {}
                }
                if (line.find("Power_On_Hours") != std::string::npos) {
                    std::istringstream lss(line);
                    std::string tmp;
                    while (lss >> tmp);
                    try { drive.power_on_hours = std::stoull(tmp); } catch (...) {}
                }
            }
            
            if (output.find("PASSED") != std::string::npos || output.find("OK") != std::string::npos) {
                drive.health_status = "OK";
            } else if (output.find("FAILED") != std::string::npos) {
                drive.health_status = "FAILED";
            } else {
                drive.health_status = "Unknown";
            }
            
            metrics.drives.push_back(drive);
        }
    }
};

#else
// Заглушка для не-Linux систем
class LinuxMetricsCollector : public MetricsCollector {
public:
    SystemMetrics collect() override {
        throw std::runtime_error("Linux metrics collector is not available on this platform");
    }

    CpuMetrics CollectCpuMetrics() override {
        throw std::runtime_error("Linux metrics collector is not available on this platform");
    }
};
#endif

// Фабричный метод для создания коллектора метрик
std::unique_ptr<MetricsCollector> create_metrics_collector() {
#ifdef __linux__
    return std::make_unique<LinuxMetricsCollector>();
#else
    throw std::runtime_error("Linux metrics collector is not available on this platform");
#endif
}

} // namespace monitoring 