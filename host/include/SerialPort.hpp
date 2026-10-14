#pragma once

#include "SerialTypes.hpp"
#include <string>
#include <vector>
#include <sys/types.h>

namespace serial {

class SerialPort {
public:
    SerialPort();
    ~SerialPort();

    // Disable copy
    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    bool open(const std::string& path, const SerialConfig& config);
    void close();
    ssize_t write(const std::vector<uint8_t>& data);
    ssize_t read(uint8_t* buffer, size_t maxLen);
    
    int getFileDescriptor() const;
    bool isOpen() const;

private:
    int m_fd;
    
    bool configure(const SerialConfig& config);
    int getBaudRateCode(uint32_t baudRate);
};

} // namespace serial
