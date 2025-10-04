#include "TextMemory.h"

namespace LiveText {

TextMemory::TextMemory()
    : textDisplayStartTime_(0)
    , currentTextAddedToMemory_(false)
{
}

void TextMemory::recordText(const std::string& text) {
    std::lock_guard<std::mutex> lock(memoryMutex_);

    if (text.empty()) {
        return;
    }

    // If this is a new text or different from current
    if (text != currentText_) {
        // Save previous text if it was displayed long enough
        if (!currentText_.empty() && textDisplayStartTime_ > 0) {
            uint64_t displayDuration = getCurrentTimestamp() - textDisplayStartTime_;
            if (displayDuration >= MIN_DISPLAY_DURATION_MS) {
                entries_.insert(entries_.begin(),
                               TextMemoryEntry(currentText_, textDisplayStartTime_, displayDuration));

                // Keep only last 100 entries
                if (entries_.size() > 100) {
                    entries_.resize(100);
                }
            }
        }

        currentText_ = text;
        textDisplayStartTime_ = getCurrentTimestamp();
        currentTextAddedToMemory_ = false;
    }
}

void TextMemory::onTextCleared() {
    std::lock_guard<std::mutex> lock(memoryMutex_);

    if (!currentText_.empty() && textDisplayStartTime_ > 0) {
        uint64_t displayDuration = getCurrentTimestamp() - textDisplayStartTime_;
        if (displayDuration >= MIN_DISPLAY_DURATION_MS) {
            entries_.insert(entries_.begin(),
                           TextMemoryEntry(currentText_, textDisplayStartTime_, displayDuration));

            // Keep only last 100 entries
            if (entries_.size() > 100) {
                entries_.resize(100);
            }
        }
    }

    currentText_.clear();
    textDisplayStartTime_ = 0;
    currentTextAddedToMemory_ = false;
}

void TextMemory::checkForStaticText() {
    std::lock_guard<std::mutex> lock(memoryMutex_);

    if (!currentText_.empty() && textDisplayStartTime_ > 0 && !currentTextAddedToMemory_) {
        uint64_t displayDuration = getCurrentTimestamp() - textDisplayStartTime_;
        if (displayDuration >= MIN_DISPLAY_DURATION_MS) {
            // Text has been static for >3 seconds, move to memory
            entries_.insert(entries_.begin(),
                           TextMemoryEntry(currentText_, textDisplayStartTime_, displayDuration));

            // Keep only last 100 entries
            if (entries_.size() > 100) {
                entries_.resize(100);
            }

            // Mark this text as already added to prevent duplicates
            currentTextAddedToMemory_ = true;
        }
    }
}

std::vector<TextMemoryEntry> TextMemory::getEntries() const {
    std::lock_guard<std::mutex> lock(memoryMutex_);
    return entries_;
}

size_t TextMemory::getEntryCount() const {
    std::lock_guard<std::mutex> lock(memoryMutex_);
    return entries_.size();
}

uint64_t TextMemory::getCurrentTimestamp() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

} // namespace LiveText