#pragma once

#ifdef _WIN32
#include "SpoutGL.h"
#endif

#include <string>
#include <memory>

namespace LiveText {

class SpoutSender {
public:
    SpoutSender();
    ~SpoutSender();

    bool initialize(const std::string& senderName, int width, int height);
    void shutdown();

    bool sendTexture(unsigned int textureID, int width, int height);
    bool isInitialized() const { return initialized_; }

    void setSize(int width, int height);

private:
    bool initialized_;
    std::string senderName_;
    int width_, height_;

#ifdef _WIN32
    std::unique_ptr<spoutGL> spout_;
#endif
};

} // namespace LiveText