#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

class HttpReporter {
public:
    void send(const nlohmann::json& metrics) {
        auto response = cpr::Post(
            cpr::Url{"https://api.monitoring.example.com/metrics"},
            cpr::Body{metrics.dump()},
            cpr::Header{{"Content-Type", "application/json"}}
        );
        if (response.status_code != 200) {
            throw std::runtime_error("Ошибка отправки метрик");
        }
    }
};