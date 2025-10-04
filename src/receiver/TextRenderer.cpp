#include "TextRenderer.h"
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace LiveText {

TextRenderer::TextRenderer()
    : ft_(nullptr)
    , faceSmall_(nullptr)
    , faceBig_(nullptr)
    , VAO_(0)
    , VBO_(0)
    , shaderProgram_(0)
    , frameBuffer_(0)
    , colorTexture_(0)
    , depthBuffer_(0)
    , currentSize_(TextSize::SMALL)
    , fadeAlpha_(0.0f)
    , isFading_(false)
    , windowWidth_(1920)
    , windowHeight_(1080)
{
}

TextRenderer::~TextRenderer() {
    shutdown();
}

bool TextRenderer::initialize(int windowWidth, int windowHeight) {
    windowWidth_ = windowWidth;
    windowHeight_ = windowHeight;

    // Initialize FreeType
    if (FT_Init_FreeType(&ft_)) {
        std::cerr << "Could not init FreeType Library" << std::endl;
        return false;
    }

    // Load ABF font - try project font first, then fallbacks
    std::vector<std::string> fontPaths = {
        "fonts/ABF.ttf",
        "../fonts/ABF.ttf",
        "../../fonts/ABF.ttf",
#ifdef _WIN32
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/calibri.ttf",
        "C:/Windows/Fonts/tahoma.ttf"
#elif __APPLE__
        "/System/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
#else
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/TTF/arial.ttf"
#endif
    };

    bool fontLoaded = false;
    for (const auto& fontPath : fontPaths) {
        if (loadFont(fontPath, SMALL_FONT_SIZE, faceSmall_, charactersSmall_) &&
            loadFont(fontPath, BIG_FONT_SIZE, faceBig_, charactersBig_)) {
            fontLoaded = true;
            std::cout << "Loaded font: " << fontPath << std::endl;
            break;
        }
    }

    if (!fontLoaded) {
        std::cerr << "Failed to load any font" << std::endl;
        return false;
    }

    // Create shader program
    shaderProgram_ = createShaderProgram();
    if (shaderProgram_ == 0) {
        std::cerr << "Failed to create shader program" << std::endl;
        return false;
    }

    // Create VAO/VBO for rendering
    glGenVertexArrays(1, &VAO_);
    glGenBuffers(1, &VBO_);
    glBindVertexArray(VAO_);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Create frame buffer for rendering to texture
    if (!createFrameBuffer()) {
        std::cerr << "Failed to create frame buffer" << std::endl;
        return false;
    }

    return true;
}

void TextRenderer::shutdown() {
    // Cleanup OpenGL objects
    if (frameBuffer_) {
        glDeleteFramebuffers(1, &frameBuffer_);
        frameBuffer_ = 0;
    }
    if (colorTexture_) {
        glDeleteTextures(1, &colorTexture_);
        colorTexture_ = 0;
    }
    if (depthBuffer_) {
        glDeleteRenderbuffers(1, &depthBuffer_);
        depthBuffer_ = 0;
    }
    if (VAO_) {
        glDeleteVertexArrays(1, &VAO_);
        VAO_ = 0;
    }
    if (VBO_) {
        glDeleteBuffers(1, &VBO_);
        VBO_ = 0;
    }
    if (shaderProgram_) {
        glDeleteProgram(shaderProgram_);
        shaderProgram_ = 0;
    }

    // Cleanup character textures
    for (auto& pair : charactersSmall_) {
        glDeleteTextures(1, &pair.second.textureID);
    }
    for (auto& pair : charactersBig_) {
        glDeleteTextures(1, &pair.second.textureID);
    }
    charactersSmall_.clear();
    charactersBig_.clear();

    // Cleanup FreeType
    if (faceSmall_) {
        FT_Done_Face(faceSmall_);
        faceSmall_ = nullptr;
    }
    if (faceBig_) {
        FT_Done_Face(faceBig_);
        faceBig_ = nullptr;
    }
    if (ft_) {
        FT_Done_FreeType(ft_);
        ft_ = nullptr;
    }
}

void TextRenderer::updateText(const std::string& text, TextSize size) {
    std::cout << "TextRenderer::updateText - text='" << text << "', size=" << (size == TextSize::SMALL ? "SMALL" : "BIG") << std::endl;
    if (currentText_ != text || currentSize_ != size) {
        currentText_ = text;
        currentSize_ = size;
        cachedText_.isDirty = true;  // Mark cache as dirty
        std::cout << "TextRenderer::updateText - text/size changed, cache marked dirty" << std::endl;
    }
    fadeAlpha_ = 1.0f;
    isFading_ = false;
    std::cout << "TextRenderer::updateText - fadeAlpha=" << fadeAlpha_ << ", isFading=" << isFading_ << std::endl;
}

void TextRenderer::clearText() {
    if (!currentText_.empty() || fadeAlpha_ > 0.01f) {
        std::cout << "Starting fade-out animation on receiver with text: '" << currentText_ << "'" << std::endl;
        isFading_ = true;
        fadeStartTime_ = std::chrono::steady_clock::now();
    }
}

void TextRenderer::render() {
    updateFade();

    std::cout << "TextRenderer::render - currentText='" << currentText_ << "', fadeAlpha=" << fadeAlpha_
              << ", isFading=" << isFading_ << ", isEmpty=" << currentText_.empty() << std::endl;

    // Always render to framebuffer (even if empty) so Syphon gets a valid texture
    // Bind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer_);
    glViewport(0, 0, windowWidth_, windowHeight_);

    // Clear with transparent black
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Only render text if we have something to show
    if (!currentText_.empty() || fadeAlpha_ > 0.01f) {
        // Enable blending
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Use shader
        glUseProgram(shaderProgram_);

        // Set projection matrix (orthographic)
        glm::mat4 projection = glm::ortho(0.0f, static_cast<GLfloat>(windowWidth_),
                                         0.0f, static_cast<GLfloat>(windowHeight_));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram_, "projection"), 1, GL_FALSE,
                          glm::value_ptr(projection));

        // Set text color (white)
        glUniform3f(glGetUniformLocation(shaderProgram_, "textColor"), 1.0f, 1.0f, 1.0f);
        glUniform1f(glGetUniformLocation(shaderProgram_, "alpha"), fadeAlpha_);

        // Update cache if needed and render
        updateTextCache();
        renderCachedText();

        // Cleanup
        glBindVertexArray(0);
        glUseProgram(0);
        glDisable(GL_BLEND);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void TextRenderer::setWindowSize(int width, int height) {
    if (width != windowWidth_ || height != windowHeight_) {
        windowWidth_ = width;
        windowHeight_ = height;

        // Recreate framebuffer with new size
        if (frameBuffer_) {
            glDeleteFramebuffers(1, &frameBuffer_);
            glDeleteTextures(1, &colorTexture_);
            glDeleteRenderbuffers(1, &depthBuffer_);
        }
        createFrameBuffer();
    }
}

bool TextRenderer::loadFont(const std::string& fontPath, int fontSize, FT_Face& face, std::map<GLchar, Character>& characters) {
    if (FT_New_Face(ft_, fontPath.c_str(), 0, &face)) {
        return false;
    }

    FT_Set_Pixel_Sizes(face, 0, fontSize);

    // Disable byte-alignment restriction
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Load first 128 characters of ASCII set
    for (GLubyte c = 0; c < 128; c++) {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            continue;
        }

        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer
        );

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        Character character = {
            texture,
            static_cast<int>(face->glyph->bitmap.width),
            static_cast<int>(face->glyph->bitmap.rows),
            static_cast<int>(face->glyph->bitmap_left),
            static_cast<int>(face->glyph->bitmap_top),
            static_cast<GLuint>(face->glyph->advance.x)
        };
        characters.insert(std::pair<GLchar, Character>(c, character));
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

GLuint TextRenderer::compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation error: " << infoLog << std::endl;
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint TextRenderer::createShaderProgram() {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, VERTEX_SHADER_SOURCE);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER_SOURCE);

    if (vertexShader == 0 || fragmentShader == 0) {
        if (vertexShader) glDeleteShader(vertexShader);
        if (fragmentShader) glDeleteShader(fragmentShader);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Shader program linking error: " << infoLog << std::endl;
        glDeleteProgram(program);
        program = 0;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}

