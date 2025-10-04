#include "SenderApp.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <OpenGL/gl.h>
#include <OpenGL/gl3.h>
#include <iostream>
#include <cstring>
#include <fstream>
#include <vector>
#include <cmath>
#include <sstream>

namespace LiveText {

SenderApp::SenderApp()
    : window_(nullptr)
    , ndiTexture_(0)
    , ndiTextureWidth_(0)
    , ndiTextureHeight_(0)
    , currentTextSize_(TextSize::SMALL)
    , autoSendEnabled_(true)
    , showCharacterCount_(true)
    , showKeyboardShortcuts_(true)
    , textTooLong_(false)
    , running_(false)
    , cursorPos_(0)
    , selectionStart_(-1)
    , selectionEnd_(-1)
    , isFading_(false)
    , fadeAlpha_(0.0f)
{
    memset(textBuffer_, 0, sizeof(textBuffer_));
}

SenderApp::~SenderApp() {
    shutdown();
}

bool SenderApp::initialize() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    // Create window
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window_ = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Live Text Sender", nullptr, nullptr);
    if (!window_) {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1); // Enable vsync

    // OpenGL loader not needed for sender (ImGui handles it)

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // First, explicitly add the default system font as index 0 (for UI elements)
    io.Fonts->AddFontDefault();

    // Then load ABF.ttf font as index 1 (for text input area only)
    std::vector<std::string> fontPaths = {
        "fonts/ABF.ttf",
        "../fonts/ABF.ttf",
        "../../fonts/ABF.ttf"
    };

    bool abfLoaded = false;
    for (const auto& fontPath : fontPaths) {
        if (std::ifstream(fontPath).good()) {
            // Add ABF font at small size (index 1) - larger for better visibility
            io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 48.0f);
            // Add ABF font at big size (index 2) - much larger for stage use
            io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 160.0f);
            std::cout << "Loaded ABF font for text input: " << fontPath << std::endl;
            abfLoaded = true;
            break;
        }
    }

    if (!abfLoaded) {
        std::cout << "ABF.ttf not found, using default ImGui font for text input" << std::endl;
        // Add the default fonts again so we have something at index 1 and 2
        io.Fonts->AddFontDefault();
        io.Fonts->AddFontDefault();
    }

    setupDarkTheme();

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Initialize components
    healthMonitor_ = std::make_unique<HealthMonitor>();
    textMemory_ = std::make_unique<TextMemory>();

    // Initialize Aeron publisher
    publisher_ = std::make_unique<DualAeronPublisher>(PRIMARY_CHANNEL, SECONDARY_CHANNEL, STREAM_ID);

    if (!publisher_->initialize()) {
        std::cerr << "Failed to initialize Aeron publisher" << std::endl;
        return false;
    }

    // Initialize NDI (optional - continues if it fails)
    initializeNDI();

    running_ = true;
    return true;
}

void SenderApp::run() {
    while (running_ && !glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        // Update health monitoring
        updateHealthMonitoring();

        // Check for static text that should be moved to memory
        if (textMemory_) {
            textMemory_->checkForStaticText();
        }

        // Update fade animation
        updateFade();

        // Update NDI texture from latest frame
        updateNDITexture();

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Render main window
        renderMainWindow();

        // Render
        ImGui::Render();
        int displayW, displayH;
        glfwGetFramebufferSize(window_, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window_);
    }
}

void SenderApp::shutdown() {
    running_ = false;

    // Shutdown NDI
    shutdownNDI();

    if (publisher_) {
        publisher_->shutdown();
        publisher_.reset();
    }

    healthMonitor_.reset();
    textMemory_.reset();

    if (window_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(window_);
        glfwTerminate();
        window_ = nullptr;
    }
}

