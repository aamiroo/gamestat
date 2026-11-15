#include <iostream>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include "overlay.h"
#include "backend.h"

int main() {
    std::cout << "GameStat starting..." << std::endl;

    // Create backend communicator
     // Handles communication with the backend Python process via pipe
    BackendCommunicator communicator;
    if (!communicator.start()) {
        std::cerr << "Failed to start backend communicator" << std::endl;
        return 1;
    }

    // Wait for initial data
     // Allow time for backend to start sending data
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Create and initialize overlay
     // Sets up the ImGui overlay window with specified dimensions and title
    Overlay overlay;
    if (!overlay.init(240, 130, "GameStat Overlay")) {
        std::cerr << "Failed to initialize overlay" << std::endl;
        communicator.stop();
        return 1;
    }

    // Run the overlay main loop
     // Main loop that renders ImGui metrics and handles input
    overlay.run(communicator);

    // Cleanup
    overlay.cleanup();
    communicator.stop();

    return 0;
}