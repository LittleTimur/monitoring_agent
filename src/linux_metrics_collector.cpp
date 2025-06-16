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

using json = nlohmann::json;
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

        // Чтение температуры CPU из thermal zones
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
        // Чтение статистики сетевых интерфейсов из /proc/net/dev
        std::ifstream net_dev("/proc/net/dev");
        std::string line;
        // Пропускаем заголовки
        std::getline(net_dev, line);
        std::getline(net_dev, line);

        // Парсинг информации о каждом сетевом интерфейсе
        while (std::getline(net_dev, line)) {
            std::istringstream iss(line);
            std::string interface_name;
            iss >> interface_name;
            interface_name = interface_name.substr(0, interface_name.length() - 1); // Убираем двоеточие

            NetworkMetrics::Interface iface;
            iface.name = interface_name;

            // Чтение статистики интерфейса
            unsigned long bytes_received, packets_received, errs_received, drop_received,
                         bytes_sent, packets_sent, errs_sent, drop_sent;
            iss >> bytes_received >> packets_received >> errs_received >> drop_received
                >> bytes_sent >> packets_sent >> errs_sent >> drop_sent;

            iface.bytes_received = bytes_received;
            iface.packets_received = packets_received;
            iface.bytes_sent = bytes_sent;
            iface.packets_sent = packets_sent;

            // TODO: Реализовать расчет bandwidth
            metrics.interfaces.push_back(iface);
        }
    }

    /**
     * @brief Сбор метрик GPU
     * @param metrics ссылка на структуру для сохранения метрик GPU
     * 
     * Заготовка для реализации сбора метрик GPU.
     * Планируется поддержка:
     * - NVIDIA GPU через nvidia-smi
     * - AMD GPU через rocm-smi
     */
    GpuMetrics collect_gpu_metrics() {
        // Для NVIDIA GPU можно использовать nvidia-smi
        // Для AMD GPU можно использовать rocm-smi
        // Здесь должна быть реализация в зависимости от доступного оборудования
        GpuMetrics metrics;
        return metrics;
    }

    /**
     * @brief Сбор метрик HDD
     * @param metrics ссылка на структуру для сохранения метрик HDD
     * 
     * Заготовка для реализации сбора SMART-атрибутов.
     * Требуется установленный пакет smartmontools.
     */
    void collect_hdd_metrics(HddMetrics& metrics) {
        // Чтение SMART-атрибутов через smartctl
        // Требуется установленный smartmontools
        // Здесь должна быть реализация сбора SMART-атрибутов
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