#include "UdpNetworkLayer.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>

namespace LiveText {

// UdpPublisher implementation
UdpPublisher::UdpPublisher(const std::string& multicastAddress, int port)
    : multicastAddress_(multicastAddress)
    , port_(port)
    , socket_(-1)
{
}

UdpPublisher::~UdpPublisher() {
    shutdown();
}

bool UdpPublisher::initialize() {
    std::lock_guard<std::mutex> lock(socketMutex_);

    if (initialized_) {
        return true;
    }

    // Create UDP socket
    socket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_ < 0) {
        stats_.setLastError("Failed to create socket: " + std::string(strerror(errno)));
        stats_.errors++;
        return false;
    }

    // Localhost unicast - no special socket options needed

    // Setup destination address
    std::memset(&destAddr_, 0, sizeof(destAddr_));
    destAddr_.sin_family = AF_INET;
    destAddr_.sin_port = htons(port_);
    if (inet_aton(multicastAddress_.c_str(), &destAddr_.sin_addr) == 0) {
        stats_.setLastError("Invalid multicast address: " + multicastAddress_);
        close(socket_);
        socket_ = -1;
        stats_.errors++;
        return false;
    }

    initialized_ = true;
    stats_.connected = true;
    stats_.setLastError("");
    std::cout << "UDP Publisher initialized on " << multicastAddress_ << ":" << port_ << std::endl;
    return true;
}