bool TextRenderer::createFrameBuffer() {
    // Generate framebuffer
    glGenFramebuffers(1, &frameBuffer_);
    glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer_);

    // Create color texture
    glGenTextures(1, &colorTexture_);
    glBindTexture(GL_TEXTURE_2D, colorTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, windowWidth_, windowHeight_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture_, 0);

    // Create depth buffer
    glGenRenderbuffers(1, &depthBuffer_);
    glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, windowWidth_, windowHeight_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer_);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Framebuffer not complete!" << std::endl;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void TextRenderer::updateFade() {
    if (isFading_) {
        auto now = std::chrono::steady_clock::now();
        float elapsed = std::chrono::duration<float>(now - fadeStartTime_).count();

        if (elapsed >= FADE_DURATION_SECONDS) {
            fadeAlpha_ = 0.0f;
            isFading_ = false;
            currentText_.clear();
        } else {
            // Exponential decay
            float progress = elapsed / FADE_DURATION_SECONDS;
            fadeAlpha_ = std::exp(-3.0f * progress); // Exponential fade
        }
    }
}

void TextRenderer::renderText(const std::string& text, float x, float y, float scale,
                             const std::map<GLchar, Character>& characters) {
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(VAO_);

    float currentX = x;

    for (char c : text) {
        auto it = characters.find(c);
        if (it == characters.end()) {
            continue;
        }

        const Character& ch = it->second;

        float xpos = currentX + ch.bearingX * scale;
        float ypos = y - (ch.sizeY - ch.bearingY) * scale;

        float w = ch.sizeX * scale;
        float h = ch.sizeY * scale;

        // Update VBO for each character
        GLfloat vertices[6][4] = {
            { xpos,     ypos + h,   0.0, 0.0 },
            { xpos,     ypos,       0.0, 1.0 },
            { xpos + w, ypos,       1.0, 1.0 },

            { xpos,     ypos + h,   0.0, 0.0 },
            { xpos + w, ypos,       1.0, 1.0 },
            { xpos + w, ypos + h,   1.0, 0.0 }
        };

        glBindTexture(GL_TEXTURE_2D, ch.textureID);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glDrawArrays(GL_TRIANGLES, 0, 6);

        currentX += (ch.advance >> 6) * scale; // Bitshift by 6 to get value in pixels (2^6 = 64)
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

float TextRenderer::getTextWidth(const std::string& text, const std::map<GLchar, Character>& characters) const {
    float width = 0.0f;

    if (text.empty()) {
        return width;
    }

    for (char c : text) {
        // Only process printable ASCII characters to avoid issues
        if (c >= 0 && c < 128) {
            auto it = characters.find(static_cast<GLchar>(c));
            if (it != characters.end()) {
                width += (it->second.advance >> 6);
            } else {
                // Use space width as fallback for missing characters
                auto spaceIt = characters.find(' ');
                if (spaceIt != characters.end()) {
                    width += (spaceIt->second.advance >> 6);
                }
            }
        }
    }
    return width;
}

void TextRenderer::updateTextCache() {
    std::cout << "TextRenderer::updateTextCache - isDirty=" << cachedText_.isDirty
              << ", currentText='" << currentText_ << "', empty=" << currentText_.empty() << std::endl;

    if (!cachedText_.isDirty || currentText_.empty()) {
        std::cout << "TextRenderer::updateTextCache - early return" << std::endl;
        return;
    }

    cachedText_.text = currentText_;
    cachedText_.size = currentSize_;
    std::cout << "TextRenderer::updateTextCache - updating cache for text='" << currentText_ << "'" << std::endl;

    // Choose character map based on size
    const auto& characters = (currentSize_ == TextSize::BIG) ? charactersBig_ : charactersSmall_;
    float scale = (currentSize_ == TextSize::BIG) ? 1.0f : 0.5f;
    float lineHeight = (currentSize_ == TextSize::BIG ? BIG_FONT_SIZE : SMALL_FONT_SIZE) * scale;

    // Split text into lines
    std::vector<std::string> lines;
    std::string currentLine;
    for (char c : currentText_) {
        if (c == '\n') {
            lines.push_back(currentLine);
            currentLine.clear();
        } else {
            currentLine += c;
        }
    }
    if (!currentLine.empty() || (!currentText_.empty() && currentText_.back() == '\n')) {
        lines.push_back(currentLine);
    }

    // Calculate total text height for vertical centering
    float totalTextHeight = lineHeight * lines.size();
    float startY = (windowHeight_ + totalTextHeight) / 2.0f - lineHeight;  // Start from top line

    // Pre-calculate all vertices
    cachedText_.vertices.clear();
    cachedText_.vertices.reserve(currentText_.length() * 24);  // 6 vertices * 4 components per character

    // Render each line centered
    for (size_t lineIdx = 0; lineIdx < lines.size(); ++lineIdx) {
        const std::string& line = lines[lineIdx];

        // Calculate line width and center position
        float lineWidth = getTextWidth(line, characters) * scale;
        float x = (windowWidth_ - lineWidth) / 2.0f;
        float y = startY - (lineIdx * lineHeight);

        // Add vertices for each character in the line
        float currentX = x;
        for (char c : line) {
            if (c >= 0 && c < 128) {
                auto it = characters.find(static_cast<GLchar>(c));
                if (it != characters.end()) {
                    const Character& ch = it->second;

                    float xpos = currentX + ch.bearingX * scale;
                    float ypos = y - (ch.sizeY - ch.bearingY) * scale;
                    float w = ch.sizeX * scale;
                    float h = ch.sizeY * scale;

                    // Add vertices for this character (2 triangles = 6 vertices)
                    GLfloat vertices[24] = {
                        xpos,     ypos + h,   0.0f, 0.0f,
                        xpos,     ypos,       0.0f, 1.0f,
                        xpos + w, ypos,       1.0f, 1.0f,

                        xpos,     ypos + h,   0.0f, 0.0f,
                        xpos + w, ypos,       1.0f, 1.0f,
                        xpos + w, ypos + h,   1.0f, 0.0f
                    };

                    for (int i = 0; i < 24; ++i) {
                        cachedText_.vertices.push_back(vertices[i]);
                    }

                    currentX += (ch.advance >> 6) * scale;
                }
            }
        }
    }

    cachedText_.isDirty = false;
}

void TextRenderer::renderCachedText() {
    std::cout << "TextRenderer::renderCachedText - vertices.size()=" << cachedText_.vertices.size()
              << ", text='" << cachedText_.text << "'" << std::endl;

    if (cachedText_.vertices.empty()) {
        std::cout << "TextRenderer::renderCachedText - early return (empty vertices)" << std::endl;
        return;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(VAO_);

    // Choose character map
    const auto& characters = (cachedText_.size == TextSize::BIG) ? charactersBig_ : charactersSmall_;

    // Render all characters using cached vertices
    size_t vertexIndex = 0;
    int charCount = 0;
    for (char c : cachedText_.text) {
        // Skip newlines - they don't have vertices
        if (c == '\n') {
            continue;
        }

        if (c >= 0 && c < 128) {
            auto it = characters.find(static_cast<GLchar>(c));
            if (it != characters.end()) {
                const Character& ch = it->second;

                // Update VBO with pre-calculated vertices
                glBindBuffer(GL_ARRAY_BUFFER, VBO_);
                glBufferSubData(GL_ARRAY_BUFFER, 0, 24 * sizeof(GLfloat),
                               &cachedText_.vertices[vertexIndex]);
                glBindBuffer(GL_ARRAY_BUFFER, 0);

                glBindTexture(GL_TEXTURE_2D, ch.textureID);
                glDrawArrays(GL_TRIANGLES, 0, 6);

                charCount++;
                vertexIndex += 24;
            } else {
                std::cout << "TextRenderer::renderCachedText - character '" << c << "' not found in map!" << std::endl;
            }
        }
    }
    std::cout << "TextRenderer::renderCachedText - rendered " << charCount << " characters" << std::endl;

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace LiveText