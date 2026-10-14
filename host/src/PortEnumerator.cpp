#include "PortEnumerator.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

namespace serial {

std::vector<DeviceInfo> PortEnumerator::listPorts() {
    std::vector<DeviceInfo> ports;
    const std::string sysClassTty = "/sys/class/tty/";

    if (!fs::exists(sysClassTty)) {
        return ports;
    }

    for (const auto& entry : fs::directory_iterator(sysClassTty)) {
        if (!entry.is_symlink() && !entry.is_directory()) continue;

        std::string ttyName = entry.path().filename().string();
        std::string devicePath = entry.path().string() + "/device";

        if (!fs::exists(devicePath)) continue;

        DeviceInfo info;
        info.portPath = "/dev/" + ttyName;

        // Try to find idVendor and idProduct by going up the device tree
        fs::path currentPath = fs::canonical(devicePath);

        while (currentPath != "/" && currentPath.string().find("/sys/devices") != std::string::npos) {
            fs::path vendorPath = currentPath / "idVendor";
            fs::path productPath = currentPath / "idProduct";

            if (fs::exists(vendorPath) && fs::exists(productPath)) {
                std::ifstream vendorFile(vendorPath);
                std::ifstream productFile(productPath);
                
                if (vendorFile >> info.vendorId && productFile >> info.productId) {
                    // Trim newlines if any
                    info.vendorId.erase(info.vendorId.find_last_not_of(" \n\r\t") + 1);
                    info.productId.erase(info.productId.find_last_not_of(" \n\r\t") + 1);
                }
                
                fs::path manufacturerPath = currentPath / "manufacturer";
                fs::path productDescPath = currentPath / "product";
                std::string mfg, prod;
                
                if (fs::exists(manufacturerPath)) {
                    std::ifstream mfgFile(manufacturerPath);
                    std::getline(mfgFile, mfg);
                }
                if (fs::exists(productDescPath)) {
                    std::ifstream prodFile(productDescPath);
                    std::getline(prodFile, prod);
                }
                
                if (!mfg.empty() && !prod.empty()) {
                    info.description = mfg + " " + prod;
                } else if (!prod.empty()) {
                    info.description = prod;
                } else if (!mfg.empty()) {
                    info.description = mfg;
                }
                
                break;
            }
            currentPath = currentPath.parent_path();
        }
        
        ports.push_back(info);
    }

    return ports;
}

std::optional<DeviceInfo> PortEnumerator::findByVidPid(const std::string& vid, const std::string& pid) {
    auto ports = listPorts();
    for (const auto& port : ports) {
        if (port.vendorId == vid && port.productId == pid) {
            return port;
        }
    }
    return std::nullopt;
}

} // namespace serial