void SenderApp::renderMainWindow() {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

    ImGui::Begin("Live Text Sender", nullptr,
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

    // Title
    ImGui::SetWindowFontScale(1.5f);
    ImGui::Text("Live Text System - Stage Control");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Separator();

    // Main layout: Text input takes most of the space
    float windowWidth = ImGui::GetContentRegionAvail().x;
    float windowHeight = ImGui::GetContentRegionAvail().y;

    // Text input area - 95% width, significant height
    float textInputHeight = windowHeight * 0.4f; // 40% of window height
    renderTextInput();

    ImGui::Spacing();

    // Control buttons in a horizontal row
    renderControlButtons();

    ImGui::Spacing();

    // Text memory takes remaining middle space
    float memoryHeight = windowHeight * 0.3f; // 30% of window height
    ImGui::BeginChild("TextMemoryArea", ImVec2(0, memoryHeight), true);
    renderTextMemory();
    ImGui::EndChild();

    ImGui::Spacing();

    // Bottom row: Health status and keyboard shortcuts side by side
    ImGui::Columns(2, "BottomColumns", false);

    // Left: System Health
    renderHealthStatus();
    ImGui::Spacing();
    renderConnectionStatus();

    ImGui::NextColumn();

    // Right: Keyboard shortcuts
    if (showKeyboardShortcuts_) {
        renderKeyboardShortcuts();
    }

    ImGui::Columns(1);

    ImGui::End();

    // Handle keyboard input
    handleKeyboardInput();
}

void SenderApp::renderTextInput() {
    ImGui::Text("Text Input");

    // Validate input
    validateTextInput();

    // Get window dimensions for 95% width
    float windowWidth = ImGui::GetContentRegionAvail().x;
    float inputWidth = windowWidth * 0.95f;
    float inputHeight = ImGui::GetContentRegionAvail().y * 0.6f; // Use more of available height

    // Get ImGui IO for font access
    ImGuiIO& io = ImGui::GetIO();

    // Declare ABF font variable (will be set below based on text size)
    ImFont* abfFont = nullptr;

    // Style the input field - black background, white text
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Pure black background
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.1f, 0.1f, 0.1f, 1.0f)); // Slightly lighter when hovered
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.05f, 0.05f, 0.05f, 1.0f)); // Very dark gray when active
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f)); // Pure white text

    // Add red tint only for actual errors (too long or invalid chars), not empty text
    bool hasActualError = textTooLong_ || validationMessage_.find("invalid characters") != std::string::npos;
    if (hasActualError) {
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
    } else {
        // Push dummy values to maintain stack balance
        ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_Border));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, ImGui::GetStyle().FrameBorderSize);
    }

    // Minimal padding - text centering is handled by overlay rendering
    float horizontalPadding = 10.0f; // Minimal horizontal padding for cursor visibility
    float verticalPadding = 10.0f; // Minimal vertical padding for cursor visibility
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(horizontalPadding, verticalPadding));

    // Center the input field horizontally
    float centerOffset = (windowWidth - inputWidth) * 0.5f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + centerOffset);

    // Select the correct ABF font based on current text size
    if (currentTextSize_ == TextSize::BIG) {
        // Use big font (index 2)
        if (io.Fonts->Fonts.Size > 2 && io.Fonts->Fonts[2] != nullptr) {
            abfFont = io.Fonts->Fonts[2];
        } else {
            abfFont = io.Fonts->Fonts[0]; // fallback to default
        }
    } else {
        // Use small font (index 1)
        if (io.Fonts->Fonts.Size > 1 && io.Fonts->Fonts[1] != nullptr) {
            abfFont = io.Fonts->Fonts[1];
        } else {
            abfFont = io.Fonts->Fonts[0]; // fallback to default
        }
    }

    // Make input widget text and selection transparent (we'll render custom overlay)
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.0f)); // Transparent text
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, ImVec4(0.3f, 0.6f, 1.0f, 0.0f)); // Transparent selection

    if (abfFont != nullptr) {
        ImGui::PushFont(abfFont);
    }

    // Scale font
    ImGui::SetWindowFontScale(1.0f); // No scaling needed with native fonts

    // Input flags with CallbackAlways to track cursor/selection position
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackAlways;
    bool enterPressed = ImGui::InputTextMultiline("##TextInput", textBuffer_, sizeof(textBuffer_),
                                                ImVec2(inputWidth, inputHeight), flags, TextInputCallback, this);

    // Store widget rect for overlay rendering
    ImVec2 widgetMin = ImGui::GetItemRectMin();
    ImVec2 widgetMax = ImGui::GetItemRectMax();

    // Render NDI video background behind the input field (AFTER widget is drawn)
    renderNDIBackground(ImVec2(0, 0), ImVec2(inputWidth, inputHeight));

    // Render custom centered text overlay with cursor and selection
    if (abfFont != nullptr) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // Render fading text if fade is active
        if (isFading_ && !fadingText_.empty()) {
            // Split fading text into lines for individual centering
            std::vector<std::string> fadeLines;
            std::stringstream fadeStream(fadingText_);
            std::string fadeLine;

            while (std::getline(fadeStream, fadeLine, '\n')) {
                fadeLines.push_back(fadeLine);
            }

            // Handle case where text ends with newline
            if (fadingText_.back() == '\n') {
                fadeLines.push_back("");
            }

            // Calculate total text height for vertical centering
            float lineHeight = abfFont->FontSize;
            float totalTextHeight = lineHeight * fadeLines.size();
            float startY = widgetMin.y + (widgetMax.y - widgetMin.y - totalTextHeight) * 0.5f;

            float widgetWidth = widgetMax.x - widgetMin.x;

            // Apply exponential fade alpha
            int alpha = (int)(255 * fadeAlpha_);

            // Draw each fading line centered
            for (size_t i = 0; i < fadeLines.size(); ++i) {
                const std::string& currentLine = fadeLines[i];

                if (!currentLine.empty()) {
                    // Calculate centered position for this line
                    ImVec2 lineSize = abfFont->CalcTextSizeA(abfFont->FontSize, FLT_MAX, 0.0f, currentLine.c_str());
                    float lineX = widgetMin.x + (widgetWidth - lineSize.x) * 0.5f;
                    float lineY = startY + i * lineHeight;

                    // Draw the fading line
                    drawList->AddText(abfFont, abfFont->FontSize, ImVec2(lineX, lineY), IM_COL32(255, 255, 255, alpha), currentLine.c_str());
                }
            }
        }

        // Render active text with cursor and selection (only if not fading)
        if (!isFading_ && strlen(textBuffer_) > 0) {
            // Split text into lines for individual centering
            std::string fullText(textBuffer_);
            std::vector<std::string> lines;
            std::stringstream ss(fullText);
            std::string line;

            while (std::getline(ss, line, '\n')) {
                lines.push_back(line);
            }

            // Handle case where text ends with newline
            if (fullText.back() == '\n') {
                lines.push_back("");
            }

            // Calculate total text height for vertical centering
            float lineHeight = abfFont->FontSize;
            float totalTextHeight = lineHeight * lines.size();
            float startY = widgetMin.y + (widgetMax.y - widgetMin.y - totalTextHeight) * 0.5f;

            float widgetWidth = widgetMax.x - widgetMin.x;

            // Keep track of character position for cursor/selection calculation
            int charPos = 0;

            // Draw selection background if there is a selection
            if (selectionStart_ != selectionEnd_ && selectionStart_ >= 0 && selectionEnd_ >= 0) {
                int selStart = std::min(selectionStart_, selectionEnd_);
                int selEnd = std::max(selectionStart_, selectionEnd_);

                int lineCharPos = 0;
                for (size_t i = 0; i < lines.size(); ++i) {
                    const std::string& currentLine = lines[i];
                    int lineEndPos = lineCharPos + currentLine.length();

                    // Check if selection intersects with this line
                    if (selStart <= lineEndPos && selEnd > lineCharPos) {
                        // Calculate line position
                        ImVec2 lineSize = abfFont->CalcTextSizeA(abfFont->FontSize, FLT_MAX, 0.0f, currentLine.c_str());
                        float lineX = widgetMin.x + (widgetWidth - lineSize.x) * 0.5f;
                        float lineY = startY + i * lineHeight;

                        // Calculate selection within this line
                        int lineSelStart = std::max(0, selStart - lineCharPos);
                        int lineSelEnd = std::min((int)currentLine.length(), selEnd - lineCharPos);

                        if (lineSelStart < lineSelEnd) {
                            std::string beforeSel = currentLine.substr(0, lineSelStart);
                            std::string selectedText = currentLine.substr(lineSelStart, lineSelEnd - lineSelStart);

                            ImVec2 beforeSize = abfFont->CalcTextSizeA(abfFont->FontSize, FLT_MAX, 0.0f, beforeSel.c_str());
                            ImVec2 selectedSize = abfFont->CalcTextSizeA(abfFont->FontSize, FLT_MAX, 0.0f, selectedText.c_str());

                            // Draw selection rectangle
                            ImVec2 selMin(lineX + beforeSize.x, lineY);
                            ImVec2 selMax(lineX + beforeSize.x + selectedSize.x, lineY + lineHeight);
                            drawList->AddRectFilled(selMin, selMax, IM_COL32(76, 153, 255, 200)); // Blue selection
                        }
                    }

                    lineCharPos = lineEndPos + 1; // +1 for the newline character
                }
            }

            // Draw each line centered
            charPos = 0;
            for (size_t i = 0; i < lines.size(); ++i) {
                const std::string& currentLine = lines[i];

                if (!currentLine.empty()) {
                    // Calculate centered position for this line
                    ImVec2 lineSize = abfFont->CalcTextSizeA(abfFont->FontSize, FLT_MAX, 0.0f, currentLine.c_str());
                    float lineX = widgetMin.x + (widgetWidth - lineSize.x) * 0.5f;
                    float lineY = startY + i * lineHeight;

                    // Draw the line
                    drawList->AddText(abfFont, abfFont->FontSize, ImVec2(lineX, lineY), IM_COL32(255, 255, 255, 255), currentLine.c_str());
                }

                charPos += currentLine.length() + 1; // +1 for newline
            }

            // Draw cursor if input is active
            if (ImGui::IsItemActive()) {
                // Find which line the cursor is on and position within that line
                int lineCharPos = 0;
                for (size_t i = 0; i < lines.size(); ++i) {
                    const std::string& currentLine = lines[i];
                    int lineEndPos = lineCharPos + currentLine.length();

                    if (cursorPos_ >= lineCharPos && cursorPos_ <= lineEndPos) {
                        // Cursor is on this line
                        int cursorInLine = cursorPos_ - lineCharPos;
                        std::string beforeCursor = currentLine.substr(0, cursorInLine);

                        // Calculate line position
                        ImVec2 lineSize = abfFont->CalcTextSizeA(abfFont->FontSize, FLT_MAX, 0.0f, currentLine.c_str());
                        float lineX = widgetMin.x + (widgetWidth - lineSize.x) * 0.5f;
                        float lineY = startY + i * lineHeight;

                        ImVec2 beforeCursorSize = abfFont->CalcTextSizeA(abfFont->FontSize, FLT_MAX, 0.0f, beforeCursor.c_str());

                        // Blinking cursor animation
                        float time = (float)ImGui::GetTime();
                        float cursorAlpha = (fmod(time, 1.0f) < 0.5f) ? 1.0f : 0.0f;

                        // Draw cursor line
                        ImVec2 cursorPosVec(lineX + beforeCursorSize.x, lineY);
                        ImVec2 cursorEnd(lineX + beforeCursorSize.x, lineY + lineHeight);
                        drawList->AddLine(cursorPosVec, cursorEnd, IM_COL32(255, 255, 255, (int)(255 * cursorAlpha)), 2.0f);
                        break;
                    }

                    lineCharPos = lineEndPos + 1; // +1 for the newline character
                }
            }
        }
    }

    // Manual text change detection by comparing with previous buffer
    std::string currentText(textBuffer_);
    bool textChanged = (currentText != previousTextBuffer_);
    previousTextBuffer_ = currentText;

    ImGui::SetWindowFontScale(1.0f);

    if (abfFont != nullptr) {
        ImGui::PopFont();
    }

    // Pop the transparent text and selection colors
    ImGui::PopStyleColor(2); // Text and TextSelectedBg

    // Clean up styling - always pop in reverse order
    ImGui::PopStyleVar(2); // FramePadding, FrameBorderSize (always pushed)
    ImGui::PopStyleColor(5); // Border, Text, FrameBgActive, FrameBgHovered, FrameBg

    // Show validation message
    if (!validationMessage_.empty()) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + centerOffset);
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", validationMessage_.c_str());
    }

    // Character count and options
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + centerOffset);

    if (showCharacterCount_) {
        size_t currentLength = strlen(textBuffer_);
        size_t maxLength = sizeof(textBuffer_) - 1;
        ImVec4 countColor = (currentLength > maxLength * 0.9f) ? ImVec4(1.0f, 0.6f, 0.0f, 1.0f) : ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
        ImGui::TextColored(countColor, "Characters: %zu/%zu", currentLength, maxLength);
    }

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + centerOffset);
    ImGui::Checkbox("Auto-send on change", &autoSendEnabled_);
    ImGui::SameLine();
    ImGui::Checkbox("Show character count", &showCharacterCount_);

    // If user is typing and there's actual text, stop any fade animation
    if (textChanged && strlen(textBuffer_) > 0) {
        isFading_ = false;
        fadeAlpha_ = 0.0f;
        fadingText_.clear();
    }

    // DEBUG: Log all relevant state
    std::cout << "DEBUG: textChanged=" << textChanged
              << ", textBuffer_='" << textBuffer_ << "'"
              << ", lastSentText_='" << lastSentText_ << "'"
              << ", isFading_=" << isFading_ << std::endl;

    // Check if text was completely cleared BEFORE sending (so we don't lose lastSentText_)
    bool shouldStartFadeOut = textChanged && strlen(textBuffer_) == 0 && !lastSentText_.empty() && !isFading_;
    std::string textToFade = lastSentText_; // Save it before it potentially gets cleared

    if (shouldStartFadeOut) {
        std::cout << "DEBUG: Will start fade-out with text: '" << textToFade << "'" << std::endl;
    }

    // Send text immediately with zero latency for real text changes only
    if (textChanged) {
        sendCurrentText();
    }

    // Now start fade-out if needed (using saved text)
    if (shouldStartFadeOut) {
        // Text was cleared - start fade-out animation
        fadingText_ = textToFade;
        isFading_ = true;
        fadeAlpha_ = 1.0f;
        fadeStartTime_ = std::chrono::steady_clock::now();
        std::cout << "FADE-OUT: Starting fade-out animation with text: '" << fadingText_ << "'" << std::endl;
    }
}

