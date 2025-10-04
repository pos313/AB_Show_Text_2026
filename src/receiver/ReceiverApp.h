#pragma once
#include <GLFW/glfw3.h>
#include <memory>
#include <atomic>
#include <thread>
#include "common/AeronConnection.h"
#include "common/HealthMonitor.h"
#include "common/TextMessage.h"
#include "TextRenderer.h"
#include "TextureSender.h"

namespace LiveText {

class ReceiverApp {
public:
    ReceiverApp();
    ~ReceiverApp();

    bool initialize();
    void run();
    void shutdown();

private:
    // GLFW and OpenGL
    GLFWwindow* window_;

    // Aeron communication
    std::unique_ptr<AeronSubscriber> subscriber_;

    // Rendering
    std::unique_ptr<TextRenderer> textRenderer_;
    std::unique_ptr<TextureSender> textureSenderSmall_;  // Syphon output for small text
    std::unique_ptr<TextureSender> textureSenderBig_;    // Syphon output for big text
    GLuint blankTexture_;  // Blank texture for clearing inactive output

    // Health monitoring
    std::unique_ptr<HealthMonitor> healthMonitor_;

    // Application state
    std::atomic<bool> running_;
    std::string currentText_;
    TextSize currentSize_;
    bool needsClearOldOutput_;
    TextSize outputToClear_;
    bool needsClearInactiveAfterFade_;

    // Fade out animation state
    bool isFading_;
    float fadeAlpha_;
    std::chrono::steady_clock::time_point fadeStartTime_;
    std::string fadingText_;

    // Callbacks
    void onMessageReceived(const TextMessage& message, int feedId);

    // Update methods
    void updateHealthMonitoring();
    void updateFade();
    void render();

    // Configuration
    static constexpr int WINDOW_WIDTH = 1920;   // Full HD for display window
    static constexpr int WINDOW_HEIGHT = 1080;  // Full HD for display window
    static constexpr int SYPHON_WIDTH = 3840;   // 4K for Syphon outputs
    static constexpr int SYPHON_HEIGHT = 2160;  // 4K for Syphon outputs
    static constexpr const char* TEXTURE_SENDER_NAME = "LiveText";
    static constexpr float FADE_DURATION_SECONDS = 2.0f;

    // Aeron configuration - using localhost unicast instead of multicast
    static constexpr const char* PRIMARY_CHANNEL = "aeron:udp?endpoint=127.0.0.1:9999";
    static constexpr const char* SECONDARY_CHANNEL = "aeron:udp?endpoint=127.0.0.1:9998";
    static constexpr int STREAM_ID = 1001;

    // Window callbacks
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
};

} // namespace LiveText