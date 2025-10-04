#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <functional>

#ifdef ENABLE_NDI
#include <Processing.NDI.Lib.h>
#else
// Forward declarations when NDI is disabled
typedef void* NDIlib_find_instance_t;
typedef void* NDIlib_recv_instance_t;
#endif

namespace LiveText {

struct NDIFrame {
    uint8_t* data;
    int width;
    int height;
    int stride;
    uint64_t timestamp;
};

class NDIReceiver {
public:
    NDIReceiver();
    ~NDIReceiver();

    // Initialize NDI library and find sources
    bool initialize();

    // Connect to a specific NDI source by name (empty = first available)
    bool connect(const std::string& sourceName = "");

    // Get list of available NDI sources
    std::vector<std::string> getAvailableSources();

    // Capture the latest video frame (non-blocking)
    // Returns nullptr if no new frame available
    NDIFrame* captureFrame();

    // Release a captured frame back to NDI
    void releaseFrame(NDIFrame* frame);

    // Check if connected to a source
    bool isConnected() const { return connected_; }

    // Get current source name
    std::string getSourceName() const { return currentSourceName_; }

    // Disconnect from current source
    void disconnect();

    // Shutdown NDI library
    void shutdown();

private:
    NDIlib_find_instance_t ndiFind_;
    NDIlib_recv_instance_t ndiRecv_;
    bool initialized_;
    bool connected_;
    std::string currentSourceName_;

    // Frame data storage
    NDIFrame currentFrame_;
};

} // namespace LiveText