void SenderApp::renderControlButtons() {
    ImGui::Text("Controls");

    // Text size buttons
    ImGui::Text("Font Size:");
    ImGui::SameLine();

    if (ImGui::Button("Small Text", ImVec2(100, 40))) {
        switchTextSize(TextSize::SMALL);
    }
    ImGui::SameLine();

    if (ImGui::Button("Big Text", ImVec2(100, 40))) {
        switchTextSize(TextSize::BIG);
    }

    // Real-time streaming info
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "Real-time streaming: Text sent on every keystroke");

    // Current settings
    ImGui::Spacing();
    ImGui::Text("Current Size: %s", (currentTextSize_ == TextSize::BIG) ? "Big" : "Small");

    // Keyboard shortcuts toggle
    ImGui::Spacing();
    ImGui::Checkbox("Show keyboard shortcuts", &showKeyboardShortcuts_);
}

void SenderApp::renderHealthStatus() {
    ImGui::Text("System Health");

    HealthStatus overall = healthMonitor_->getOverallStatus();
    ImVec4 statusColor;

    switch (overall) {
        case HealthStatus::HEALTHY:
            statusColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
            break;
        case HealthStatus::WARNING:
            statusColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
            break;
        case HealthStatus::CRITICAL:
            statusColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
            break;
        default:
            statusColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
            break;
    }

    ImGui::TextColored(statusColor, "Status: %s", healthMonitor_->getStatusString().c_str());

    // Network Traffic Lights and Statistics
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Network Status");

    if (publisher_) {
        auto stats = publisher_->getStats();

        // Display traffic lights for each feed
        for (size_t i = 0; i < stats.size(); ++i) {
            const auto& stat = stats[i];

            // Traffic light square
            ImVec4 lightColor;
            const char* statusText;
            if (stat.isConnected && !stat.hasErrors) {
                lightColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // Green
                statusText = "ONLINE";
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
            float square_size = 15.0f;
            draw_list->AddRectFilled(pos, ImVec2(pos.x + square_size, pos.y + square_size),
                                   ImGui::ColorConvertFloat4ToU32(lightColor));
            draw_list->AddRect(pos, ImVec2(pos.x + square_size, pos.y + square_size),
                             ImGui::ColorConvertFloat4ToU32(ImVec4(0.3f, 0.3f, 0.3f, 1.0f)));

            // Move cursor past the square
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + square_size + 8.0f);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);

            // Feed label and status
            ImGui::TextColored(lightColor, "Feed %zu: %s", i + 1, statusText);

            // Statistics on same line
            ImGui::SameLine();
            ImGui::Text("| Sent: %lu | Bytes: %lu", stat.messagesPublished, stat.bytesPublished);

            if (stat.hasErrors) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "| Errors!");
                if (!stat.lastError.empty() && ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Last error: %s", stat.lastError.c_str());
                }
            }
        }

        // Overall network statistics
        ImGui::Spacing();
        uint64_t totalSent = 0, totalBytes = 0;
        for (const auto& stat : stats) {
            totalSent += stat.messagesPublished;
            totalBytes += stat.bytesPublished;
        }

        ImGui::Text("Total: %lu messages, %lu bytes", totalSent, totalBytes);
        if (totalBytes > 1024) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "(%.1f KB)", totalBytes / 1024.0);
        }
    } else {
        // No network connection
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        float square_size = 15.0f;
        draw_list->AddRectFilled(pos, ImVec2(pos.x + square_size, pos.y + square_size),
                               ImGui::ColorConvertFloat4ToU32(ImVec4(0.5f, 0.5f, 0.5f, 1.0f)));
        draw_list->AddRect(pos, ImVec2(pos.x + square_size, pos.y + square_size),
                         ImGui::ColorConvertFloat4ToU32(ImVec4(0.3f, 0.3f, 0.3f, 1.0f)));

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + square_size + 8.0f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Network: NOT INITIALIZED");
    }

    if (ImGui::TreeNode("Health Details")) {
        auto metrics = healthMonitor_->getAllMetrics();
        for (const auto& metric : metrics) {
            ImVec4 metricColor = statusColor;
            switch (metric.status) {
                case HealthStatus::HEALTHY: metricColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); break;
                case HealthStatus::WARNING: metricColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); break;
                case HealthStatus::CRITICAL: metricColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); break;
                default: metricColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f); break;
            }

            ImGui::TextColored(metricColor, "%s: %.1f", metric.name.c_str(), metric.value);
            if (!metric.details.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("(%s)", metric.details.c_str());
            }
        }
        ImGui::TreePop();
    }
}

