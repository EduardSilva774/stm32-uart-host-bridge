#include "SerialPort.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <iostream>
#include <cstring>

namespace serial {

SerialPort::SerialPort() : m_fd(-1) {}

SerialPort::~SerialPort() {
    close();
}

bool SerialPort::open(const std::string& path, const SerialConfig& config) {
    if (isOpen()) {
        close();
    }

    m_fd = ::open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (m_fd < 0) {
        return false;
    }

    if (!configure(config)) {
        close();
        return false;
    }

    return true;
}

void SerialPort::close() {
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

ssize_t SerialPort::write(const std::vector<uint8_t>& data) {
    if (!isOpen() || data.empty()) {
        return -1;
    }
    return ::write(m_fd, data.data(), data.size());
}

ssize_t SerialPort::read(uint8_t* buffer, size_t maxLen) {
    if (!isOpen() || buffer == nullptr || maxLen == 0) {
        return -1;
    }
    return ::read(m_fd, buffer, maxLen);
}

int SerialPort::getFileDescriptor() const {
    return m_fd;
}

bool SerialPort::isOpen() const {
    return m_fd >= 0;
}

int SerialPort::getBaudRateCode(uint32_t baudRate) {
    switch (baudRate) {
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
        case 460800: return B460800;
        case 921600: return B921600;
        case 1000000: return B1000000;
        case 2000000: return B2000000;
        case 3000000: return B3000000;
        default: return B115200; // Default
    }
}

bool SerialPort::configure(const SerialConfig& config) {
    struct termios tty;
    if (tcgetattr(m_fd, &tty) != 0) {
        return false;
    }

    // Set baud rate
    int baudCode = getBaudRateCode(config.baudRate);
    cfsetospeed(&tty, baudCode);
    cfsetispeed(&tty, baudCode);

    // 8N1 default or configurable
    tty.c_cflag &= ~CSIZE;
    switch (config.dataBits) {
        case 5: tty.c_cflag |= CS5; break;
        case 6: tty.c_cflag |= CS6; break;
        case 7: tty.c_cflag |= CS7; break;
        case 8: default: tty.c_cflag |= CS8; break;
    }

    if (config.stopBits == 2) {
        tty.c_cflag |= CSTOPB;
    } else {
        tty.c_cflag &= ~CSTOPB;
    }

    switch (config.parity) {
        case ParityMode::EVEN:
            tty.c_cflag |= PARENB;
            tty.c_cflag &= ~PARODD;
            break;
        case ParityMode::ODD:
            tty.c_cflag |= PARENB;
            tty.c_cflag |= PARODD;
            break;
        case ParityMode::NONE:
        default:
            tty.c_cflag &= ~PARENB;
            break;
    }

    // Disable hardware flow control
    tty.c_cflag &= ~CRTSCTS;
    // Enable receiver, ignore modem control lines
    tty.c_cflag |= CREAD | CLOCAL;

    // Disable canonical mode, echo, and signals
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    // Disable software flow control
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    // Disable special handling of bytes
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

    // Disable output processing
    tty.c_oflag &= ~(OPOST | ONLCR);

    // VMIN and VTIME
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0; // Non-blocking read

    if (tcsetattr(m_fd, TCSANOW, &tty) != 0) {
        return false;
    }

    return true;
}

} // namespace serial
