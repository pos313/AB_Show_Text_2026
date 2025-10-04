#pragma once
#include <GLFW/glfw3.h>
#include <memory>
#include <string>
#include <atomic>
#include <chrono>
#include "common/AeronConnection.h"
#include "common/HealthMonitor.h"
#include "common/TextMessage.h"
#include "TextMemory.h"
#include "NDIReceiver.h"

// Forward declarations for ImGui
struct ImGuiInputTextCallbackData;
struct ImVec2;

namespace LiveText {

class SenderApp {
public:
    SenderApp();
    ~SenderApp();

    bool initialize();
    void run();
    void shutdown();

private:
    // GLFW and ImGui
    GLFWwindow* window_;

    // Aeron communication
    std::unique_ptr<DualAeronPublisher> publisher_;

    // Health monitoring
    std::unique_ptr<HealthMonitor> healthMonitor_;

    // Text memory
    std::unique_ptr<TextMemory> textMemory_;

    // NDI video receiver
    std::unique_ptr<NDIReceiver> ndiReceiver_;
    GLuint ndiTexture_;
    int ndiTextureWidth_;
    int ndiTextureHeight_;

    // UI state
    char textBuffer_[512];
    TextSize currentTextSize_;
    bool autoSendEnabled_;
    bool showCharacterCount_;
    bool showKeyboardShortcuts_;

    // Input validation state
    bool textTooLong_;
    std::string validationMessage_;

    // Application state
    std::atomic<bool> running_;
    std::string lastSentText_;
    std::string previousTextBuffer_;  // For manual text change detection

    // Cursor/selection tracking for centered text
    int cursorPos_;
    int selectionStart_;
    int selectionEnd_;

    // Fade out animation state
    bool isFading_;
    float fadeAlpha_;
    std::chrono::steady_clock::time_point fadeStartTime_;
    std::string fadingText_;


    // UI methods
    void renderMainWindow();
    void renderTextInput();
    void renderControlButtons();
    void renderHealthStatus();
    void renderTextMemory();
    void renderConnectionStatus();
    void renderKeyboardShortcuts();
    void handleKeyboardInput();

    // Validation methods
    void validateTextInput();
    bool isTextValid() const;

    // ImGui callback for text input
    static int TextInputCallback(ImGuiInputTextCallbackData* data);

    // Action methods
    void sendText();
    void sendCurrentText();
    void clearText();
    void switchTextSize(TextSize size);

    // Style methods
    void setupDarkTheme();
    void updateHealthMonitoring();
    void updateFade();

    // NDI methods
    bool initializeNDI();
    void updateNDITexture();
    void renderNDIBackground(const ImVec2& pos, const ImVec2& size);
    void shutdownNDI();

    // Configuration
    static constexpr int WINDOW_WIDTH = 1200;
    static constexpr int WINDOW_HEIGHT = 800;
    static constexpr float SMALL_TEXT_SIZE = 18.0f;
    static constexpr float BIG_TEXT_SIZE = 64.0f;
    static constexpr float FADE_DURATION_SECONDS = 2.0f;

    // Aeron configuration - using localhost unicast instead of multicast
    static constexpr const char* PRIMARY_CHANNEL = "aeron:udp?endpoint=127.0.0.1:9999";
    static constexpr const char* SECONDARY_CHANNEL = "aeron:udp?endpoint=127.0.0.1:9998";
    static constexpr int STREAM_ID = 1001;
};

} // namespace LiveText