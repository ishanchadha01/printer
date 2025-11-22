#include "include/workers/controller.hpp"
#include <iostream>

int main()
{
    Controller main_controller;
    std::cout << "Starting controller... waiting for shutdown command.\n";
    std::cout << "UI: http://localhost:3000 (npm start)\nAPI: http://localhost:8000 (Flask viz)\n";
    std::cout << "Logs: /tmp/ui.log and /tmp/visualization-server.log\n";
    std::cout << "Set AUTO_START_FRONTENDS=0 to skip auto-launch.\n";
    main_controller.run();
    std::cout << "Controller stopped.\n";
    return 0;
};
