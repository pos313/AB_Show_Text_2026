#include "SyphonSender.h"
#include <iostream>

#ifdef __APPLE__
// Only try to include Syphon if it's available
#if __has_include(<Syphon/Syphon.h>)
#import <Syphon/SyphonOpenGLServer.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl3.h>
#define SYPHON_AVAILABLE
#endif
#endif

namespace LiveText {

SyphonSender::SyphonSender()
    : initialized_(false)
    , width_(0)
    , height_(0)
#ifdef __APPLE__
    , syphonServer_(nullptr)
    , context_(nullptr)
#endif
{
}

SyphonSender::~SyphonSender() {
    shutdown();
}

bool SyphonSender::initialize(const std::string& senderName, int width, int height) {
    senderName_ = senderName;
    width_ = width;
    height_ = height;

#ifdef SYPHON_AVAILABLE
    @autoreleasepool {
        // Get the current CGL context directly
        CGLContextObj cglContext = CGLGetCurrentContext();
        if (!cglContext) {
            std::cerr << "No current OpenGL context for Syphon" << std::endl;
            return false;
        }

        // Create Syphon server
        NSString* name = [NSString stringWithUTF8String:senderName_.c_str()];
        SyphonOpenGLServer* server = [[SyphonOpenGLServer alloc] initWithName:name
                                                                      context:cglContext
                                                                      options:nil];
        syphonServer_ = (__bridge void*)server;

        if (!server) {
            std::cerr << "Failed to create Syphon server: " << senderName_ << std::endl;
            return false;
        }

        std::cout << "Syphon server initialized: " << senderName_ << " (" << width_ << "x" << height_ << ")" << std::endl;
        initialized_ = true;
        return true;
    }
#else
    std::cout << "Syphon not available - running without texture sharing (for GUI/rendering testing)" << std::endl;
    initialized_ = true; // Allow the app to run for testing
    return true;
#endif
}

void SyphonSender::shutdown() {
#ifdef SYPHON_AVAILABLE
    @autoreleasepool {
        if (syphonServer_) {
            SyphonOpenGLServer* server = (__bridge SyphonOpenGLServer*)syphonServer_;
            [server stop];
            syphonServer_ = nullptr;
        }
    }
#endif
    initialized_ = false;
}

bool SyphonSender::sendTexture(unsigned int textureID, int width, int height) {
    if (!initialized_) {
        return false;
    }

#ifdef SYPHON_AVAILABLE
    @autoreleasepool {
        if (!syphonServer_) {
            return false;
        }

        // Update server size if changed
        if (width != width_ || height != height_) {
            setSize(width, height);
        }

        // Create texture rect
        NSRect textureRect = NSMakeRect(0, 0, width_, height_);
        NSSize textureSize = NSMakeSize(width_, height_);

        // Publish the texture
        SyphonOpenGLServer* server = (__bridge SyphonOpenGLServer*)syphonServer_;
        [server publishFrameTexture:textureID
                       textureTarget:GL_TEXTURE_2D
                         imageRegion:textureRect
                   textureDimensions:textureSize
                             flipped:NO];

        return true;
    }
#else
    // Testing mode - just return true
    return true;
#endif
}

void SyphonSender::setSize(int width, int height) {
    if (width == width_ && height == height_) {
        return;
    }

    width_ = width;
    height_ = height;

#ifdef SYPHON_AVAILABLE
    // Syphon handles size changes automatically through texture dimensions
#endif
}

} // namespace LiveText