#pragma once
#include <string>
#include <memory>

#include "SpoutSender.h"
#include "SyphonSender.h"

namespace LiveText {

class TextureSender {
public:
    TextureSender();
    ~TextureSender();

    bool initialize(const std::string& senderName, int width, int height);
    void shutdown();

    bool sendTexture(unsigned int textureID, int width, int height);
    bool isInitialized() const;

    void setSize(int width, int height);
    std::string getPlatformInfo() const;

private:
    std::string senderName_;
    int width_, height_;

#ifdef _WIN32
    std::unique_ptr<SpoutSender> spoutSender_;
#elif __APPLE__
    std::unique_ptr<SyphonSender> syphonSender_;
#else
    // Linux/other platforms - no texture sharing
#endif
};

} // namespace LiveText