void SenderApp::renderConnectionStatus() {
    ImGui::Text("Connection Status");

    if (publisher_) {
        auto stats = publisher_->getStats();

        for (size_t i = 0; i < stats.size(); ++i) {
            const auto& stat = stats[i];
            const char* feedName = (i == 0) ? "Primary" : "Secondary";

            ImVec4 connColor = stat.isConnected ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
            ImGui::TextColored(connColor, "%s: %s", feedName, stat.isConnected ? "Connected" : "Disconnected");

            if (stat.isConnected) {
                ImGui::Text("  Messages: %llu", stat.messagesPublished);
                ImGui::Text("  Bytes: %llu", stat.bytesPublished);
            } else if (!stat.lastError.empty()) {
                ImGui::TextDisabled("  Error: %s", stat.lastError.c_str());
            }
        }

        bool healthy = publisher_->isHealthy();
        ImGui::Text("Overall: %s", healthy ? "HEALTHY" : "DEGRADED");
    }
}

void SenderApp::renderTextMemory() {
    ImGui::Text("Text Memory (Messages displayed > 3 seconds)");

    if (ImGui::BeginChild("TextMemoryScroll", ImVec2(0, 200), true)) {
        auto entries = textMemory_->getEntries();

        if (entries.empty()) {
            ImGui::TextDisabled("No messages in memory yet");
        } else {
            for (const auto& entry : entries) {
                // Format timestamp
                time_t timestamp = entry.displayedAt / 1000;
                struct tm* timeinfo = localtime(&timestamp);
                char timeBuffer[80];
                strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", timeinfo);

                ImGui::Text("[%s] %s", timeBuffer, entry.text.c_str());
            }
        }
    }
    ImGui::EndChild();
}

