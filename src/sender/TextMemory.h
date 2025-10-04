#pragma once
#include <vector>
#include <string>
#include <chrono>
#include <mutex>

namespace LiveText {

struct TextMemoryEntry {
    std::string text;
    uint64_t displayedAt;
    uint64_t displayDuration;

    TextMemoryEntry() : displayedAt(0), displayDuration(0) {}
    TextMemoryEntry(const std::string& text, uint64_t displayedAt, uint64_t displayDuration)
        : text(text), displayedAt(displayedAt), displayDuration(displayDuration) {}
};

class TextMemory {
public:
    TextMemory();

    void recordText(const std::string& text);
    void onTextCleared();
    void checkForStaticText(); // Check if current text should be moved to memory

    std::vector<TextMemoryEntry> getEntries() const;
    size_t getEntryCount() const;

private:
    mutable std::mutex memoryMutex_;
    std::vector<TextMemoryEntry> entries_;
    std::string currentText_;
    uint64_t textDisplayStartTime_;
    bool currentTextAddedToMemory_;  // Flag to prevent duplicate entries
    static constexpr uint64_t MIN_DISPLAY_DURATION_MS = 3000;  // 3 seconds

    uint64_t getCurrentTimestamp() const;
};

} // namespace LiveText