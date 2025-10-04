#pragma once
#ifdef __APPLE__
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#endif
#include <OpenGL/gl.h>
#include <OpenGL/gl3.h>
#else
#include <GL/gl3w.h>
#endif
#include <ft2build.h>
#include FT_FREETYPE_H
#include <string>
#include <map>
#include <memory>
#include <chrono>
#include "common/TextMessage.h"

namespace LiveText {

struct Character {
    GLuint textureID;   // ID handle of the glyph texture
    int sizeX, sizeY;   // Size of glyph
    int bearingX, bearingY; // Offset from baseline to left/top of glyph
    GLuint advance;     // Offset to advance to next glyph
};

class TextRenderer {
public:
    TextRenderer();
    ~TextRenderer();

    bool initialize(int windowWidth, int windowHeight);
    void shutdown();

    void updateText(const std::string& text, TextSize size);
    void clearText();
    void render();

    void setWindowSize(int width, int height);

    // Get rendered texture for Spout
    GLuint getRenderedTexture() const { return colorTexture_; }
    bool hasContent() const { return !currentText_.empty() || fadeAlpha_ > 0.01f; }

private:
    // FreeType
    FT_Library ft_;
    FT_Face faceSmall_;
    FT_Face faceBig_;

    // OpenGL objects
    GLuint VAO_, VBO_;
    GLuint shaderProgram_;
    GLuint frameBuffer_;
    GLuint colorTexture_;
    GLuint depthBuffer_;

    // Character maps for different font sizes
    std::map<GLchar, Character> charactersSmall_;
    std::map<GLchar, Character> charactersBig_;

    // Current state
    std::string currentText_;
    TextSize currentSize_;
    float fadeAlpha_;
    bool isFading_;
    std::chrono::steady_clock::time_point fadeStartTime_;

    // Performance optimization - cached rendering data
    struct CachedTextData {
        std::string text;
        TextSize size;
        std::vector<GLfloat> vertices;
        float textWidth;
        float textHeight;
        bool isDirty;

        CachedTextData() : isDirty(true) {}
    } cachedText_;

    // Rendering properties
    int windowWidth_, windowHeight_;
    static constexpr float FADE_DURATION_SECONDS = 2.0f;
    static constexpr int SMALL_FONT_SIZE = 192;  // 4x original (48 -> 192)
    static constexpr int BIG_FONT_SIZE = 384;   // 4x original (96 -> 384)

    // Shader sources
    static constexpr const char* VERTEX_SHADER_SOURCE = R"(
        #version 330 core
        layout (location = 0) in vec4 vertex; // <vec2 pos, vec2 tex>
        out vec2 TexCoords;

        uniform mat4 projection;

        void main() {
            gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);
            TexCoords = vertex.zw;
        }
    )";

    static constexpr const char* FRAGMENT_SHADER_SOURCE = R"(
        #version 330 core
        in vec2 TexCoords;
        out vec4 color;

        uniform sampler2D text;
        uniform vec3 textColor;
        uniform float alpha;

        void main() {
            vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);
            color = vec4(textColor, alpha) * sampled;
        }
    )";

    // Helper methods
    bool loadFont(const std::string& fontPath, int fontSize, FT_Face& face, std::map<GLchar, Character>& characters);
    GLuint compileShader(GLenum type, const char* source);
    GLuint createShaderProgram();
    bool createFrameBuffer();
    void updateFade();
    void updateTextCache();
    void renderCachedText();
    void renderText(const std::string& text, float x, float y, float scale, const std::map<GLchar, Character>& characters);
    float getTextWidth(const std::string& text, const std::map<GLchar, Character>& characters) const;
};

} // namespace LiveText