bool UdpPublisher::publish(const TextMessage& message) {
    if (!initialized_ || socket_ < 0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(socketMutex_);

    // Serialize message
    std::vector<uint8_t> buffer(1024);
    size_t size = message.serialize(buffer.data(), buffer.size());

    // Send UDP packet
    auto startTime = std::chrono::high_resolution_clock::now();
    ssize_t sent = sendto(socket_, buffer.data(), size, 0,
                         (struct sockaddr*)&destAddr_, sizeof(destAddr_));

    if (sent < 0) {
        stats_.setLastError("Send failed: " + std::string(strerror(errno)));
        stats_.errors++;
        return false;
    }

    // Update statistics
    stats_.messagesSent++;
    stats_.bytesSent += sent;

    // Calculate latency (rough estimate - actual latency would require receiver feedback)
    auto endTime = std::chrono::high_resolution_clock::now();
    auto latency = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    // Simple moving average for latency
    double currentAvg = stats_.avgLatencyMs.load();
    stats_.avgLatencyMs = currentAvg * 0.9 + latency * 0.1;

    return true;
}

void UdpPublisher::shutdown() {
    std::lock_guard<std::mutex> lock(socketMutex_);

    if (socket_ >= 0) {
        close(socket_);
        socket_ = -1;
    }

    initialized_ = false;
    stats_.connected = false;
    std::cout << "UDP Publisher shutdown" << std::endl;
}

bool UdpPublisher::isHealthy() const {
    return initialized_ && stats_.connected.load();
}

// UdpSubscriber implementation
UdpSubscriber::UdpSubscriber(const std::vector<std::string>& multicastAddresses, int port)
    : port_(port)
{
    feeds_.resize(multicastAddresses.size());
    for (size_t i = 0; i < multicastAddresses.size(); ++i) {
        feeds_[i].address = multicastAddresses[i];
        feeds_[i].lastMessage = std::chrono::steady_clock::now();
    }
}

UdpSubscriber::~UdpSubscriber() {
    shutdown();
}

bool UdpSubscriber::initialize() {
    bool allSuccess = true;

    for (size_t i = 0; i < feeds_.size(); ++i) {
        if (!setupSocket(i)) {
            allSuccess = false;
            std::cout << "Failed to setup socket for feed " << i << ": " << feeds_[i].address << std::endl;
        } else {
            feeds_[i].stats.connected = true;
            std::cout << "UDP Subscriber initialized for feed " << i << ": "
                      << feeds_[i].address << ":" << port_ << std::endl;
        }
    }

    return allSuccess;
}

bool UdpSubscriber::setupSocket(int feedIndex) {
    FeedInfo& feed = feeds_[feedIndex];

    // Create UDP socket
    feed.socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (feed.socket < 0) {
        feed.stats.setLastError("Failed to create socket: " + std::string(strerror(errno)));
        feed.stats.errors++;
        return false;
    }

    // Allow multiple sockets to bind to the same port
    int reuse = 1;
    if (setsockopt(feed.socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        feed.stats.setLastError("Failed to set SO_REUSEADDR: " + std::string(strerror(errno)));
        close(feed.socket);
        feed.socket = -1;
        feed.stats.errors++;
        return false;
    }

#ifdef SO_REUSEPORT
    // Enable port reuse for multiple processes/sockets
    if (setsockopt(feed.socket, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
        feed.stats.setLastError("Failed to set SO_REUSEPORT: " + std::string(strerror(errno)));
        close(feed.socket);
        feed.socket = -1;
        feed.stats.errors++;
        return false;
    }
#endif

    // Bind to multicast group
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(feed.socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        feed.stats.setLastError("Failed to bind socket: " + std::string(strerror(errno)));
        close(feed.socket);
        feed.socket = -1;
        feed.stats.errors++;
        return false;
    }

    // Localhost unicast - no need to join multicast group

    // Set socket to non-blocking
    int flags = fcntl(feed.socket, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(feed.socket, F_SETFL, flags | O_NONBLOCK);
    }

    return true;
}

void UdpSubscriber::setMessageCallback(MessageCallback callback) {
    messageCallback_ = callback;
}

void UdpSubscriber::start() {
    running_ = true;

    // Start receiver threads
    receiverThreads_.clear();
    for (size_t i = 0; i < feeds_.size(); ++i) {
        if (feeds_[i].socket >= 0) {
            receiverThreads_.emplace_back(&UdpSubscriber::receiveLoop, this, i);
        }
    }

    std::cout << "UDP Subscriber started with " << receiverThreads_.size() << " receiver threads" << std::endl;
}

void UdpSubscriber::receiveLoop(int feedIndex) {
    FeedInfo& feed = feeds_[feedIndex];
    std::vector<uint8_t> buffer(1024);

    while (running_) {
        ssize_t received = recv(feed.socket, buffer.data(), buffer.size(), 0);

        if (received > 0) {
            // Update statistics
            feed.stats.messagesReceived++;
            feed.stats.bytesReceived += received;
            feed.lastMessage = std::chrono::steady_clock::now();

            // Try to deserialize message
            try {
                TextMessage message;
                if (message.deserialize(buffer.data(), received)) {
                    if (messageCallback_) {
                        messageCallback_(message, feedIndex);
                    }
                } else {
                    feed.stats.errors++;
                    std::cout << "Failed to deserialize message on feed " << feedIndex << std::endl;
                }
            } catch (const std::exception& e) {
                feed.stats.errors++;
                feed.stats.setLastError("Deserialization error: " + std::string(e.what()));
            }

            // Update active feed based on most recent activity
            updateActiveFeed();

        } else if (received < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            feed.stats.errors++;
            feed.stats.setLastError("Receive error: " + std::string(strerror(errno)));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } else {
            // No data available, short sleep
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void UdpSubscriber::updateActiveFeed() {
    auto now = std::chrono::steady_clock::now();
    int bestFeed = activeFeed_.load();
    auto bestTime = feeds_[bestFeed].lastMessage;

    for (size_t i = 0; i < feeds_.size(); ++i) {
        if (feeds_[i].stats.connected && feeds_[i].lastMessage > bestTime) {
            bestFeed = i;
            bestTime = feeds_[i].lastMessage;
        }
    }

    activeFeed_ = bestFeed;
}

void UdpSubscriber::shutdown() {
    running_ = false;

    // Wait for all receiver threads to finish
    for (auto& thread : receiverThreads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    receiverThreads_.clear();

    // Close all sockets
    for (auto& feed : feeds_) {
        if (feed.socket >= 0) {
            close(feed.socket);
            feed.socket = -1;
        }
        feed.stats.connected = false;
    }

    std::cout << "UDP Subscriber shutdown" << std::endl;
}

bool UdpSubscriber::isHealthy() const {
    for (const auto& feed : feeds_) {
        if (feed.stats.connected.load()) {
            return true;
        }
    }
    return false;
}

std::vector<NetworkStats> UdpSubscriber::getStats() const {
    std::vector<NetworkStats> stats;
    for (const auto& feed : feeds_) {
        stats.push_back(feed.stats);
    }
    return stats;
}

// DualUdpPublisher implementation
DualUdpPublisher::DualUdpPublisher(const std::string& primaryAddress,
                                   const std::string& secondaryAddress, int port) {
    primary_ = std::make_unique<UdpPublisher>(primaryAddress, port);
    secondary_ = std::make_unique<UdpPublisher>(secondaryAddress, port);
}

DualUdpPublisher::~DualUdpPublisher() {
    shutdown();
}

bool DualUdpPublisher::initialize() {
    std::cout << "Initializing dual UDP publisher..." << std::endl;

    bool primaryOk = primary_->initialize();
    bool secondaryOk = secondary_->initialize();

    std::cout << "Primary: " << (primaryOk ? "OK" : "FAILED")
              << ", Secondary: " << (secondaryOk ? "OK" : "FAILED") << std::endl;

    return primaryOk || secondaryOk;
}

bool DualUdpPublisher::publish(const TextMessage& message) {
    bool primarySuccess = primary_->publish(message);
    bool secondarySuccess = secondary_->publish(message);

    return primarySuccess || secondarySuccess;
}

void DualUdpPublisher::shutdown() {
    if (primary_) {
        primary_->shutdown();
    }
    if (secondary_) {
        secondary_->shutdown();
    }
    std::cout << "Dual UDP Publisher shutdown" << std::endl;
}

std::vector<NetworkStats> DualUdpPublisher::getStats() const {
    std::vector<NetworkStats> stats;
    if (primary_) {
        stats.push_back(primary_->getStats());
    }
    if (secondary_) {
        stats.push_back(secondary_->getStats());
    }
    return stats;
}

bool DualUdpPublisher::isHealthy() const {
    return (primary_ && primary_->isHealthy()) || (secondary_ && secondary_->isHealthy());
}

} // namespace LiveText