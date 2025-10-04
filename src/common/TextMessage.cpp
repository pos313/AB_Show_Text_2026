#include "TextMessage.h"
#include <cstring>
#include <algorithm>

namespace LiveText {

TextMessage::TextMessage()
    : type(MessageType::TEXT_UPDATE)
    , size(TextSize::SMALL)
    , textLength(0)
    , timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count())
{
    memset(text, 0, sizeof(text));
}

TextMessage::TextMessage(const std::string& text, TextSize size)
    : type(MessageType::TEXT_UPDATE)
    , size(size)
    , timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count())
{
    setText(text);
}

TextMessage TextMessage::createClearMessage() {
    TextMessage msg;
    msg.type = MessageType::CLEAR_TEXT;
    msg.textLength = 0;
    memset(msg.text, 0, sizeof(msg.text));
    return msg;
}

TextMessage TextMessage::createHeartbeat() {
    TextMessage msg;
    msg.type = MessageType::HEARTBEAT;
    msg.textLength = 0;
    memset(msg.text, 0, sizeof(msg.text));
    return msg;
}

std::string TextMessage::getText() const {
    return std::string(text, textLength);
}

void TextMessage::setText(const std::string& newText) {
    textLength = std::min(newText.length(), sizeof(text) - 1);
    memcpy(text, newText.c_str(), textLength);
    text[textLength] = '\0';
}

size_t TextMessage::serialize(uint8_t* buffer, size_t bufferSize) const {
    const size_t requiredSize = sizeof(TextMessage);
    if (bufferSize < requiredSize || buffer == nullptr) {
        return 0;
    }

    // Validate internal state before serialization
    if (textLength >= sizeof(text)) {
        return 0;  // Invalid state - prevent buffer overflow
    }

    // Validate message type
    if (type != MessageType::TEXT_UPDATE &&
        type != MessageType::CLEAR_TEXT &&
        type != MessageType::HEARTBEAT) {
        return 0;  // Invalid message type
    }

    // Safe copy with validation
    memcpy(buffer, this, requiredSize);
    return requiredSize;
}

bool TextMessage::deserialize(const uint8_t* buffer, size_t bufferSize) {
    if (bufferSize < sizeof(TextMessage) || buffer == nullptr) {
        return false;
    }

    // Safe copy
    memcpy(this, buffer, sizeof(TextMessage));

    // Validate and sanitize all fields
    if (textLength >= sizeof(text)) {
        textLength = sizeof(text) - 1;  // Cap at maximum safe length
    }

    // Validate message type
    if (type != MessageType::TEXT_UPDATE &&
        type != MessageType::CLEAR_TEXT &&
        type != MessageType::HEARTBEAT) {
        return false;  // Invalid message type
    }

    // Validate text size enum
    if (size != TextSize::SMALL && size != TextSize::BIG) {
        size = TextSize::SMALL;  // Default to safe value
    }

    // Ensure null termination
    text[textLength] = '\0';

    // Validate timestamp (basic sanity check)
    if (timestamp == 0) {
        timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    return true;
}

} // namespace LiveText