#pragma once
#include "../IBluetoothConnector.h"
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

struct MacOSBluetoothState;

// Native objects and their run loop belong to one worker. Callbacks hold shared
// state, never a pointer back to a connector that may already be destroyed.
class MacOSBluetoothConnector final : public IBluetoothConnector {
public:
    MacOSBluetoothConnector() = default;
    ~MacOSBluetoothConnector() override;
    void connect(const std::string& address) override;
    int send(char* buffer, size_t length) override;
    int recv(char* buffer, size_t length) override;
    void disconnect() noexcept override;
    void cancelPending() noexcept override;
    bool isConnected() noexcept override;
    SonyProtocolVersion getProtocolVersion() noexcept override;
    std::vector<BluetoothDevice> getConnectedDevices() override;
private:
    friend struct MacOSBluetoothConnectorTestAccess;
    std::shared_ptr<MacOSBluetoothState> state() const;
    mutable std::mutex _stateMutex;
    std::mutex _lifecycleMutex, _sendMutex;
    std::shared_ptr<MacOSBluetoothState> _state;
    std::thread _thread;
    std::chrono::milliseconds _connectTimeout{10000}, _writeTimeout{2500};
    // Test seam: an unowned object implementing the native device selectors.
    std::function<void*(const std::string&)> _deviceFactory;
};
