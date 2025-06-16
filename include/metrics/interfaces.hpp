#pragma once
#include <nlohmann/json.hpp>

class IMetricCollector {
public:
    virtual ~IMetricCollector() = default;
    virtual nlohmann::json collect_cpu_metrics() = 0;
    virtual nlohmann::json collect_memory_metrics() = 0;
    virtual nlohmann::json collect_disk_metrics() = 0;
    virtual nlohmann::json collect_network_metrics() = 0;
    virtual nlohmann::json collect_temperature_metrics() = 0;
};