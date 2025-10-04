#include "AeronConnection.h"
#include <iostream>
#include <mutex>

namespace LiveText {

// AeronPublisher Implementation
AeronPublisher::AeronPublisher(const std::string& channel, int streamId)
    : channel_(channel)
    , streamId_(streamId)
    , running_(false)
{
}

AeronPublisher::~AeronPublisher() {
    shutdown();
}

bool AeronPublisher::initialize() {
    try {
        aeron::Context context;
        aeron_ = aeron::Aeron::connect(context);

        const std::string publicationChannel = channel_;
        publication_ = aeron_->addPublication(publicationChannel, streamId_);

        // Wait for connection
        int attempts = 0;
        while (!publication_->isConnected() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        if (!publication_->isConnected()) {
            stats_.hasErrors = true;
            stats_.lastError = "Failed to connect publication";
            return false;
        }

        stats_.isConnected = true;
        running_ = true;
        healthCheckThread_ = std::thread(&AeronPublisher::healthCheckLoop, this);

        return true;
    } catch (const std::exception& e) {
        stats_.hasErrors = true;
        stats_.lastError = e.what();
        return false;
    }
}

bool AeronPublisher::publish(const TextMessage& message) {
    if (!publication_ || !publication_->isConnected()) {
        return false;
    }

    try {
        uint8_t buffer[TextMessage::getMaxSerializedSize()];
        size_t length = message.serialize(buffer, sizeof(buffer));

        if (length == 0) {
            return false;
        }

        aeron::AtomicBuffer atomicBuffer(buffer, length);
        long result = publication_->offer(atomicBuffer, 0, length);

        if (result > 0) {
            stats_.messagesPublished++;
            stats_.bytesPublished += length;
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        stats_.hasErrors = true;
        stats_.lastError = e.what();
        return false;
    }
}

void AeronPublisher::shutdown() {
    running_ = false;
    if (healthCheckThread_.joinable()) {
        healthCheckThread_.join();
    }

    if (publication_) {
        publication_->close();
        publication_.reset();
    }

    if (aeron_) {
        aeron_->close();
        aeron_.reset();
    }

    stats_.isConnected = false;
}

bool AeronPublisher::isHealthy() const {
    return stats_.isConnected && !stats_.hasErrors &&
           (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count() - stats_.lastHeartbeat) < 5000;
}

void AeronPublisher::healthCheckLoop() {
    while (running_) {
        if (publication_ && publication_->isConnected()) {
            TextMessage heartbeat = TextMessage::createHeartbeat();
            publish(heartbeat);
            stats_.lastHeartbeat = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

// AeronSubscriber Implementation
AeronSubscriber::AeronSubscriber(const std::vector<std::string>& channels, int streamId)
    : channels_(channels)
    , streamId_(streamId)
    , running_(false)
    , activeFeed_(-1)
{
    stats_.resize(channels_.size());
}

AeronSubscriber::~AeronSubscriber() {
    shutdown();
}

bool AeronSubscriber::initialize() {
    try {
        aeron::Context context;
        aeron_ = aeron::Aeron::connect(context);

        for (size_t i = 0; i < channels_.size(); ++i) {
            auto subscription = aeron_->addSubscription(channels_[i], streamId_);
            subscriptions_.push_back(subscription);

            // Wait for connection
            int attempts = 0;
            while (!subscription->isConnected() && attempts < 100) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                attempts++;
            }

            if (subscription->isConnected()) {
                stats_[i].isConnected = true;
                if (activeFeed_ == -1) {
                    activeFeed_ = i;
                }
            } else {
                stats_[i].hasErrors = true;
                stats_[i].lastError = "Failed to connect subscription";
            }
        }

        return activeFeed_ >= 0;
    } catch (const std::exception& e) {
        for (auto& stat : stats_) {
            stat.hasErrors = true;
            stat.lastError = e.what();
        }
        return false;
    }
}

void AeronSubscriber::setMessageCallback(MessageCallback callback) {
    messageCallback_ = callback;
}

void AeronSubscriber::start() {
    running_ = true;
    pollingThread_ = std::thread(&AeronSubscriber::pollingLoop, this);
}

void AeronSubscriber::shutdown() {
    running_ = false;
    if (pollingThread_.joinable()) {
        pollingThread_.join();
    }

    for (auto& subscription : subscriptions_) {
        if (subscription) {
            subscription->close();
        }
    }
    subscriptions_.clear();

    if (aeron_) {
        aeron_->close();
        aeron_.reset();
    }

    for (auto& stat : stats_) {
        stat.isConnected = false;
    }
}

bool AeronSubscriber::isHealthy() const {
    for (const auto& stat : stats_) {
        if (stat.isConnected && !stat.hasErrors &&
            (std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::system_clock::now().time_since_epoch()).count() - stat.lastHeartbeat) < 10000) {
            return true;
        }
    }
    return false;
}

void AeronSubscriber::pollingLoop() {
    while (running_) {
        bool messageReceived = false;

        for (size_t i = 0; i < subscriptions_.size(); ++i) {
            if (subscriptions_[i] && subscriptions_[i]->isConnected()) {
                auto fragmentHandler = [this, i](aeron::AtomicBuffer& buffer,
                                                 aeron::util::index_t offset,
                                                 aeron::util::index_t length,
                                                 aeron::Header& header) {
                    handleFragment(buffer, offset, length, header, i);
                };

                int fragments = subscriptions_[i]->poll(fragmentHandler, 10);
                if (fragments > 0) {
                    messageReceived = true;
                    if (activeFeed_ != static_cast<int>(i)) {
                        activeFeed_ = i;  // Switch to active feed
                    }
                }
            }
        }

        if (!messageReceived) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void AeronSubscriber::handleFragment(aeron::AtomicBuffer& buffer, aeron::util::index_t offset,
                                   aeron::util::index_t length, aeron::Header& header, int feedId) {
    if (messageCallback_) {
        TextMessage message;
        if (message.deserialize(buffer.buffer() + offset, length)) {
            stats_[feedId].messagesReceived++;
            stats_[feedId].bytesReceived += length;

            if (message.type == MessageType::HEARTBEAT) {
                stats_[feedId].lastHeartbeat = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
            } else {
                messageCallback_(message, feedId);
            }
        }
    }
}

// DualAeronPublisher Implementation
DualAeronPublisher::DualAeronPublisher(const std::string& primaryChannel,
                                       const std::string& secondaryChannel, int streamId)
{
    primary_ = std::make_unique<AeronPublisher>(primaryChannel, streamId);
    secondary_ = std::make_unique<AeronPublisher>(secondaryChannel, streamId);
    combinedStats_.resize(2);
}

DualAeronPublisher::~DualAeronPublisher() {
    shutdown();
}

bool DualAeronPublisher::initialize() {
    bool primaryOk = primary_->initialize();
    bool secondaryOk = secondary_->initialize();

    return primaryOk || secondaryOk;  // At least one must succeed
}

bool DualAeronPublisher::publish(const TextMessage& message) {
    bool primarySuccess = primary_->publish(message);
    bool secondarySuccess = secondary_->publish(message);

    return primarySuccess || secondarySuccess;  // At least one must succeed
}

void DualAeronPublisher::shutdown() {
    if (primary_) {
        primary_->shutdown();
    }
    if (secondary_) {
        secondary_->shutdown();
    }
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
    if (primary_) {
        combinedStats_[0] = primary_->getStats();
    }
    if (secondary_) {
        combinedStats_[1] = secondary_->getStats();
    }
}

} // namespace LiveText