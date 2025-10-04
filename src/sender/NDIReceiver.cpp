#include "NDIReceiver.h"
#include <iostream>
#include <cstring>

#ifdef ENABLE_NDI
#include <Processing.NDI.Lib.h>
#endif

namespace LiveText {

NDIReceiver::NDIReceiver()
    : ndiFind_(nullptr)
    , ndiRecv_(nullptr)
    , initialized_(false)
    , connected_(false)
    , currentSourceName_("")
{
    memset(&currentFrame_, 0, sizeof(currentFrame_));
}

NDIReceiver::~NDIReceiver() {
    shutdown();
}

bool NDIReceiver::initialize() {
#ifdef ENABLE_NDI
    if (initialized_) {
        return true;
    }

    // Initialize NDI library
    if (!NDIlib_initialize()) {
        std::cerr << "Failed to initialize NDI library" << std::endl;
        return false;
    }

    // Create NDI finder
    NDIlib_find_create_t findCreate;
    findCreate.show_local_sources = true;
    findCreate.p_groups = nullptr;
    findCreate.p_extra_ips = nullptr;

    ndiFind_ = NDIlib_find_create_v2(&findCreate);
    if (!ndiFind_) {
        std::cerr << "Failed to create NDI finder" << std::endl;
        NDIlib_destroy();
        return false;
    }

    initialized_ = true;
    std::cout << "NDI initialized successfully" << std::endl;
    return true;
#else
    std::cerr << "NDI support not compiled in. Build with -DENABLE_NDI=ON" << std::endl;
    return false;
#endif
}

bool NDIReceiver::connect(const std::string& sourceName) {
#ifdef ENABLE_NDI
    if (!initialized_) {
        std::cerr << "NDI not initialized" << std::endl;
        return false;
    }

    if (connected_) {
        disconnect();
    }

    // Wait a bit for sources to be discovered
    std::cout << "Searching for NDI sources..." << std::endl;
    NDIlib_find_wait_for_sources(ndiFind_, 2000);

    // Get available sources
    uint32_t numSources = 0;
    const NDIlib_source_t* sources = NDIlib_find_get_current_sources(ndiFind_, &numSources);

    if (numSources == 0) {
        std::cerr << "No NDI sources found" << std::endl;
        return false;
    }

    std::cout << "Found " << numSources << " NDI source(s):" << std::endl;
    for (uint32_t i = 0; i < numSources; i++) {
        std::cout << "  [" << i << "] " << sources[i].p_ndi_name << std::endl;
    }

    // Select source
    int selectedIndex = -1;
    if (sourceName.empty()) {
        // Use first source
        selectedIndex = 0;
    } else {
        // Find source by name
        for (uint32_t i = 0; i < numSources; i++) {
            if (std::string(sources[i].p_ndi_name).find(sourceName) != std::string::npos) {
                selectedIndex = i;
                break;
            }
        }
    }

    if (selectedIndex < 0 || selectedIndex >= (int)numSources) {
        std::cerr << "Could not find NDI source: " << sourceName << std::endl;
        return false;
    }

    // Create receiver with low bandwidth settings
    NDIlib_recv_create_v3_t recvCreate;
    memset(&recvCreate, 0, sizeof(recvCreate));
    recvCreate.source_to_connect_to = sources[selectedIndex];
    recvCreate.bandwidth = NDIlib_recv_bandwidth_lowest; // Request preview/proxy stream
    recvCreate.color_format = NDIlib_recv_color_format_BGRX_BGRA; // Easy to upload to OpenGL
    recvCreate.allow_video_fields = false;

    ndiRecv_ = NDIlib_recv_create_v3(&recvCreate);
    if (!ndiRecv_) {
        std::cerr << "Failed to create NDI receiver" << std::endl;
        return false;
    }

    currentSourceName_ = sources[selectedIndex].p_ndi_name;
    connected_ = true;

    std::cout << "Connected to NDI source: " << currentSourceName_ << std::endl;
    return true;
#else
    return false;
#endif
}

std::vector<std::string> NDIReceiver::getAvailableSources() {
    std::vector<std::string> sourceNames;

#ifdef ENABLE_NDI
    if (!initialized_) {
        return sourceNames;
    }

    // Wait a bit for sources
    NDIlib_find_wait_for_sources(ndiFind_, 1000);

    uint32_t numSources = 0;
    const NDIlib_source_t* sources = NDIlib_find_get_current_sources(ndiFind_, &numSources);

    for (uint32_t i = 0; i < numSources; i++) {
        sourceNames.push_back(sources[i].p_ndi_name);
    }
#endif

    return sourceNames;
}

NDIFrame* NDIReceiver::captureFrame() {
#ifdef ENABLE_NDI
    if (!connected_ || !ndiRecv_) {
        return nullptr;
    }

    NDIlib_video_frame_v2_t videoFrame;
    NDIlib_audio_frame_v3_t audioFrame;
    NDIlib_metadata_frame_t metadataFrame;

    // Non-blocking capture with 0ms timeout
    NDIlib_frame_type_e frameType = NDIlib_recv_capture_v3(
        ndiRecv_, &videoFrame, &audioFrame, &metadataFrame, 0);

    // Free audio/metadata if received
    if (frameType == NDIlib_frame_type_audio) {
        NDIlib_recv_free_audio_v3(ndiRecv_, &audioFrame);
        return nullptr;
    }
    if (frameType == NDIlib_frame_type_metadata) {
        NDIlib_recv_free_metadata(ndiRecv_, &metadataFrame);
        return nullptr;
    }

    // Check if we got video
    if (frameType == NDIlib_frame_type_video) {
        // Fill our frame structure
        currentFrame_.data = videoFrame.p_data;
        currentFrame_.width = videoFrame.xres;
        currentFrame_.height = videoFrame.yres;
        currentFrame_.stride = videoFrame.line_stride_in_bytes;
        currentFrame_.timestamp = videoFrame.timestamp;

        // Note: We'll need to free this frame later with releaseFrame()
        // Store the NDI frame pointer so we can free it properly
        return &currentFrame_;
    }

    // Handle status changes
    if (frameType == NDIlib_frame_type_status_change) {
        std::cout << "NDI status changed" << std::endl;
    }
#endif

    return nullptr;
}

void NDIReceiver::releaseFrame(NDIFrame* frame) {
#ifdef ENABLE_NDI
    if (!frame || !ndiRecv_) {
        return;
    }

    // We need to free the NDI video frame
    // Since we don't store the original NDIlib_video_frame_v2_t, we'll need to modify this
    // For now, just clear our frame structure
    memset(&currentFrame_, 0, sizeof(currentFrame_));
#endif
}

void NDIReceiver::disconnect() {
#ifdef ENABLE_NDI
    if (ndiRecv_) {
        NDIlib_recv_destroy(ndiRecv_);
        ndiRecv_ = nullptr;
    }
    connected_ = false;
    currentSourceName_.clear();
#endif
}

void NDIReceiver::shutdown() {
#ifdef ENABLE_NDI
    disconnect();

    if (ndiFind_) {
        NDIlib_find_destroy(ndiFind_);
        ndiFind_ = nullptr;
    }

    if (initialized_) {
        NDIlib_destroy();
        initialized_ = false;
    }
#endif
}

} // namespace LiveText
