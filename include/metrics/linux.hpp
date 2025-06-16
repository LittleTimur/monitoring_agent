#include "interfaces/imetric_collector.hpp"
#include <fstream>
#include <filesystem>

class LinuxMetricCollector : public IMetricCollector {
public:
    nlohmann::json collect_cpu_metrics() override {
        nlohmann::json data;
        std::ifstream proc_stat("/proc/stat");
        // Парсим использование CPU (user, system, idle)
        // ...
        return data;
    }

    nlohmann::json collect_temperature_metrics() override {
        nlohmann::json data;
        for (const auto& entry : std::filesystem::directory_iterator("/sys/class/thermal")) {
            if (entry.path().string().find("thermal_zone") != std::string::npos) {
                std::ifstream temp_file(entry.path() / "temp");
                int temp; 
                temp_file >> temp;
                data[entry.path().filename()] = temp / 1000.0; // °C
            }
        }
        return data;
    }
    // ... остальные метрики
};