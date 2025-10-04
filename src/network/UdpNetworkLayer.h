#pragma once
#include "common/TextMessage.h"
#include "common/AeronConnection.h"
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <mutex>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace LiveText {

struct NetworkStats {
    std::atomic<uint64_t> messagesSent{0};
    std::atomic<uint64_t> messagesReceived{0};
    std::atomic<uint64_t> bytesSent{0};
    std::atomic<uint64_t> bytesReceived{0};
    std::atomic<uint64_t> errors{0};
    std::atomic<uint64_t> gaps{0};
    std::atomic<double> avgLatencyMs{0.0};
    std::atomic<bool> connected{false};
    std::string lastError;
    mutable std::mutex errorMutex;

    // Copy constructor for vector compatibility
    NetworkStats(const NetworkStats& other)
        : messagesSent(other.messagesSent.load())
        , messagesReceived(other.messagesReceived.load())
        , bytesSent(other.bytesSent.load())
        , bytesReceived(other.bytesReceived.load())
        , errors(other.errors.load())
        , gaps(other.gaps.load())
        , avgLatencyMs(other.avgLatencyMs.load())
        , connected(other.connected.load())
        , lastError(other.getLastError())
    {}

    NetworkStats() = default;

    // Assignment operator
    NetworkStats& operator=(const NetworkStats& other) {
        if (this != &other) {
            messagesSent = other.messagesSent.load();
            messagesReceived = other.messagesReceived.load();
            bytesSent = other.bytesSent.load();
            bytesReceived = other.bytesReceived.load();
            errors = other.errors.load();
            gaps = other.gaps.load();
            avgLatencyMs = other.avgLatencyMs.load();
            connected = other.connected.load();
            setLastError(other.getLastError());
        }
        return *this;
    }

    void setLastError(const std::string& error) {
        std::lock_guard<std::mutex> lock(errorMutex);
        lastError = error;
    }

    std::string getLastError() const {
        std::lock_guard<std::mutex> lock(errorMutex);
        return lastError;
    }
};

class UdpPublisher {
public:
    UdpPublisher(const std::string& multicastAddress, int port);
    ~UdpPublisher();

    bool initialize();
    bool publish(const TextMessage& message);
    void shutdown();
    bool isHealthy() const;
    NetworkStats& getStats() { return stats_; }
    const NetworkStats& getStats() const { return stats_; }

private:
    std::string multicastAddress_;
    int port_;
    int socket_;
    struct sockaddr_in destAddr_;
    NetworkStats stats_;
    std::atomic<bool> initialized_{false};
    mutable std::mutex socketMutex_;
};

class UdpSubscriber {
public:
    using MessageCallback = std::function<void(const TextMessage&, int)>;

    UdpSubscriber(const std::vector<std::string>& multicastAddresses, int port);
    ~UdpSubscriber();

    bool initialize();
    void setMessageCallback(MessageCallback callback);
    void start();
    void shutdown();
    bool isHealthy() const;
    std::vector<NetworkStats> getStats() const;
    int getActiveFeed() const { return activeFeed_.load(); }

private:
    struct FeedInfo {
        std::string address;
        int socket = -1;
        NetworkStats stats;
        std::chrono::steady_clock::time_point lastMessage;
    };

    std::vector<FeedInfo> feeds_;
    int port_;
    std::vector<std::thread> receiverThreads_;
    std::atomic<bool> running_{false};
    std::atomic<int> activeFeed_{0};
    MessageCallback messageCallback_;

    void receiveLoop(int feedIndex);
    void updateActiveFeed();
    bool setupSocket(int feedIndex);
};

// Dual publisher implementation
class DualUdpPublisher {
public:
    DualUdpPublisher(const std::string& primaryAddress, const std::string& secondaryAddress, int port);
    ~DualUdpPublisher();

    bool initialize();
    bool publish(const TextMessage& message);
    void shutdown();
    bool isHealthy() const;
    std::vector<NetworkStats> getStats() const;

private:
    std::unique_ptr<UdpPublisher> primary_;
    std::unique_ptr<UdpPublisher> secondary_;
};

} // namespace LiveText