#include "ReceiverApp.h"
#include <GL/gl3w.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <sstream>
#include <vector>

namespace LiveText {

ReceiverApp::ReceiverApp()
    : window_(nullptr)
    , running_(false)
    , currentSize_(TextSize::SMALL)
    , needsClearOldOutput_(false)
    , outputToClear_(TextSize::SMALL)
    , needsClearInactiveAfterFade_(false)
    , isFading_(false)
    , fadeAlpha_(1.0f)
    , blankTexture_(0)
{
}

ReceiverApp::~ReceiverApp() {
    shutdown();
}

bool ReceiverApp::initialize() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    // Create window (hidden for headless rendering)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE); // Visible window for GUI display

    window_ = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Live Text Receiver", nullptr, nullptr);
    if (!window_) {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1); // Enable vsync

    // Set callbacks
    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, framebufferSizeCallback);
    glfwSetKeyCallback(window_, keyCallback);

    // OpenGL context already initialized by GLFW

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr; // Disable .ini file

    // Load fonts - match sender's font configuration
    std::string fontPath = "fonts/ABF.ttf";

    // Add default font first (system font)
    io.Fonts->AddFontDefault();

    // Add ABF font at small size (48px native) - index 1
    io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 48.0f);

    // Add ABF font at big size (160px native) - index 2
    io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 160.0f);

    // Setup ImGui style
    ImGui::StyleColorsDark();

    // Initialize ImGui backends
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    std::cout << "ImGui initialized for receiver GUI with ABF font" << std::endl;

    // Initialize components
    healthMonitor_ = std::make_unique<HealthMonitor>();

    // Initialize text renderer at 4K resolution for Syphon outputs
    textRenderer_ = std::make_unique<TextRenderer>();
    if (!textRenderer_->initialize(SYPHON_WIDTH, SYPHON_HEIGHT)) {
        std::cerr << "Failed to initialize text renderer" << std::endl;
        return false;
    }

    // Initialize texture senders (Spout on Windows, Syphon on macOS) at 4K resolution
    // Small text output
    textureSenderSmall_ = std::make_unique<TextureSender>();
    if (!textureSenderSmall_->initialize("LiveText-Small", SYPHON_WIDTH, SYPHON_HEIGHT)) {
        std::cerr << "Failed to initialize small texture sender" << std::endl;
        return false;
    }
    std::cout << "Small text output initialized (4K): " << textureSenderSmall_->getPlatformInfo() << std::endl;

    // Big text output
    textureSenderBig_ = std::make_unique<TextureSender>();
    if (!textureSenderBig_->initialize("LiveText-Big", SYPHON_WIDTH, SYPHON_HEIGHT)) {
        std::cerr << "Failed to initialize big texture sender" << std::endl;
        return false;
    }
    std::cout << "Big text output initialized (4K): " << textureSenderBig_->getPlatformInfo() << std::endl;

    // Create a blank texture for clearing inactive outputs
    glGenTextures(1, &blankTexture_);
    glBindTexture(GL_TEXTURE_2D, blankTexture_);
    std::vector<unsigned char> blankData(SYPHON_WIDTH * SYPHON_HEIGHT * 4, 0);  // RGBA all zeros (transparent black)
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, SYPHON_WIDTH, SYPHON_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, blankData.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    std::cout << "Blank texture created for clearing inactive outputs" << std::endl;

    // Initialize Aeron subscriber
    std::vector<std::string> channels = {PRIMARY_CHANNEL, SECONDARY_CHANNEL};
    subscriber_ = std::make_unique<AeronSubscriber>(channels, STREAM_ID);

    if (!subscriber_->initialize()) {
        std::cerr << "Failed to initialize Aeron subscriber" << std::endl;
        return false;
    }

    // Set message callback
    subscriber_->setMessageCallback([this](const TextMessage& message, int feedId) {
        onMessageReceived(message, feedId);
    });

    // Start subscriber
    subscriber_->start();

    running_ = true;
    std::cout << "Live Text Receiver initialized successfully" << std::endl;
    std::cout << "Listening for messages on dual Aeron feeds..." << std::endl;

    return true;
}

void ReceiverApp::run() {
    while (running_ && !glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        // Update health monitoring and fade animation
        updateHealthMonitoring();
        updateFade();

        // Render frame
        render();

        // Small delay to prevent excessive CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }
}

void ReceiverApp::shutdown() {
    running_ = false;

    if (subscriber_) {
        subscriber_->shutdown();
        subscriber_.reset();
    }

    if (textureSenderSmall_) {
        textureSenderSmall_->shutdown();
        textureSenderSmall_.reset();
    }

    if (textureSenderBig_) {
        textureSenderBig_->shutdown();
        textureSenderBig_.reset();
    }

    if (textRenderer_) {
        textRenderer_->shutdown();
        textRenderer_.reset();
    }

    if (blankTexture_) {
        glDeleteTextures(1, &blankTexture_);
        blankTexture_ = 0;
    }

    healthMonitor_.reset();

    // Shutdown ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (window_) {
        glfwDestroyWindow(window_);
        glfwTerminate();
        window_ = nullptr;
    }
}

