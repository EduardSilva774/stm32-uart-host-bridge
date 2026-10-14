#include "AsyncSerialReceiver.hpp"
#include <poll.h>
#include <unistd.h>
#include <iostream>
#include <algorithm>

namespace serial {

AsyncSerialReceiver::AsyncSerialReceiver(SerialPort& serialPort)
    : m_serialPort(serialPort), m_isRunning(false) {}

AsyncSerialReceiver::~AsyncSerialReceiver() {
    stop();
}

void AsyncSerialReceiver::start() {
    if (m_isRunning.exchange(true)) {
        return; // Already running
    }
    m_workerThread = std::thread(&AsyncSerialReceiver::receiveLoop, this);
}

void AsyncSerialReceiver::stop() {
    if (m_isRunning.exchange(false)) {
        if (m_workerThread.joinable()) {
            m_workerThread.join();
        }
    }
}

void AsyncSerialReceiver::addObserver(ISerialObserver* observer) {
    std::lock_guard<std::mutex> lock(m_observersMutex);
    if (std::find(m_observers.begin(), m_observers.end(), observer) == m_observers.end()) {
        m_observers.push_back(observer);
    }
}

void AsyncSerialReceiver::removeObserver(ISerialObserver* observer) {
    std::lock_guard<std::mutex> lock(m_observersMutex);
    m_observers.erase(std::remove(m_observers.begin(), m_observers.end(), observer), m_observers.end());
}

void AsyncSerialReceiver::receiveLoop() {
    const size_t bufferSize = 4096;
    std::vector<uint8_t> buffer(bufferSize);

    while (m_isRunning) {
        int fd = m_serialPort.getFileDescriptor();
        if (fd < 0) {
            notifyError("Invalid file descriptor. Port might be closed.");
            m_isRunning = false;
            break;
        }

        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLIN | POLLERR | POLLHUP;

        int ret = poll(&pfd, 1, 100); // 100ms timeout

        if (ret < 0) {
            // Error in poll
            notifyError("poll() returned error");
            m_isRunning = false;
            break;
        } else if (ret == 0) {
            // Timeout, loop again to check m_isRunning
            continue;
        } else {
            // Events occurred
            if (pfd.revents & (POLLERR | POLLHUP)) {
                notifyClosed();
                m_isRunning = false;
                break;
            }

            if (pfd.revents & POLLIN) {
                ssize_t bytesRead = m_serialPort.read(buffer.data(), buffer.size());
                if (bytesRead > 0) {
                    std::vector<uint8_t> data(buffer.begin(), buffer.begin() + bytesRead);
                    notifyData(data);
                } else if (bytesRead == 0) {
                    // EOF or disconnected
                    notifyClosed();
                    m_isRunning = false;
                    break;
                } else {
                    // bytesRead < 0
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        notifyError("read() failed");
                        notifyClosed();
                        m_isRunning = false;
                        break;
                    }
                }
            }
        }
    }
}

void AsyncSerialReceiver::notifyData(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(m_observersMutex);
    for (auto* obs : m_observers) {
        obs->onDataReceived(data);
    }
}

void AsyncSerialReceiver::notifyError(const std::string& errorMsg) {
    std::lock_guard<std::mutex> lock(m_observersMutex);
    for (auto* obs : m_observers) {
        obs->onError(errorMsg);
    }
}

void AsyncSerialReceiver::notifyClosed() {
    std::lock_guard<std::mutex> lock(m_observersMutex);
    for (auto* obs : m_observers) {
        obs->onConnectionClosed();
    }
}

} // namespace serial
