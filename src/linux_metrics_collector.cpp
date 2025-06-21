/**
 * @file linux_metrics_collector.cpp
 * @brief Реализация сбора системных метрик для Linux
 * 
 * Этот файл содержит реализацию класса LinuxMetricsCollector, который собирает
 * различные системные метрики в Linux через чтение системных файлов и использование
 * системных вызовов. Метрики включают CPU, память, диски, сеть, GPU и HDD.
 */

#include "../include/metrics_collector.hpp"
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
#include <nlohmann/json.hpp>
#include <cstdio>
#include <array>
#include <map>
#include <thread>

#ifdef __linux__
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <unistd.h>
#include <net/if.h>
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
public:
    /**
     * @brief Конструктор класса
     * @throw std::runtime_error если недоступны необходимые системные файлы
     * 
     * Проверяет наличие и доступность критически важных системных файлов
     * для сбора метрик.
     */
    LinuxMetricsCollector() {
        // Проверяем доступность необходимых файлов
        if (!std::filesystem::exists("/proc/stat")) {
            throw std::runtime_error("Cannot access /proc/stat");
        }
        if (!std::filesystem::exists("/proc/meminfo")) {
            throw std::runtime_error("Cannot access /proc/meminfo");
        }
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

    CpuMetrics CollectCpuMetrics() override {
        return collect_cpu_metrics();
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
        CpuMetrics metrics;
        // Чтение CPU статистики из /proc/stat
        std::ifstream stat_file("/proc/stat");
        std::string line;
        if (std::getline(stat_file, line)) {
            std::istringstream iss(line);
            std::string cpu;
            unsigned long user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
            iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal >> guest >> guest_nice;
            
            // Расчет общего времени и процента использования
            unsigned long total = user + nice + system + idle + iowait + irq + softirq + steal;
            metrics.usage_percent = 100.0 * (total - idle) / total;
        }

        // Чтение загрузки по каждому ядру (только сырые значения, проценты невозможны без stateful)
        std::vector<double> core_usage;
        std::ifstream stat_file_cores("/proc/stat");
        // Пропускаем первую строку (общая)
        std::getline(stat_file_cores, line);
        while (std::getline(stat_file_cores, line)) {
            if (line.rfind("cpu", 0) == 0 && line[3] >= '0' && line[3] <= '9') {
                // cpuN строка
                std::istringstream iss(line);
                std::string cpu;
                unsigned long user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
                iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal >> guest >> guest_nice;
                // Без stateful-режима невозможно корректно вычислить usage_percent
                core_usage.push_back(std::numeric_limits<double>::quiet_NaN());
            } else {
                break;
            }
        }
        metrics.core_usage = core_usage;

        // Чтение температуры CPU из thermal zones (общая)
        try {
            std::filesystem::path thermal_path("/sys/class/thermal");
            for (const auto& entry : std::filesystem::directory_iterator(thermal_path)) {
                if (entry.path().filename().string().starts_with("thermal_zone")) {
                    std::ifstream temp_file(entry.path() / "temp");
                    if (temp_file.is_open()) {
                        int temp;
                        temp_file >> temp;
                        metrics.temperature = temp / 1000.0; // Конвертация из миллиградусов в градусы
                        break;
                    }
                }
            }
        } catch (const std::exception&) {
            // Если не удалось получить температуру, оставляем значение по умолчанию
        }

        // Чтение температур по ядрам через hwmon
        std::vector<double> core_temperatures;
        try {
            for (const auto& hwmon_entry : std::filesystem::directory_iterator("/sys/class/hwmon")) {
                std::filesystem::path hwmon_path = hwmon_entry.path();
                for (const auto& file_entry : std::filesystem::directory_iterator(hwmon_path)) {
                    std::string fname = file_entry.path().filename().string();
                    if (fname.find("temp") == 0 && fname.find("_input") != std::string::npos) {
                        // Проверяем label, если есть
                        std::string label_file = fname.substr(0, fname.find("_input")) + "_label";
                        std::filesystem::path label_path = hwmon_path / label_file;
                        bool is_core = false;
                        if (std::filesystem::exists(label_path)) {
                            std::ifstream label_ifs(label_path);
                            std::string label;
                            std::getline(label_ifs, label);
                            if (label.find("Core") != std::string::npos || label.find("core") != std::string::npos) {
                                is_core = true;
                            }
                        } else {
                            // Если нет label, добавляем все
                            is_core = true;
                        }
                        if (is_core) {
                            std::ifstream temp_ifs(file_entry.path());
                            int temp_val = 0;
                            temp_ifs >> temp_val;
                            core_temperatures.push_back(temp_val / 1000.0);
                        }
                    }
                }
            }
        } catch (const std::exception&) {
            // Если не удалось получить температуры по ядрам, оставляем пустым
        }
        metrics.core_temperatures = core_temperatures;

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
        MemoryMetrics metrics;
        std::ifstream meminfo("/proc/meminfo");
        std::string line;
        unsigned long total = 0, free = 0, buffers = 0, cached = 0;

        // Парсинг /proc/meminfo для получения информации о памяти
        while (std::getline(meminfo, line)) {
            std::istringstream iss(line);
            std::string key;
            unsigned long value;
            iss >> key >> value;

            if (key == "MemTotal:") total = value;
            else if (key == "MemFree:") free = value;
            else if (key == "Buffers:") buffers = value;
            else if (key == "Cached:") cached = value;
        }

        // Расчет метрик памяти
        metrics.total_bytes = total * 1024; // Конвертация из килобайт в байты
        metrics.free_bytes = (free + buffers + cached) * 1024;
        metrics.used_bytes = metrics.total_bytes - metrics.free_bytes;
        metrics.usage_percent = (metrics.used_bytes * 100.0) / metrics.total_bytes;
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
        std::ifstream mtab("/etc/mtab");
        std::string line;

        // Чтение информации о смонтированных файловых системах
        while (std::getline(mtab, line)) {
            std::istringstream iss(line);
            std::string device, mount_point, fs_type;
            iss >> device >> mount_point >> fs_type;

            // Получение статистики файловой системы
            struct statvfs stat;
            if (statvfs(mount_point.c_str(), &stat) == 0) {
                DiskMetrics::Partition partition;
                partition.mount_point = mount_point;
                partition.filesystem = fs_type;
                partition.total_bytes = stat.f_blocks * stat.f_frsize;
                partition.free_bytes = stat.f_bfree * stat.f_frsize;
                partition.used_bytes = partition.total_bytes - partition.free_bytes;
                partition.usage_percent = (partition.used_bytes * 100.0) / partition.total_bytes;
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
        // Первый замер
        std::map<std::string, std::pair<uint64_t, uint64_t>> stats0;
        {
            std::ifstream net_dev("/proc/net/dev");
            std::string line;
            std::getline(net_dev, line); // skip header
            std::getline(net_dev, line); // skip header
            while (std::getline(net_dev, line)) {
                std::istringstream iss(line);
                std::string interface_name;
                iss >> interface_name;
                interface_name = interface_name.substr(0, interface_name.length() - 1); // remove ':'
                unsigned long bytes_received, packets_received, errs_received, drop_received,
                              bytes_sent, packets_sent, errs_sent, drop_sent;
                iss >> bytes_received >> packets_received >> errs_received >> drop_received
                    >> bytes_sent >> packets_sent >> errs_sent >> drop_sent;
                stats0[interface_name] = {bytes_sent, bytes_received};
            }
        }
        // Задержка 1 секунда
        std::this_thread::sleep_for(std::chrono::seconds(1));
        // Второй замер
        std::map<std::string, std::pair<uint64_t, uint64_t>> stats1;
        {
            std::ifstream net_dev("/proc/net/dev");
            std::string line;
            std::getline(net_dev, line); // skip header
            std::getline(net_dev, line); // skip header
            while (std::getline(net_dev, line)) {
                std::istringstream iss(line);
                std::string interface_name;
                iss >> interface_name;
                interface_name = interface_name.substr(0, interface_name.length() - 1); // remove ':'
                unsigned long bytes_received, packets_received, errs_received, drop_received,
                              bytes_sent, packets_sent, errs_sent, drop_sent;
                iss >> bytes_received >> packets_received >> errs_received >> drop_received
                    >> bytes_sent >> packets_sent >> errs_sent >> drop_sent;
                stats1[interface_name] = {bytes_sent, bytes_received};
            }
        }
        // Формируем метрики
        for (const auto& [iface, val0] : stats0) {
            NetworkInterface netif;
            netif.name = iface;
            netif.bytes_sent = val0.first;
            netif.bytes_received = val0.second;
            netif.packets_sent = 0; // Можно доработать при необходимости
            netif.packets_received = 0;
            // Если есть второй замер — считаем bandwidth
            if (stats1.count(iface)) {
                netif.bandwidth_sent = stats1[iface].first > val0.first ? stats1[iface].first - val0.first : 0;
                netif.bandwidth_received = stats1[iface].second > val0.second ? stats1[iface].second - val0.second : 0;
            } else {
                netif.bandwidth_sent = 0;
                netif.bandwidth_received = 0;
            }
            metrics.interfaces.push_back(netif);
        }
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
        GpuMetrics metrics;
        // 1. NVIDIA: nvidia-smi
        {
            std::array<char, 128> buffer;
            std::string result;
            std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("nvidia-smi --query-gpu=temperature.gpu,utilization.gpu,memory.used,memory.total --format=csv,noheader,nounits 2>&1", "r"), pclose);
            if (pipe && fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                result = buffer.data();
                if (result.find("not recognized") == std::string::npos &&
                    result.find("command not found") == std::string::npos &&
                    result.find("No devices were found") == std::string::npos &&
                    result.find("NVIDIA-SMI has failed") == std::string::npos) {
                    std::istringstream iss(result);
                    double temp = 0, usage = 0, mem_used = 0, mem_total = 0;
                    char comma;
                    iss >> temp >> comma >> usage >> comma >> mem_used >> comma >> mem_total;
                    metrics.temperature = temp;
                    metrics.usage_percent = usage;
                    metrics.memory_used = static_cast<uint64_t>(mem_used) * 1024 * 1024; // MB -> bytes
                    metrics.memory_total = static_cast<uint64_t>(mem_total) * 1024 * 1024; // MB -> bytes
                    return metrics;
                }
            }
        }
        // 2. AMD: rocm-smi
        {
            std::array<char, 4096> buffer; // увеличим буфер для JSON
            std::string result;
            std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("rocm-smi --showtemp --showuse --showmemuse --json 2>&1", "r"), pclose);
            if (pipe && fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                result = buffer.data();
                if (result.find("not recognized") == std::string::npos &&
                    result.find("command not found") == std::string::npos &&
                    result.find("No AMD GPU found") == std::string::npos) {
                    try {
                        auto json = nlohmann::json::parse(result);
                        // Обычно rocm-smi возвращает массив GPU
                        if (json.is_array() && !json.empty()) {
                            const auto& gpu = json[0];
                            if (gpu.contains("Temperature (Sensor 0)"))
                                metrics.temperature = gpu["Temperature (Sensor 0)"].get<double>();
                            if (gpu.contains("GPU use (%)"))
                                metrics.usage_percent = gpu["GPU use (%)"].get<double>();
                            if (gpu.contains("VRAM used (B)"))
                                metrics.memory_used = gpu["VRAM used (B)"].get<uint64_t>();
                            if (gpu.contains("VRAM total (B)"))
                                metrics.memory_total = gpu["VRAM total (B)"].get<uint64_t>();
                            return metrics;
                        }
                    } catch (...) {
                        // Ошибка парсинга — игнорируем
                    }
                }
            }
        }
        // 3. Intel: intel_gpu_top
        {
            std::array<char, 4096> buffer;
            std::string result;
            std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("intel_gpu_top -J -s 2000 2>&1 | head -n 1", "r"), pclose);
            if (pipe && fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                result = buffer.data();
                if (result.find("not recognized") == std::string::npos &&
                    result.find("command not found") == std::string::npos &&
                    result.find("No Intel GPU found") == std::string::npos) {
                    try {
                        auto json = nlohmann::json::parse(result);
                        // Обычно intel_gpu_top возвращает объект с ключом "engines"
                        if (json.contains("engines")) {
                            double total_usage = 0.0;
                            int count = 0;
                            for (const auto& [name, val] : json["engines"].items()) {
                                if (val.contains("busy") && val["busy"].is_number()) {
                                    total_usage += val["busy"].get<double>();
                                    ++count;
                                }
                            }
                            if (count > 0)
                                metrics.usage_percent = total_usage / count;
                        }
                        // Температура и память — если есть
                        if (json.contains("temperature"))
                            metrics.temperature = json["temperature"].get<double>();
                        if (json.contains("memory")) {
                            if (json["memory"].contains("used"))
                                metrics.memory_used = json["memory"]["used"].get<uint64_t>();
                            if (json["memory"].contains("total"))
                                metrics.memory_total = json["memory"]["total"].get<uint64_t>();
                        }
                        return metrics;
                    } catch (...) {
                        // Ошибка парсинга — игнорируем
                    }
                }
            }
        }
        // Если ничего не сработало
        metrics.usage_percent = -1.0;
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
        // Список устройств: /dev/sd* и /dev/nvme*
        std::vector<std::string> devices;
        // SATA/SAS
        for (const auto& entry : std::filesystem::directory_iterator("/dev")) {
            std::string name = entry.path().filename().string();
            if (name.find("sd") == 0 && name.length() == 3) {
                devices.push_back("/dev/" + name);
            }
        }
        // NVMe
        for (const auto& entry : std::filesystem::directory_iterator("/dev")) {
            std::string name = entry.path().filename().string();
            if (name.find("nvme") == 0 && name.find("n") == std::string::npos && name.find("p") == std::string::npos) {
                devices.push_back("/dev/" + name);
            }
        }
        for (const auto& dev : devices) {
            HddDrive drive;
            drive.name = dev;
            drive.temperature = 0.0;
            drive.power_on_hours = 0;
            drive.health_status = "Unknown";
            // smartctl -A (attributes)
            std::string cmd = "smartctl -A " + dev + " 2>/dev/null";
            FILE* pipe = popen(cmd.c_str(), "r");
            if (pipe) {
                char buffer[512];
                std::string output;
                while (fgets(buffer, sizeof(buffer), pipe)) {
                    output += buffer;
                }
                pclose(pipe);
                std::istringstream iss(output);
                std::string line;
                while (std::getline(iss, line)) {
                    // Температура (обычно ID 194 или 190 для SATA, для NVMe ищем "Temperature:")
                    if (line.find("Temperature_Celsius") != std::string::npos || line.find("Temperature") != std::string::npos) {
                        std::istringstream lss(line);
                        std::string tmp;
                        int temp = 0;
                        while (lss >> tmp) {
                            try { temp = std::stoi(tmp); } catch (...) { continue; }
                        }
                        if (temp > 0 && temp < 100) drive.temperature = temp;
                    }
                    // Power_On_Hours
                    if (line.find("Power_On_Hours") != std::string::npos) {
                        std::istringstream lss(line);
                        std::string tmp;
                        int hours = 0;
                        while (lss >> tmp) {
                            try { hours = std::stoi(tmp); } catch (...) { continue; }
                        }
                        if (hours > 0) drive.power_on_hours = hours;
                    }
                    // NVMe: Temperature: 35 Celsius
                    if (line.find("Temperature:") != std::string::npos && line.find("Celsius") != std::string::npos) {
                        size_t pos = line.find(":");
                        if (pos != std::string::npos) {
                            std::string temp_str = line.substr(pos + 1);
                            try {
                                int temp = std::stoi(temp_str);
                                if (temp > 0 && temp < 100) drive.temperature = temp;
                            } catch (...) {}
                        }
                    }
                }
            }
            // smartctl -H (health)
            cmd = "smartctl -H " + dev + " 2>/dev/null";
            pipe = popen(cmd.c_str(), "r");
            if (pipe) {
                char buffer[256];
                std::string output;
                while (fgets(buffer, sizeof(buffer), pipe)) {
                    output += buffer;
                }
                pclose(pipe);
                std::istringstream iss(output);
                std::string line;
                while (std::getline(iss, line)) {
                    if (line.find("PASSED") != std::string::npos) drive.health_status = "PASSED";
                    else if (line.find("FAILED") != std::string::npos) drive.health_status = "FAILED";
                    else if (line.find("Warning") != std::string::npos) drive.health_status = "Warning";
                }
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