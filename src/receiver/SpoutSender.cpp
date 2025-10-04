#include "SpoutSender.h"
#include <iostream>

namespace LiveText {

SpoutSender::SpoutSender()
    : initialized_(false)
    , width_(0)
    , height_(0)
{
#ifdef _WIN32
    spout_ = std::make_unique<spoutGL>();
#endif
}

SpoutSender::~SpoutSender() {
    shutdown();
}

bool SpoutSender::initialize(const std::string& senderName, int width, int height) {
    senderName_ = senderName;
    width_ = width;
    height_ = height;

#ifdef _WIN32
    if (!spout_) {
        std::cerr << "Spout not available on this platform" << std::endl;
        return false;
    }

    // Create Spout sender
    if (!spout_->CreateSender(senderName_.c_str(), width_, height_)) {
        std::cerr << "Failed to create Spout sender: " << senderName_ << std::endl;
        return false;
    }

    std::cout << "Spout sender initialized: " << senderName_ << " (" << width_ << "x" << height_ << ")" << std::endl;
    initialized_ = true;
    return true;
#else
    std::cout << "Spout not available on this platform - running without Spout" << std::endl;
    initialized_ = true; // Allow the app to run without Spout on non-Windows platforms
    return true;
#endif
}

void SpoutSender::shutdown() {
#ifdef _WIN32
    if (spout_ && initialized_) {
        spout_->ReleaseSender();
        initialized_ = false;
    }
#endif
    initialized_ = false;
}

bool SpoutSender::sendTexture(unsigned int textureID, int width, int height) {
    if (!initialized_) {
        return false;
    }

#ifdef _WIN32
    if (!spout_) {
        return false;
    }

    // Update sender size if changed
    if (width != width_ || height != height_) {
        setSize(width, height);
    }

    // Send the texture
    return spout_->SendTexture(textureID, GL_TEXTURE_2D, width_, height_);
#else
    // On non-Windows platforms, just return true to keep the app functional
    return true;
#endif
}

void SpoutSender::setSize(int width, int height) {
    if (width == width_ && height == height_) {
        return;
    }

    width_ = width;
    height_ = height;

#ifdef _WIN32
    if (spout_ && initialized_) {
        spout_->UpdateSender(senderName_.c_str(), width_, height_);
    }
#endif
}

} // namespace LiveText