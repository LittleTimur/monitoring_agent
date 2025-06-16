#include "metrics/interfaces.hpp"
#include <windows.h>
#include <wbemidl.h>

class WindowsMetricCollector : public IMetricCollector {
public:
    nlohmann::json collect_cpu() override {
        nlohmann::json data;
        // Пример через WMI (упрощённо)
        data["cpu_usage"] = GetCpuUsageViaWMI(); // Заглушка
        return data;
    }
    // ... остальные метрики
};