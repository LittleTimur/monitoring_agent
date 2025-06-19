#pragma once

#include "metrics_collector.hpp"
#include <string>
#include <vector>

namespace monitoring {

class LinuxMetricsCollector : public MetricsCollector {
public:
    LinuxMetricsCollector() = default;
    ~LinuxMetricsCollector() override = default;

    void collectMetrics() override;
    double getCpuUsage() override;
    double getMemoryUsage() override;
    std::vector<double> getDiskUsage() override;
    std::vector<double> getNetworkUsage() override;
};

} // namespace monitoring 