void ReceiverApp::onMessageReceived(const TextMessage& message, int feedId) {
    std::cout << "RECEIVER_DEBUG: Message received, type=" << static_cast<int>(message.type)
              << ", feedId=" << feedId << std::endl;

    switch (message.type) {
        case MessageType::TEXT_UPDATE:
            {
                // Check if size changed - if so, flag that we need to clear the old output
                TextSize previousSize = currentSize_;
                currentText_ = message.getText();
                currentSize_ = message.size;

                // If size changed, mark that we need to clear the old output
                if (previousSize != currentSize_) {
                    needsClearOldOutput_ = true;
                    outputToClear_ = previousSize;
                    std::cout << "RECEIVER_DEBUG: Size changed from " << (previousSize == TextSize::SMALL ? "SMALL" : "BIG")
                              << " to " << (currentSize_ == TextSize::SMALL ? "SMALL" : "BIG") << std::endl;
                }

                // Update with the new text and size
                textRenderer_->updateText(currentText_, currentSize_);
                std::cout << "RECEIVER_DEBUG: TEXT_UPDATE - text='" << currentText_ << "'" << std::endl;
            }
            break;

        case MessageType::CLEAR_TEXT:
            std::cout << "RECEIVER_DEBUG: CLEAR_TEXT received - starting fade out" << std::endl;
            if (!currentText_.empty()) {
                // Start fade out animation
                fadingText_ = currentText_;
                isFading_ = true;
                fadeAlpha_ = 1.0f;
                fadeStartTime_ = std::chrono::steady_clock::now();
                // Mark that we need to clear the inactive output after fade completes
                needsClearInactiveAfterFade_ = true;
            }
            textRenderer_->clearText();
            currentText_.clear();
            break;

        case MessageType::HEARTBEAT:
            // Heartbeats are handled automatically in the subscriber
            break;
    }
}

void ReceiverApp::updateHealthMonitoring() {
    if (subscriber_) {
        auto stats = subscriber_->getStats();

        for (size_t i = 0; i < stats.size(); ++i) {
            const auto& stat = stats[i];
            std::string connectionName = (i == 0) ? "Primary Feed" : "Secondary Feed";

            healthMonitor_->updateConnectionStatus(connectionName, stat.isConnected, stat.lastError);

            if (stat.isConnected) {
                healthMonitor_->updateMetric(connectionName + " Messages",
                                           static_cast<double>(stat.messagesReceived),
                                           HealthStatus::HEALTHY);
            }
        }

        // Overall subscriber health
        bool subscriberHealthy = subscriber_->isHealthy();
        int activeFeed = subscriber_->getActiveFeed();

        healthMonitor_->updateMetric("Subscriber Health", subscriberHealthy ? 1.0 : 0.0,
                                   subscriberHealthy ? HealthStatus::HEALTHY : HealthStatus::CRITICAL);

        if (activeFeed >= 0) {
            healthMonitor_->updateMetric("Active Feed", static_cast<double>(activeFeed),
                                       HealthStatus::HEALTHY,
                                       activeFeed == 0 ? "Primary" : "Secondary");
        }
    }

    // Text renderer health
    if (textRenderer_) {
        healthMonitor_->updateMetric("Text Renderer", 1.0, HealthStatus::HEALTHY, "Operational");
    }

    // Texture sender health
    if (textureSenderSmall_) {
        bool senderHealthy = textureSenderSmall_->isInitialized();
        std::string senderName = "Small Text Output (" + textureSenderSmall_->getPlatformInfo() + ")";
        healthMonitor_->updateMetric(senderName, senderHealthy ? 1.0 : 0.0,
                                   senderHealthy ? HealthStatus::HEALTHY : HealthStatus::CRITICAL);
    }

    if (textureSenderBig_) {
        bool senderHealthy = textureSenderBig_->isInitialized();
        std::string senderName = "Big Text Output (" + textureSenderBig_->getPlatformInfo() + ")";
        healthMonitor_->updateMetric(senderName, senderHealthy ? 1.0 : 0.0,
                                   senderHealthy ? HealthStatus::HEALTHY : HealthStatus::CRITICAL);
    }
}

