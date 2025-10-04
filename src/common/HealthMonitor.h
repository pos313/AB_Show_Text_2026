#pragma once
#include <chrono>
#include <string>
#include <vector>
#include <mutex>

namespace LiveText {

enum class HealthStatus {
    HEALTHY,
    WARNING,
    CRITICAL,
    DISCONNECTED
};

struct HealthMetric {
    std::string name;
    HealthStatus status;
    std::string details;
    uint64_t lastUpdate;
    double value;
    double threshold;

    HealthMetric(const std::string& name)
        : name(name)
        , status(HealthStatus::DISCONNECTED)
        , lastUpdate(0)
        , value(0.0)
        , threshold(0.0)
    {}
};

class HealthMonitor {
public:
    HealthMonitor();

    void updateMetric(const std::string& name, double value, HealthStatus status, const std::string& details = "");
    void updateConnectionStatus(const std::string& connectionName, bool connected, const std::string& error = "");

    HealthStatus getOverallStatus() const;
    std::vector<HealthMetric> getAllMetrics() const;
    HealthMetric getMetric(const std::string& name) const;

    bool isHealthy() const;
    std::string getStatusString() const;
    std::string getDetailedReport() const;

private:
    mutable std::mutex metricsMutex_;
    std::vector<HealthMetric> metrics_;

    HealthMetric* findMetric(const std::string& name);
    const HealthMetric* findMetric(const std::string& name) const;
    uint64_t getCurrentTimestamp() const;
};

} // namespace LiveText