void SenderApp::sendText() {
    if (publisher_) {
        std::string text(textBuffer_);
        if (!text.empty()) {
            TextMessage message(text, currentTextSize_);
            publisher_->publish(message);
            lastSentText_ = text;

            // Record in text memory
            textMemory_->recordText(text);
        }
    }
}

void SenderApp::sendCurrentText() {
    if (publisher_) {
        std::string text(textBuffer_);

        std::cout << "SEND_DEBUG: text='" << text << "', lastSentText_='" << lastSentText_ << "'" << std::endl;

        // Send the current text state (even if empty)
        if (!text.empty()) {
            TextMessage message(text, currentTextSize_);
            publisher_->publish(message);
            lastSentText_ = text;
            textMemory_->recordText(text);
            std::cout << "SEND_DEBUG: Sent text message, updated lastSentText_='" << lastSentText_ << "'" << std::endl;
        } else {
            // Text is empty - send clear message
            if (!lastSentText_.empty()) {
                // Send clear message
                TextMessage clearMessage = TextMessage::createClearMessage();
                publisher_->publish(clearMessage);
                textMemory_->onTextCleared();
                std::cout << "SEND_DEBUG: Sent clear message, lastSentText_ remains='" << lastSentText_ << "'" << std::endl;
                // Don't clear lastSentText_ here - let the fade-out logic handle it
            } else {
                std::cout << "SEND_DEBUG: Text empty and lastSentText_ already empty - no action" << std::endl;
            }
        }
    }
}