void ReceiverApp::updateFade() {
    if (isFading_) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration<float>(now - fadeStartTime_).count();

        if (elapsed >= FADE_DURATION_SECONDS) {
            // Fade complete
            isFading_ = false;
            fadeAlpha_ = 0.0f;
            fadingText_.clear();

            // If we need to clear the inactive output, mark it
            if (needsClearInactiveAfterFade_) {
                TextSize inactiveSize = (currentSize_ == TextSize::SMALL) ? TextSize::BIG : TextSize::SMALL;
                needsClearOldOutput_ = true;
                outputToClear_ = inactiveSize;
                needsClearInactiveAfterFade_ = false;
            }
        } else {
            // Calculate fade alpha (1.0 to 0.0)
            fadeAlpha_ = 1.0f - (elapsed / FADE_DURATION_SECONDS);
        }
    }
}

void ReceiverApp::render() {
    std::cout << "ReceiverApp::render - currentSize=" << (currentSize_ == TextSize::SMALL ? "SMALL" : "BIG")
              << ", currentText='" << currentText_ << "'" << std::endl;

    // Render text with current size (TextRenderer handles fade internally)
    textRenderer_->render();

    // Get the rendered texture
    GLuint textureID = textRenderer_->getRenderedTexture();
    std::cout << "ReceiverApp::render - textureID=" << textureID << std::endl;

    // Always send to both outputs - active gets rendered text, inactive gets blank
    glFinish();

    if (currentSize_ == TextSize::SMALL) {
        // Small mode: send rendered text to small output, blank to big output
        std::cout << "ReceiverApp::render - sending rendered text to SMALL output, blank to BIG output" << std::endl;
        if (textureID > 0) {
            textureSenderSmall_->sendTexture(textureID, SYPHON_WIDTH, SYPHON_HEIGHT);
        }
        textureSenderBig_->sendTexture(blankTexture_, SYPHON_WIDTH, SYPHON_HEIGHT);
    } else {
        // Big mode: send rendered text to big output, blank to small output
        std::cout << "ReceiverApp::render - sending rendered text to BIG output, blank to SMALL output" << std::endl;
        if (textureID > 0) {
            textureSenderBig_->sendTexture(textureID, SYPHON_WIDTH, SYPHON_HEIGHT);
        }
        textureSenderSmall_->sendTexture(blankTexture_, SYPHON_WIDTH, SYPHON_HEIGHT);
    }

    // Render ImGui interface
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Start ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Create main window
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(WINDOW_WIDTH, WINDOW_HEIGHT));
    ImGui::Begin("Live Text Receiver", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);

    // Display current text (large and centered like sender)
    // Determine what text to display and with what alpha
    std::string textToDisplay;
    float alpha = 1.0f;
    TextSize sizeToUse = currentSize_;

    if (!currentText_.empty()) {
        // Display current text at full opacity
        textToDisplay = currentText_;
        alpha = 1.0f;
    } else if (isFading_ && !fadingText_.empty()) {
        // Display fading text with fade alpha
        textToDisplay = fadingText_;
        alpha = fadeAlpha_;
    }
    // If no current text and not fading, display nothing (blank screen)

    std::cout << "ReceiverApp::render - IMGUI DISPLAY - textToDisplay='" << textToDisplay
              << "', alpha=" << alpha << ", currentText_='" << currentText_ << "'" << std::endl;

    if (!textToDisplay.empty()) {
        ImGuiIO& io = ImGui::GetIO();

        // Get the appropriate ABF font (no scaling needed - using native font sizes)
        ImFont* abfFont = (sizeToUse == TextSize::BIG) ? io.Fonts->Fonts[2] : io.Fonts->Fonts[1];

        ImGui::PushFont(abfFont);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

        // Apply alpha for fade effect
        if (alpha < 1.0f) {
            ImVec4 textColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);
            textColor.w = alpha;
            ImGui::PushStyleColor(ImGuiCol_Text, textColor);
        }

        // Split text into lines for per-line centering like the sender
        std::vector<std::string> lines;
        std::stringstream ss(textToDisplay);
        std::string line;
        while (std::getline(ss, line)) {
            lines.push_back(line);
        }

        // Calculate total text height
        float lineHeight = ImGui::GetTextLineHeight();
        float totalTextHeight = lines.size() * lineHeight;

        // Get available content region (ImGui window content area)
        ImVec2 contentRegion = ImGui::GetContentRegionAvail();
        float startY = (contentRegion.y - totalTextHeight) * 0.5f;

        // Render each line centered horizontally
        for (size_t i = 0; i < lines.size(); ++i) {
            float lineWidth = ImGui::CalcTextSize(lines[i].c_str()).x;
            float centerX = (contentRegion.x - lineWidth) * 0.5f;

            ImGui::SetCursorPosX(centerX);
            ImGui::SetCursorPosY(startY + i * lineHeight);
            ImGui::TextUnformatted(lines[i].c_str());
        }

        // Pop alpha style if applied
        if (alpha < 1.0f) {
            ImGui::PopStyleColor();
        }

        ImGui::PopStyleVar();
        ImGui::PopFont();
    }
    // No else clause - blank screen when no text to display

    // Show network status with traffic lights at the bottom
    ImGui::SetCursorPosY(WINDOW_HEIGHT - 100);
    ImGui::Separator();

    // Network Traffic Lights and Statistics
    ImGui::Text("Network Status");

    if (subscriber_) {
        auto stats = subscriber_->getStats();

        // Display traffic lights for each feed
        for (size_t i = 0; i < stats.size(); ++i) {
            const auto& stat = stats[i];

            // Traffic light square
            ImVec4 lightColor;
            const char* statusText;
            if (stat.isConnected && !stat.hasErrors) {
                lightColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // Green
                statusText = "RECEIVING";
            } else if (stat.isConnected && stat.hasErrors) {
                lightColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow
                statusText = "ISSUES";
            } else {
                lightColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
                statusText = "OFFLINE";
            }

            // Draw traffic light square
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImVec2 pos = ImGui::GetCursorScreenPos();
            float square_size = 12.0f;
            draw_list->AddRectFilled(pos, ImVec2(pos.x + square_size, pos.y + square_size),
                                   ImGui::ColorConvertFloat4ToU32(lightColor));
            draw_list->AddRect(pos, ImVec2(pos.x + square_size, pos.y + square_size),
                             ImGui::ColorConvertFloat4ToU32(ImVec4(0.3f, 0.3f, 0.3f, 1.0f)));

            // Move cursor past the square
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + square_size + 6.0f);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);

            // Feed label and status (compact for bottom display)
            ImGui::TextColored(lightColor, "Feed %zu: %s", i + 1, statusText);

            // Statistics on same line (compact)
            ImGui::SameLine();
            ImGui::Text("| RX: %lu msg", stat.messagesReceived);

            if (stat.hasErrors) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "| ERR!");
            }
        }

        // Active feed and text size info
        ImGui::Text("Active Feed: %d | Text Size: %s",
                   subscriber_->getActiveFeed() + 1,
                   (currentSize_ == TextSize::BIG) ? "Large" : "Small");

        // Overall statistics (compact)
        uint64_t totalReceived = 0, totalBytes = 0;
        for (const auto& stat : stats) {
            totalReceived += stat.messagesReceived;
            totalBytes += stat.bytesReceived;
        }

        ImGui::SameLine();
        ImGui::Text("| Total: %lu msg, %.1f KB", totalReceived, totalBytes / 1024.0);

    } else {
        // No network connection
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        float square_size = 12.0f;
        draw_list->AddRectFilled(pos, ImVec2(pos.x + square_size, pos.y + square_size),
                               ImGui::ColorConvertFloat4ToU32(ImVec4(0.5f, 0.5f, 0.5f, 1.0f)));
        draw_list->AddRect(pos, ImVec2(pos.x + square_size, pos.y + square_size),
                         ImGui::ColorConvertFloat4ToU32(ImVec4(0.3f, 0.3f, 0.3f, 1.0f)));

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + square_size + 6.0f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Network: NOT INITIALIZED");

        ImGui::SameLine();
        ImGui::TextDisabled("| Text Size: %s", (currentSize_ == TextSize::BIG) ? "Large" : "Small");
    }

    ImGui::End();

    // Render ImGui
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window_);
}

