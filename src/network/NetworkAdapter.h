#pragma once
#include "common/AeronConnection.h"
#include "UdpNetworkLayer.h"

namespace LiveText {

// Adapter to make UDP network layer compatible with existing Aeron interface
class UdpAeronPublisher {
public:
    UdpAeronPublisher(const std::string& channel, int streamId);
    ~UdpAeronPublisher();

    bool initialize();
    bool publish(const TextMessage& message);
    void shutdown();
    bool isHealthy() const;
    ConnectionStats getStats() const;

private:
    std::unique_ptr<UdpPublisher> udpPublisher_;
    std::string extractAddressFromChannel(const std::string& channel);
    int extractPortFromChannel(const std::string& channel);
};

class UdpAeronSubscriber {
public:
    using MessageCallback = std::function<void(const TextMessage&, int)>;

    UdpAeronSubscriber(const std::vector<std::string>& channels, int streamId);
    ~UdpAeronSubscriber();

    bool initialize();
    void setMessageCallback(MessageCallback callback);
    void start();
    void shutdown();
    bool isHealthy() const;
    std::vector<ConnectionStats> getStats() const;
    int getActiveFeed() const;

private:
    std::unique_ptr<UdpSubscriber> udpSubscriber_;
    std::vector<std::string> extractAddressesFromChannels(const std::vector<std::string>& channels);
    int extractPortFromChannel(const std::string& channel);
};

class UdpDualAeronPublisher {
public:
    UdpDualAeronPublisher(const std::string& primaryChannel, const std::string& secondaryChannel, int streamId);
    ~UdpDualAeronPublisher();

    bool initialize();
    bool publish(const TextMessage& message);
    void shutdown();
    std::vector<ConnectionStats> getStats() const;
    bool isHealthy() const;

private:
    std::unique_ptr<DualUdpPublisher> dualUdpPublisher_;
    std::string extractAddressFromChannel(const std::string& channel);
    int extractPortFromChannel(const std::string& channel);
};

} // namespace LiveText