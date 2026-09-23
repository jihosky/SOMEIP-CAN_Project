#include "vehicle_someip/vehicle_data_provider.hpp"

#include "vehicle_service/vehicle_data.hpp"
#include "vehicle_service/vehicle_service.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc != 1 && !(argc == 2 && std::string(argv[1]) == "--demo")) {
        std::cerr << "Usage: " << argv[0] << " [--demo]\n";
        return 2;
    }

    vehicle_service::VehicleService service;
    if (argc == 2) {
        service.update({123.45, 2500, 85, std::chrono::system_clock::now()});
    }
    auto application = vsomeip::runtime::get()->create_application("vehicle-provider");
    vehicle_someip::VehicleDataProvider provider(application, service);
    const char* configuration = std::getenv("VSOMEIP_CONFIGURATION");
    std::cerr << "[SOMEIP] application=" << application->get_name()
              << " VSOMEIP_CONFIGURATION="
              << (configuration ? configuration : "(unset)") << std::endl;
    if (!provider.init()) {
        std::cerr << "Could not initialize vSomeIP provider; check VSOMEIP_CONFIGURATION\n";
        return 1;
    }
    std::cout << "VehicleDataService provider ready"
              << (argc == 2 ? " (demo values)" : " (no data)")
              << "; vSomeIP registration pending" << std::endl;
    provider.start();
    return 0;
}
