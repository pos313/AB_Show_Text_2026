#include "NetworkAdapter.h"
#include <regex>
#include <iostream>

namespace LiveText {

// Helper function to extract address from Aeron channel string
// Format: "aeron:udp?endpoint=224.0.1.1:9999"
std::string extractAddress(const std::string& channel) {
    std::regex addressRegex(R"(endpoint=([^:]+):(\d+))");
    std::smatch match;
    if (std::regex_search(channel, match, addressRegex)) {
        return match[1].str();
    }
    return "224.0.1.1"; // Default fallback
}

int extractPort(const std::string& channel) {
    std::regex portRegex(R"(endpoint=[^:]+:(\d+))");
    std::smatch match;
    if (std::regex_search(channel, match, portRegex)) {
        return std::stoi(match[1].str());
    }
    return 9999; // Default fallback
}

ConnectionStats networkStatsToConnectionStats(const NetworkStats& netStats) {
    ConnectionStats connStats;
    connStats.isConnected = netStats.connected.load();
    connStats.hasErrors = netStats.errors.load() > 0;
    connStats.messagesReceived = netStats.messagesReceived.load();
    connStats.messagesPublished = netStats.messagesSent.load();
    connStats.bytesReceived = netStats.bytesReceived.load();
    connStats.bytesPublished = netStats.bytesSent.load();
    connStats.lastError = netStats.getLastError();
    connStats.lastHeartbeat = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return connStats;
}

// UdpAeronPublisher implementation
UdpAeronPublisher::UdpAeronPublisher(const std::string& channel, int streamId) {
    std::string address = extractAddress(channel);
    int port = extractPort(channel);
    udpPublisher_ = std::make_unique<UdpPublisher>(address, port);
}

UdpAeronPublisher::~UdpAeronPublisher() {
    shutdown();
}

bool UdpAeronPublisher::initialize() {
    return udpPublisher_->initialize();
}

bool UdpAeronPublisher::publish(const TextMessage& message) {
    return udpPublisher_->publish(message);
}

void UdpAeronPublisher::shutdown() {
    udpPublisher_->shutdown();
}

bool UdpAeronPublisher::isHealthy() const {
    return udpPublisher_->isHealthy();
}

ConnectionStats UdpAeronPublisher::getStats() const {
    return networkStatsToConnectionStats(udpPublisher_->getStats());
}

// UdpAeronSubscriber implementation
UdpAeronSubscriber::UdpAeronSubscriber(const std::vector<std::string>& channels, int streamId) {
    std::vector<std::string> addresses;
    int port = 9999;

    for (const std::string& channel : channels) {
        addresses.push_back(extractAddress(channel));
        port = extractPort(channel); // Use port from last channel
    }

    udpSubscriber_ = std::make_unique<UdpSubscriber>(addresses, port);
}

UdpAeronSubscriber::~UdpAeronSubscriber() {
    shutdown();
}

bool UdpAeronSubscriber::initialize() {
    return udpSubscriber_->initialize();
}

void UdpAeronSubscriber::setMessageCallback(MessageCallback callback) {
    udpSubscriber_->setMessageCallback(callback);
}

void UdpAeronSubscriber::start() {
    udpSubscriber_->start();
}

void UdpAeronSubscriber::shutdown() {
    udpSubscriber_->shutdown();
}

bool UdpAeronSubscriber::isHealthy() const {
    return udpSubscriber_->isHealthy();
}

std::vector<ConnectionStats> UdpAeronSubscriber::getStats() const {
    std::vector<NetworkStats> networkStats = udpSubscriber_->getStats();
    std::vector<ConnectionStats> connStats;

    for (const auto& netStat : networkStats) {
        connStats.push_back(networkStatsToConnectionStats(netStat));
    }

    return connStats;
}

int UdpAeronSubscriber::getActiveFeed() const {
    return udpSubscriber_->getActiveFeed();
}

// UdpDualAeronPublisher implementation
UdpDualAeronPublisher::UdpDualAeronPublisher(const std::string& primaryChannel,
                                             const std::string& secondaryChannel, int streamId) {
    std::string primaryAddress = extractAddress(primaryChannel);
    std::string secondaryAddress = extractAddress(secondaryChannel);
    int port = extractPort(primaryChannel);

    dualUdpPublisher_ = std::make_unique<DualUdpPublisher>(primaryAddress, secondaryAddress, port);
}

UdpDualAeronPublisher::~UdpDualAeronPublisher() {
    shutdown();
}

bool UdpDualAeronPublisher::initialize() {
    return dualUdpPublisher_->initialize();
}

bool UdpDualAeronPublisher::publish(const TextMessage& message) {
    return dualUdpPublisher_->publish(message);
}

void UdpDualAeronPublisher::shutdown() {
    dualUdpPublisher_->shutdown();
}

std::vector<ConnectionStats> UdpDualAeronPublisher::getStats() const {
    std::vector<NetworkStats> networkStats = dualUdpPublisher_->getStats();
    std::vector<ConnectionStats> connStats;

    for (const auto& netStat : networkStats) {
        connStats.push_back(networkStatsToConnectionStats(netStat));
    }

    return connStats;
}

bool UdpDualAeronPublisher::isHealthy() const {
    return dualUdpPublisher_->isHealthy();
}

} // namespace LiveText