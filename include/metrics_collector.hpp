/**
 * @file metrics_collector.hpp
 * @brief Заголовочный файл, определяющий структуры данных и интерфейсы для сбора системных метрик
 * 
 * Этот файл содержит определения всех структур данных, необходимых для хранения
 * системных метрик, а также интерфейсы для их сбора и отправки.
 */

#pragma once  // Защита от повторного включения файла в один модуль

// Стандартные библиотеки C++
#include <string>    // Для работы со строками (std::string)
#include <vector>    // Для динамических массивов (std::vector)
#include <memory>    // Для умных указателей (std::unique_ptr, std::shared_ptr)
#include <chrono>    // Для работы со временем (std::chrono)

/**
 * @namespace monitoring
 * @brief Пространство имен, содержащее все определения, связанные с мониторингом системы
 */
namespace monitoring {

/**
 * @struct CpuMetrics
 * @brief Структура для хранения метрик центрального процессора
 * 
 * Содержит информацию об использовании CPU, его температуре
 * и детальную информацию по каждому ядру
 */
struct CpuMetrics {
    double usage_percent;              ///< Общее использование CPU в процентах (0-100)
    double temperature;                ///< Температура CPU в градусах Цельсия
    std::vector<double> core_temperatures; ///< Температуры ядер CPU
    std::vector<double> core_usage;    ///< Использование каждого ядра в процентах
};

/**
 * @struct MemoryMetrics
 * @brief Структура для хранения метрик оперативной памяти
 * 
 * Содержит информацию об общем объеме памяти, использованной
 * и свободной памяти, а также процент использования
 */
struct MemoryMetrics {
    uint64_t total_bytes;             ///< Общий объем памяти в байтах
    uint64_t used_bytes;              ///< Использованная память в байтах
    uint64_t free_bytes;              ///< Свободная память в байтах
    double usage_percent;             ///< Процент использования памяти (0-100)
};

/**
 * @struct DiskMetrics
 * @brief Структура для хранения метрик дисковых накопителей
 * 
 * Содержит информацию о всех разделах дисков, включая
 * точки монтирования, типы файловых систем и использование пространства
 */
struct DiskPartition {
    std::string mount_point;          ///< Точка монтирования
    std::string filesystem;           ///< Тип файловой системы
    uint64_t total_bytes;             ///< Общий объем в байтах
    uint64_t used_bytes;              ///< Использовано байт
    uint64_t free_bytes;              ///< Свободно байт
    double usage_percent;             ///< Процент использования (0-100)
};

struct DiskMetrics {
    std::vector<DiskPartition> partitions; ///< Список разделов диска
};

/**
 * @struct NetworkMetrics
 * @brief Структура для хранения метрик сетевых интерфейсов
 * 
 * Содержит информацию о всех сетевых интерфейсах, включая
 * статистику передачи данных и пропускную способность
 */
struct NetworkInterface {
    std::string name;                 ///< Имя интерфейса
    uint64_t bytes_sent;              ///< Отправлено байт
    uint64_t bytes_received;          ///< Получено байт
    uint64_t packets_sent;            ///< Отправлено пакетов
    uint64_t packets_received;        ///< Получено пакетов
    uint64_t bandwidth_sent;          ///< Текущая скорость отправки (байт/с)
    uint64_t bandwidth_received;      ///< Текущая скорость получения (байт/с)
};

struct NetworkMetrics {
    std::vector<NetworkInterface> interfaces; ///< Список сетевых интерфейсов
};

/**
 * @struct GpuMetrics
 * @brief Структура для хранения метрик графического процессора
 * 
 * Содержит информацию о температуре, использовании и памяти GPU
 */
struct GpuMetrics {
    double temperature;               ///< Температура GPU в градусах Цельсия
    double usage_percent;             ///< Использование GPU в процентах (0-100)
    uint64_t memory_used;            ///< Использовано видеопамяти в байтах
    uint64_t memory_total;           ///< Общий объем видеопамяти в байтах
};

/**
 * @struct HddMetrics
 * @brief Структура для хранения метрик жестких дисков
 * 
 * Содержит информацию о температуре, времени работы
 * и состоянии здоровья физических дисков
 */
struct HddDrive {
    std::string name;                 ///< Имя диска
    double temperature;               ///< Температура в градусах Цельсия
    uint64_t power_on_hours;          ///< Время работы в часах
    std::string health_status;        ///< Статус здоровья диска
};

struct HddMetrics {
    std::vector<HddDrive> drives;     ///< Список жестких дисков
};

/**
 * @struct SystemMetrics
 * @brief Объединяющая структура для всех системных метрик
 * 
 * Содержит все метрики системы, собранные в один момент времени,
 * включая временную метку сбора
 */
struct SystemMetrics {
    std::chrono::system_clock::time_point timestamp; ///< Временная метка сбора метрик
    CpuMetrics cpu;                   ///< Метрики CPU
    MemoryMetrics memory;             ///< Метрики памяти
    DiskMetrics disk;                 ///< Метрики дисков
    NetworkMetrics network;           ///< Метрики сети
    GpuMetrics gpu;                   ///< Метрики GPU
    HddMetrics hdd;                   ///< Метрики HDD
};

/**
 * @class MetricsSender
 * @brief Абстрактный класс для отправки собранных метрик
 * 
 * Определяет интерфейс для отправки метрик на сервер мониторинга.
 * Конкретные реализации могут использовать различные протоколы
 * (HTTP, SNMP, и т.д.)
 */
class MetricsSender {
public:
    virtual ~MetricsSender() = default;  ///< Виртуальный деструктор

    /**
     * @brief Отправляет собранные метрики
     * @param metrics Собранные системные метрики
     * @return true если отправка успешна, false в случае ошибки
     */
    virtual bool send(const SystemMetrics& metrics) = 0;
};

// Базовый абстрактный класс для сбора метрик
class MetricsCollector {
public:
    virtual ~MetricsCollector() = default;
    virtual SystemMetrics collect() = 0;
};

} // namespace monitoring 