// Static callbacks
void ReceiverApp::framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    ReceiverApp* app = static_cast<ReceiverApp*>(glfwGetWindowUserPointer(window));
    if (app && app->textRenderer_) {
        // Note: Keep Syphon outputs and text renderer at 4K regardless of window size
        // Only the display window is Full HD
    }
}

void ReceiverApp::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    ReceiverApp* app = static_cast<ReceiverApp*>(glfwGetWindowUserPointer(window));

    if (action == GLFW_PRESS) {
        switch (key) {
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(window, GLFW_TRUE);
                break;
            case GLFW_KEY_H:
                // Print health status to console
                if (app && app->healthMonitor_) {
                    std::cout << "\n=== Health Status ===" << std::endl;
                    std::cout << app->healthMonitor_->getDetailedReport() << std::endl;
                }
                break;
            case GLFW_KEY_S:
                // Print subscriber stats
                if (app && app->subscriber_) {
                    auto stats = app->subscriber_->getStats();
                    std::cout << "\n=== Subscriber Stats ===" << std::endl;
                    for (size_t i = 0; i < stats.size(); ++i) {
                        const auto& stat = stats[i];
                        std::cout << "Feed " << i << ": " <<
                                    (stat.isConnected ? "CONNECTED" : "DISCONNECTED") <<
                                    ", Messages: " << stat.messagesReceived <<
                                    ", Bytes: " << stat.bytesReceived << std::endl;
                    }
                    std::cout << "Active Feed: " << app->subscriber_->getActiveFeed() << std::endl;
                }
                break;
        }
    }
}

} // namespace LiveText