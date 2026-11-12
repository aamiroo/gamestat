#include <iostream>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include "overlay.h"
#include "backend.h"

int main() {
    std::cout << "GameStat starting..." << std::endl;

    // Create backend communicator
    BackendCommunicator communicator;
    if (!communicator.start()) {
        std::cerr << "Failed to start backend communicator" << std::endl;
        return 1;
    }

    // Wait for initial data
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Create and initialize overlay
    Overlay overlay;
    if (!overlay.init(240, 130, "GameStat Overlay")) {
        std::cerr << "Failed to initialize overlay" << std::endl;
        communicator.stop();
        return 1;
    }

    // Run the overlay main loop
    overlay.run(communicator);

    // Cleanup
    overlay.cleanup();
    communicator.stop();

    return 0;
}