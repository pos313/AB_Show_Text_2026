#include "HealthMonitor.h"
#include <algorithm>
#include <sstream>

namespace LiveText {

HealthMonitor::HealthMonitor() {
}

void HealthMonitor::updateMetric(const std::string& name, double value, HealthStatus status, const std::string& details) {
    std::lock_guard<std::mutex> lock(metricsMutex_);

    HealthMetric* metric = findMetric(name);
    if (!metric) {
        metrics_.emplace_back(name);
        metric = &metrics_.back();
    }

    metric->value = value;
    metric->status = status;
    metric->details = details;
    metric->lastUpdate = getCurrentTimestamp();
}

void HealthMonitor::updateConnectionStatus(const std::string& connectionName, bool connected, const std::string& error) {
    HealthStatus status = connected ? HealthStatus::HEALTHY : HealthStatus::CRITICAL;
    std::string details = connected ? "Connected" : ("Disconnected: " + error);
    updateMetric(connectionName, connected ? 1.0 : 0.0, status, details);
}

HealthStatus HealthMonitor::getOverallStatus() const {
    std::lock_guard<std::mutex> lock(metricsMutex_);

    if (metrics_.empty()) {
        return HealthStatus::DISCONNECTED;
    }

    const uint64_t now = getCurrentTimestamp();
    HealthStatus overall = HealthStatus::HEALTHY;

    for (const auto& metric : metrics_) {
        const uint64_t age = now - metric.lastUpdate;

        // Consider metric stale if not updated in last 10 seconds
        if (age > 10000) {
            overall = std::max(overall, HealthStatus::WARNING);
        } else {
            overall = std::max(overall, metric.status);
        }

        // Early exit on critical status
        if (overall == HealthStatus::CRITICAL) {
            break;
        }
    }

    return overall;
}

std::vector<HealthMetric> HealthMonitor::getAllMetrics() const {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    return metrics_;
}

HealthMetric HealthMonitor::getMetric(const std::string& name) const {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    const HealthMetric* metric = findMetric(name);
    return metric ? *metric : HealthMetric(name);
}

bool HealthMonitor::isHealthy() const {
    return getOverallStatus() == HealthStatus::HEALTHY;
}

std::string HealthMonitor::getStatusString() const {
    switch (getOverallStatus()) {
        case HealthStatus::HEALTHY: return "HEALTHY";
        case HealthStatus::WARNING: return "WARNING";
        case HealthStatus::CRITICAL: return "CRITICAL";
        case HealthStatus::DISCONNECTED: return "DISCONNECTED";
    }
    return "UNKNOWN";
}

std::string HealthMonitor::getDetailedReport() const {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    std::stringstream ss;
    uint64_t now = getCurrentTimestamp();

    ss << "Health Status: " << getStatusString() << "\n";
    ss << "Metrics:\n";

    for (const auto& metric : metrics_) {
        ss << "  " << metric.name << ": ";

        if (now - metric.lastUpdate > 10000) {
            ss << "STALE";
        } else {
            switch (metric.status) {
                case HealthStatus::HEALTHY: ss << "OK"; break;
                case HealthStatus::WARNING: ss << "WARN"; break;
                case HealthStatus::CRITICAL: ss << "CRIT"; break;
                case HealthStatus::DISCONNECTED: ss << "DISC"; break;
            }
        }

        ss << " (" << metric.value << ")";
        if (!metric.details.empty()) {
            ss << " - " << metric.details;
        }
        ss << "\n";
    }

    return ss.str();
}

HealthMetric* HealthMonitor::findMetric(const std::string& name) {
    auto it = std::find_if(metrics_.begin(), metrics_.end(),
                          [&name](const HealthMetric& m) { return m.name == name; });
    return (it != metrics_.end()) ? &(*it) : nullptr;
}

const HealthMetric* HealthMonitor::findMetric(const std::string& name) const {
    auto it = std::find_if(metrics_.begin(), metrics_.end(),
                          [&name](const HealthMetric& m) { return m.name == name; });
    return (it != metrics_.end()) ? &(*it) : nullptr;
}

uint64_t HealthMonitor::getCurrentTimestamp() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

} // namespace LiveText