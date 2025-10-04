// Network implementations - can use real UDP or stub for testing
#include "common/AeronConnection.h"
#include "network/NetworkAdapter.h"
#include <iostream>
#include <thread>
#include <chrono>

// Define whether to use real networking or stub
#ifndef USE_REAL_NETWORK
#define USE_REAL_NETWORK 1
#endif

namespace LiveText {

// AeronPublisher implementation (real or stub)
AeronPublisher::AeronPublisher(const std::string& channel, int streamId)
    : channel_(channel)
    , streamId_(streamId)
    , running_(false)
{
#if USE_REAL_NETWORK
    realPublisher_ = std::make_unique<UdpAeronPublisher>(channel, streamId);
#endif
    stats_.isConnected = false;
    stats_.hasErrors = false;
}

AeronPublisher::~AeronPublisher() {
    shutdown();
}

bool AeronPublisher::initialize() {
#if USE_REAL_NETWORK
    if (realPublisher_) {
        bool success = realPublisher_->initialize();
        if (success) {
            stats_ = realPublisher_->getStats();
            running_ = true;
            std::cout << "[REAL] AeronPublisher connected successfully on " << channel_ << std::endl;
        } else {
            std::cout << "[REAL] AeronPublisher failed to connect on " << channel_ << std::endl;
        }
        return success;
    }
#endif

    std::cout << "[STUB] AeronPublisher initializing on " << channel_ << ":" << streamId_ << std::endl;

    // Simulate connection delay
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    stats_.isConnected = true;
    stats_.hasErrors = false;
    stats_.lastError.clear();
    running_ = true;

    // Start heartbeat thread
    healthCheckThread_ = std::thread(&AeronPublisher::healthCheckLoop, this);

    std::cout << "[STUB] AeronPublisher connected successfully" << std::endl;
    return true;
}

bool AeronPublisher::publish(const TextMessage& message) {
#if USE_REAL_NETWORK
    if (realPublisher_) {
        bool success = realPublisher_->publish(message);
        if (success) {
            stats_ = realPublisher_->getStats();
        }
        return success;
    }
#endif

    if (!stats_.isConnected || !running_) {
        return false;
    }

    // Simulate message publishing
    stats_.messagesPublished++;
    stats_.bytesPublished += TextMessage::getMaxSerializedSize();

    std::string typeStr;
    switch (message.type) {
        case MessageType::TEXT_UPDATE:
            typeStr = "TEXT_UPDATE";
            std::cout << "[STUB] Published text: \"" << message.getText() << "\" (size: "
                      << (message.size == TextSize::BIG ? "BIG" : "SMALL") << ")" << std::endl;
            break;
        case MessageType::CLEAR_TEXT:
            typeStr = "CLEAR_TEXT";
            std::cout << "[STUB] Published clear command" << std::endl;
            break;
        case MessageType::HEARTBEAT:
            typeStr = "HEARTBEAT";
            break;
    }

    return true;
}

void AeronPublisher::shutdown() {
    running_ = false;
    if (healthCheckThread_.joinable()) {
        healthCheckThread_.join();
    }

    stats_.isConnected = false;
    std::cout << "[STUB] AeronPublisher shutdown" << std::endl;
}

bool AeronPublisher::isHealthy() const {
    return stats_.isConnected && !stats_.hasErrors;
}

void AeronPublisher::healthCheckLoop() {
    while (running_) {
        stats_.lastHeartbeat = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        // Publish heartbeat
        TextMessage heartbeat = TextMessage::createHeartbeat();
        if (stats_.isConnected) {
            stats_.messagesPublished++;
            stats_.bytesPublished += TextMessage::getMaxSerializedSize();
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

// AeronSubscriber implementation (real or stub)
AeronSubscriber::AeronSubscriber(const std::vector<std::string>& channels, int streamId)
    : channels_(channels)
    , streamId_(streamId)
    , running_(false)
    , activeFeed_(0)
{
#if USE_REAL_NETWORK
    realSubscriber_ = std::make_unique<UdpAeronSubscriber>(channels, streamId);
#endif
    stats_.resize(channels_.size());
}

AeronSubscriber::~AeronSubscriber() {
    shutdown();
}

bool AeronSubscriber::initialize() {
#if USE_REAL_NETWORK
    if (realSubscriber_) {
        bool success = realSubscriber_->initialize();
        if (success) {
            std::cout << "[REAL] AeronSubscriber initialized successfully with " << channels_.size() << " channels" << std::endl;
        } else {
            std::cout << "[REAL] AeronSubscriber failed to initialize" << std::endl;
        }
        return success;
    }
#endif

    std::cout << "[STUB] AeronSubscriber initializing..." << std::endl;

    // Simulate connection to all channels
    for (size_t i = 0; i < channels_.size(); ++i) {
        std::cout << "[STUB] Connecting to channel " << i << ": " << channels_[i] << std::endl;

        // Simulate connection delay
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        stats_[i].isConnected = true;
        stats_[i].hasErrors = false;
        stats_[i].lastError.clear();
    }

    activeFeed_ = 0; // Use first feed as active
    std::cout << "[STUB] AeronSubscriber connected to " << channels_.size() << " channels" << std::endl;
    return true;
}

void AeronSubscriber::setMessageCallback(MessageCallback callback) {
#if USE_REAL_NETWORK
    if (realSubscriber_) {
        realSubscriber_->setMessageCallback(callback);
        return;
    }
#endif
    messageCallback_ = callback;
}

void AeronSubscriber::start() {
#if USE_REAL_NETWORK
    if (realSubscriber_) {
        realSubscriber_->start();
        std::cout << "[REAL] AeronSubscriber started with real UDP network" << std::endl;
        return;
    }
#endif
    running_ = true;
    pollingThread_ = std::thread(&AeronSubscriber::pollingLoop, this);
    std::cout << "[STUB] AeronSubscriber started polling" << std::endl;
}

void AeronSubscriber::shutdown() {
    running_ = false;
    if (pollingThread_.joinable()) {
        pollingThread_.join();
    }

    for (auto& stat : stats_) {
        stat.isConnected = false;
    }

    std::cout << "[STUB] AeronSubscriber shutdown" << std::endl;
}

bool AeronSubscriber::isHealthy() const {
    for (const auto& stat : stats_) {
        if (stat.isConnected && !stat.hasErrors) {
            return true;
        }
    }
    return false;
}

void AeronSubscriber::pollingLoop() {
    std::cout << "[STUB] Starting polling loop (simulated messages)" << std::endl;

    while (running_) {
        // Simulate periodic heartbeats
        if (messageCallback_) {
            TextMessage heartbeat = TextMessage::createHeartbeat();
            messageCallback_(heartbeat, activeFeed_);

            stats_[activeFeed_].messagesReceived++;
            stats_[activeFeed_].bytesReceived += TextMessage::getMaxSerializedSize();
            stats_[activeFeed_].lastHeartbeat = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void AeronSubscriber::handleFragment(aeron::AtomicBuffer& buffer, aeron::util::index_t offset,
                                   aeron::util::index_t length, aeron::Header& header, int feedId) {
    // Stub implementation - not used in testing mode
}

// DualAeronPublisher stub implementation
DualAeronPublisher::DualAeronPublisher(const std::string& primaryChannel,
                                       const std::string& secondaryChannel, int streamId) {
    primary_ = std::make_unique<AeronPublisher>(primaryChannel, streamId);
    secondary_ = std::make_unique<AeronPublisher>(secondaryChannel, streamId);
    combinedStats_.resize(2);
}

DualAeronPublisher::~DualAeronPublisher() {
    shutdown();
}

bool DualAeronPublisher::initialize() {
    std::cout << "[STUB] DualAeronPublisher initializing both feeds..." << std::endl;

    bool primaryOk = primary_->initialize();
    bool secondaryOk = secondary_->initialize();

    std::cout << "[STUB] Primary: " << (primaryOk ? "OK" : "FAILED")
              << ", Secondary: " << (secondaryOk ? "OK" : "FAILED") << std::endl;

    return primaryOk || secondaryOk;
}

bool DualAeronPublisher::publish(const TextMessage& message) {
    bool primarySuccess = primary_->publish(message);
    bool secondarySuccess = secondary_->publish(message);

    return primarySuccess || secondarySuccess;
}

void DualAeronPublisher::shutdown() {
    if (primary_) {
        primary_->shutdown();
    }
    if (secondary_) {
        secondary_->shutdown();
    }
    std::cout << "[STUB] DualAeronPublisher shutdown" << std::endl;
}

const std::vector<ConnectionStats>& DualAeronPublisher::getStats() const {
    updateCombinedStats();
    return combinedStats_;
}

bool DualAeronPublisher::isHealthy() const {
    return (primary_ && primary_->isHealthy()) || (secondary_ && secondary_->isHealthy());
}

void DualAeronPublisher::updateCombinedStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);

    // Need to cast away const to modify mutable member
    auto* mutableStats = const_cast<std::vector<ConnectionStats>*>(&combinedStats_);

    if (primary_) {
        (*mutableStats)[0] = primary_->getStats();
    }
    if (secondary_) {
        (*mutableStats)[1] = secondary_->getStats();
    }
}

} // namespace LiveText