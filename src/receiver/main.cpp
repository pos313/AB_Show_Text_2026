#include "ReceiverApp.h"
#include <iostream>

int main() {
    try {
        LiveText::ReceiverApp app;

        if (!app.initialize()) {
            std::cerr << "Failed to initialize receiver application" << std::endl;
            return 1;
        }

        std::cout << "Live Text Receiver started successfully" << std::endl;
        std::cout << "Controls:" << std::endl;
        std::cout << "  ESC - Exit application" << std::endl;
        std::cout << "  H   - Print health status" << std::endl;
        std::cout << "  S   - Print subscriber statistics" << std::endl;
        std::cout << std::endl;
        std::cout << "Spout sender name: LiveText" << std::endl;
        std::cout << "Waiting for text messages..." << std::endl;

        app.run();
        app.shutdown();

        std::cout << "Application shutdown complete" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Application error: " << e.what() << std::endl;
        return 1;
    }
}