void SenderApp::clearText() {
    if (publisher_) {
        TextMessage clearMessage = TextMessage::createClearMessage();
        publisher_->publish(clearMessage);

        // Record cleared text in memory
        textMemory_->onTextCleared();

        // Start fade out animation with current text
        if (strlen(textBuffer_) > 0) {
            fadingText_ = std::string(textBuffer_);
            isFading_ = true;
            fadeAlpha_ = 1.0f;
            fadeStartTime_ = std::chrono::steady_clock::now();
        }

        // Clear input field
        memset(textBuffer_, 0, sizeof(textBuffer_));
        lastSentText_.clear();
    }
}

void SenderApp::switchTextSize(TextSize size) {
    currentTextSize_ = size;
    // Always send when size changes (to ensure receiver clears the old output)
    sendText();
}

void SenderApp::setupDarkTheme() {
    ImGuiStyle& style = ImGui::GetStyle();

    // Dark theme colors
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
    style.Colors[ImGuiCol_Text] = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.0f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.45f, 0.45f, 0.45f, 1.0f);

    // Rounded corners
    style.WindowRounding = 5.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.ScrollbarRounding = 3.0f;
}

void SenderApp::updateHealthMonitoring() {
    if (publisher_) {
        auto stats = publisher_->getStats();

        for (size_t i = 0; i < stats.size(); ++i) {
            const auto& stat = stats[i];
            std::string connectionName = (i == 0) ? "Primary Connection" : "Secondary Connection";

            healthMonitor_->updateConnectionStatus(connectionName, stat.isConnected, stat.lastError);

            if (stat.isConnected) {
                healthMonitor_->updateMetric(connectionName + " Messages/sec",
                                           static_cast<double>(stat.messagesPublished),
                                           HealthStatus::HEALTHY);
            }
        }

        // Overall publisher health
        bool publisherHealthy = publisher_->isHealthy();
        healthMonitor_->updateMetric("Publisher Health", publisherHealthy ? 1.0 : 0.0,
                                   publisherHealthy ? HealthStatus::HEALTHY : HealthStatus::CRITICAL);
    }
}

