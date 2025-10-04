#include "SenderApp.h"
#include <iostream>

int main() {
    try {
        LiveText::SenderApp app;

        if (!app.initialize()) {
            std::cerr << "Failed to initialize sender application" << std::endl;
            return 1;
        }

        std::cout << "Live Text Sender started successfully" << std::endl;
        std::cout << "Press ESC or close window to exit" << std::endl;

        app.run();
        app.shutdown();

        std::cout << "Application shutdown complete" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Application error: " << e.what() << std::endl;
        return 1;
    }
}