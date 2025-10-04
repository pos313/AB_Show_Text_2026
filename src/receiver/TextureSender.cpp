#include "TextureSender.h"
#include <iostream>

namespace LiveText {

TextureSender::TextureSender()
    : width_(0)
    , height_(0)
{
#ifdef _WIN32
    spoutSender_ = std::make_unique<SpoutSender>();
#elif __APPLE__
    syphonSender_ = std::make_unique<SyphonSender>();
#endif
}

TextureSender::~TextureSender() {
    shutdown();
}

bool TextureSender::initialize(const std::string& senderName, int width, int height) {
    senderName_ = senderName;
    width_ = width;
    height_ = height;

#ifdef _WIN32
    if (spoutSender_) {
        return spoutSender_->initialize(senderName, width, height);
    }
#elif __APPLE__
    if (syphonSender_) {
        return syphonSender_->initialize(senderName, width, height);
    }
#else
    std::cout << "Texture sharing not available on this platform" << std::endl;
    return true; // Allow app to run without texture sharing
#endif

    return false;
}

void TextureSender::shutdown() {
#ifdef _WIN32
    if (spoutSender_) {
        spoutSender_->shutdown();
    }
#elif __APPLE__
    if (syphonSender_) {
        syphonSender_->shutdown();
    }
#endif
}

bool TextureSender::sendTexture(unsigned int textureID, int width, int height) {
#ifdef _WIN32
    if (spoutSender_) {
        return spoutSender_->sendTexture(textureID, width, height);
    }
#elif __APPLE__
    if (syphonSender_) {
        return syphonSender_->sendTexture(textureID, width, height);
    }
#else
    // On platforms without texture sharing, just return success
    return true;
#endif

    return false;
}

bool TextureSender::isInitialized() const {
#ifdef _WIN32
    return spoutSender_ && spoutSender_->isInitialized();
#elif __APPLE__
    return syphonSender_ && syphonSender_->isInitialized();
#else
    return true; // Always "initialized" on platforms without texture sharing
#endif
}

void TextureSender::setSize(int width, int height) {
    if (width == width_ && height == height_) {
        return;
    }

    width_ = width;
    height_ = height;

#ifdef _WIN32
    if (spoutSender_) {
        spoutSender_->setSize(width, height);
    }
#elif __APPLE__
    if (syphonSender_) {
        syphonSender_->setSize(width, height);
    }
#endif
}

std::string TextureSender::getPlatformInfo() const {
#ifdef _WIN32
    return "Windows Spout";
#elif __APPLE__
    return "macOS Syphon";
#else
    return "No texture sharing";
#endif
}

} // namespace LiveText