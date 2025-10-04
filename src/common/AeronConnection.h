#pragma once
#ifndef TESTING_MODE
#include <aeron/Aeron.h>
#endif
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <mutex>
#include "TextMessage.h"

// Forward declarations for real network classes
namespace LiveText {
class UdpAeronPublisher;
class UdpAeronSubscriber;
class UdpDualAeronPublisher;
}

#ifdef TESTING_MODE
// Stub definitions for testing
namespace aeron {
    class AtomicBuffer {};
    namespace util {
        using index_t = int;
    }
    class Header {};
}
#endif

namespace LiveText {

struct ConnectionStats {
    uint64_t messagesReceived = 0;
    uint64_t messagesPublished = 0;
    uint64_t bytesReceived = 0;
    uint64_t bytesPublished = 0;
    uint64_t lastHeartbeat = 0;
    bool isConnected = false;
    bool hasErrors = false;
    std::string lastError;
};

class AeronPublisher {
public:
    AeronPublisher(const std::string& channel, int streamId);
    ~AeronPublisher();

    bool initialize();
    bool publish(const TextMessage& message);
    void shutdown();

    const ConnectionStats& getStats() const { return stats_; }
    bool isHealthy() const;

private:
    std::string channel_;
    int streamId_;
#ifndef TESTING_MODE
    std::shared_ptr<aeron::Aeron> aeron_;
    std::shared_ptr<aeron::Publication> publication_;
#endif
    std::unique_ptr<UdpAeronPublisher> realPublisher_;
    ConnectionStats stats_;
    std::thread healthCheckThread_;
    std::atomic<bool> running_;

    void healthCheckLoop();
};

class AeronSubscriber {
public:
    using MessageCallback = std::function<void(const TextMessage&, int feedId)>;

    AeronSubscriber(const std::vector<std::string>& channels, int streamId);
    ~AeronSubscriber();

    bool initialize();
    void setMessageCallback(MessageCallback callback);
    void start();
    void shutdown();

    const std::vector<ConnectionStats>& getStats() const { return stats_; }
    bool isHealthy() const;
    int getActiveFeed() const { return activeFeed_; }

private:
    std::vector<std::string> channels_;
    int streamId_;
#ifndef TESTING_MODE
    std::shared_ptr<aeron::Aeron> aeron_;
    std::vector<std::shared_ptr<aeron::Subscription>> subscriptions_;
#endif
    std::unique_ptr<UdpAeronSubscriber> realSubscriber_;
    std::vector<ConnectionStats> stats_;
    MessageCallback messageCallback_;
    std::thread pollingThread_;
    std::atomic<bool> running_;
    std::atomic<int> activeFeed_;

    void pollingLoop();
    void handleFragment(aeron::AtomicBuffer& buffer, aeron::util::index_t offset,
                       aeron::util::index_t length, aeron::Header& header, int feedId);
};

class DualAeronPublisher {
public:
    DualAeronPublisher(const std::string& primaryChannel, const std::string& secondaryChannel, int streamId);
    ~DualAeronPublisher();

    bool initialize();
    bool publish(const TextMessage& message);
    void shutdown();

    const std::vector<ConnectionStats>& getStats() const;
    bool isHealthy() const;

private:
    std::unique_ptr<AeronPublisher> primary_;
    std::unique_ptr<AeronPublisher> secondary_;
    std::unique_ptr<UdpDualAeronPublisher> realDualPublisher_;
    std::vector<ConnectionStats> combinedStats_;
    mutable std::mutex statsMutex_;

    void updateCombinedStats() const;
};

} // namespace LiveText