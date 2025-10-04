#pragma once
#include <string>
#include <cstdint>
#include <chrono>

namespace LiveText {

enum class TextSize : uint8_t {
    SMALL = 0,
    BIG = 1
};

enum class MessageType : uint8_t {
    TEXT_UPDATE = 1,
    CLEAR_TEXT = 2,
    HEARTBEAT = 3
};

struct TextMessage {
    MessageType type;
    TextSize size;
    uint32_t textLength;
    uint64_t timestamp;
    char text[512];  // Fixed size for efficient serialization

    TextMessage();
    explicit TextMessage(const std::string& text, TextSize size = TextSize::SMALL);
    static TextMessage createClearMessage();
    static TextMessage createHeartbeat();

    std::string getText() const;
    void setText(const std::string& text);

    // Serialization
    size_t serialize(uint8_t* buffer, size_t bufferSize) const;
    bool deserialize(const uint8_t* buffer, size_t bufferSize);
    static constexpr size_t getMaxSerializedSize() { return sizeof(TextMessage); }
};

} // namespace LiveText