void SenderApp::renderKeyboardShortcuts() {
    ImGui::Separator();
    ImGui::Text("Keyboard Shortcuts:");
    ImGui::BulletText("Ctrl+Enter: Send text");
    ImGui::BulletText("Ctrl+D: Clear text");
    ImGui::BulletText("Ctrl+1: Small text mode");
    ImGui::BulletText("Ctrl+2: Big text mode");
    ImGui::BulletText("F1: Toggle shortcuts help");
}

void SenderApp::handleKeyboardInput() {
    ImGuiIO& io = ImGui::GetIO();

    // Handle keyboard shortcuts
    if (io.KeyCtrl) {
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter)) && isTextValid()) {
            sendText();
        } else if (ImGui::IsKeyPressed(ImGuiKey_D)) {
            clearText();
        } else if (ImGui::IsKeyPressed(ImGuiKey_1)) {
            switchTextSize(TextSize::SMALL);
        } else if (ImGui::IsKeyPressed(ImGuiKey_2)) {
            switchTextSize(TextSize::BIG);
        }
    }

    // Toggle shortcuts help
    if (ImGui::IsKeyPressed(ImGuiKey_F1)) {
        showKeyboardShortcuts_ = !showKeyboardShortcuts_;
    }
}

void SenderApp::validateTextInput() {
    size_t currentLength = strlen(textBuffer_);
    size_t maxLength = sizeof(textBuffer_) - 1;

    textTooLong_ = (currentLength >= maxLength);
    validationMessage_.clear();

    if (textTooLong_) {
        validationMessage_ = "Text too long - maximum " + std::to_string(maxLength) + " characters";
    }
    // Don't show validation message for empty text - it's normal for the field to be empty

    // Check for invalid characters (non-printable ASCII)
    bool hasInvalidChars = false;
    for (size_t i = 0; i < currentLength; ++i) {
        char c = textBuffer_[i];
        if (c < 32 && c != '\n' && c != '\r' && c != '\t') {
            hasInvalidChars = true;
            break;
        }
    }

    if (hasInvalidChars) {
        validationMessage_ = "Contains invalid characters";
    }
}

bool SenderApp::isTextValid() const {
    size_t currentLength = strlen(textBuffer_);
    return currentLength > 0 && currentLength < sizeof(textBuffer_) - 1 && !textTooLong_;
}

int SenderApp::TextInputCallback(ImGuiInputTextCallbackData* data) {
    SenderApp* app = static_cast<SenderApp*>(data->UserData);
    if (!app) {
        return 0;
    }

    // Capture cursor and selection positions
    app->cursorPos_ = data->CursorPos;
    app->selectionStart_ = data->SelectionStart;
    app->selectionEnd_ = data->SelectionEnd;

    return 0;
}

void SenderApp::updateFade() {
    if (!isFading_) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration<float>(now - fadeStartTime_).count();

    if (elapsed >= FADE_DURATION_SECONDS) {
        // Fade completed
        isFading_ = false;
        fadeAlpha_ = 0.0f;
        fadingText_.clear();
        lastSentText_.clear(); // Clear after fade animation is complete
    } else {
        // Exponential fade out
        float progress = elapsed / FADE_DURATION_SECONDS;
        fadeAlpha_ = std::exp(-5.0f * progress); // Exponential decay
    }
}

// ============================================================================
// NDI Implementation
// ============================================================================

