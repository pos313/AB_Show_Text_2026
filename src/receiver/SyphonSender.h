#pragma once

#include <string>
#include <memory>

#ifdef __APPLE__
// Use void pointers to avoid Objective-C in C++ header
typedef void* SyphonServerPtr;
typedef void* NSOpenGLContextPtr;
#endif

namespace LiveText {

class SyphonSender {
public:
    SyphonSender();
    ~SyphonSender();

    bool initialize(const std::string& senderName, int width, int height);
    void shutdown();

    bool sendTexture(unsigned int textureID, int width, int height);
    bool isInitialized() const { return initialized_; }

    void setSize(int width, int height);

private:
    bool initialized_;
    std::string senderName_;
    int width_, height_;

#ifdef __APPLE__
    SyphonServerPtr syphonServer_;
    NSOpenGLContextPtr context_;
#endif
};

} // namespace LiveText