bool SenderApp::initializeNDI() {
    std::cout << "Initializing NDI receiver..." << std::endl;

    ndiReceiver_ = std::make_unique<NDIReceiver>();

    if (!ndiReceiver_->initialize()) {
        std::cerr << "Warning: Failed to initialize NDI. Video background disabled." << std::endl;
        ndiReceiver_.reset();
        return false;
    }

    // Try to connect to first available NDI source
    // Change "" to specific name like "Resolume" or "Arena" to filter
    if (!ndiReceiver_->connect("")) {
        std::cerr << "Warning: No NDI source found. Video background disabled." << std::endl;
        ndiReceiver_.reset();
        return false;
    }

    std::cout << "Connected to NDI source: " << ndiReceiver_->getSourceName() << std::endl;

    // Create OpenGL texture for NDI frames
    glGenTextures(1, &ndiTexture_);
    glBindTexture(GL_TEXTURE_2D, ndiTexture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

void SenderApp::updateNDITexture() {
    if (!ndiReceiver_ || !ndiReceiver_->isConnected()) {
        return;
    }

    // Capture latest NDI frame
    NDIFrame* frame = ndiReceiver_->captureFrame();
    if (!frame || !frame->data) {
        return;
    }

    // Upload frame to OpenGL texture
    glBindTexture(GL_TEXTURE_2D, ndiTexture_);

    // Reallocate texture if size changed
    if (frame->width != ndiTextureWidth_ || frame->height != ndiTextureHeight_) {
        ndiTextureWidth_ = frame->width;
        ndiTextureHeight_ = frame->height;
        std::cout << "NDI frame size: " << ndiTextureWidth_ << "x" << ndiTextureHeight_ << std::endl;
    }

    // Upload BGRA data to texture
    // NDI gives us BGRA, OpenGL expects BGRA with GL_BGRA format
    glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->stride / 4); // 4 bytes per pixel (BGRA)
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame->width, frame->height,
                 0, GL_BGRA, GL_UNSIGNED_BYTE, frame->data);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    glBindTexture(GL_TEXTURE_2D, 0);

    // Release the frame back to NDI
    ndiReceiver_->releaseFrame(frame);
}

void SenderApp::renderNDIBackground(const ImVec2& pos, const ImVec2& size) {
    if (!ndiTexture_ || ndiTextureWidth_ == 0 || ndiTextureHeight_ == 0) {
        return;
    }

    // Use ImGui's low-level draw list to render texture behind the input widget
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (!drawList) {
        return;
    }

    // Get actual widget bounds (after it was drawn)
    ImVec2 rectMin = ImGui::GetItemRectMin();
    ImVec2 rectMax = ImGui::GetItemRectMax();
    float areaWidth = rectMax.x - rectMin.x;
    float areaHeight = rectMax.y - rectMin.y;

    // Calculate aspect-fit for NDI video within input area (maintain aspect ratio, show full video)
    float videoAspect = (float)ndiTextureWidth_ / (float)ndiTextureHeight_;
    float areaAspect = areaWidth / areaHeight;

    float scaledWidth, scaledHeight;

    // Scale to fit - find the limiting dimension
    if (videoAspect > areaAspect) {
        // Video is wider than area - fit to width
        scaledWidth = areaWidth;
        scaledHeight = areaWidth / videoAspect;
    } else {
        // Video is taller than area - fit to height
        scaledHeight = areaHeight;
        scaledWidth = areaHeight * videoAspect;
    }

    // Center the scaled video in the area
    float offsetX = (areaWidth - scaledWidth) * 0.5f;
    float offsetY = (areaHeight - scaledHeight) * 0.5f;

    ImVec2 videoMin = ImVec2(rectMin.x + offsetX, rectMin.y + offsetY);
    ImVec2 videoMax = ImVec2(rectMin.x + offsetX + scaledWidth, rectMin.y + offsetY + scaledHeight);

    // Draw the NDI video texture centered and aspect-fitted
    drawList->AddImage(
        (ImTextureID)(intptr_t)ndiTexture_,
        videoMin,
        videoMax,
        ImVec2(0, 0), // UV min
        ImVec2(1, 1), // UV max
        IM_COL32(255, 255, 255, 128) // Semi-transparent so text is visible
    );
}

void SenderApp::shutdownNDI() {
    if (ndiTexture_) {
        glDeleteTextures(1, &ndiTexture_);
        ndiTexture_ = 0;
    }

    if (ndiReceiver_) {
        ndiReceiver_->shutdown();
        ndiReceiver_.reset();
    }
}

} // namespace LiveText