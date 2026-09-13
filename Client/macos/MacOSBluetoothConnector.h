#pragma once

#include <stdio.h>

#include "../IBluetoothConnector.h"
#include "IOBluetooth/IOBluetooth.h"
#include "Constants.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>
#include <thread>

class MacOSBluetoothConnector final : public IBluetoothConnector
{
public:
    MacOSBluetoothConnector();
    ~MacOSBluetoothConnector();

    static void connectToMac(
        MacOSBluetoothConnector* macOSBluetoothConnector,
        std::promise<void> connectPromise) noexcept(false);

    virtual void connect(const std::string& addrStr) noexcept(false);
    virtual int send(char* buf, size_t length) noexcept(false);
    virtual int recv(char* buf, size_t length) noexcept(false);
    virtual void disconnect() noexcept;
    virtual bool isConnected() noexcept;
    virtual void closeConnection();
    virtual SonyProtocolVersion getProtocolVersion() noexcept;
    virtual std::vector<BluetoothDevice> getConnectedDevices() noexcept(false);

    // Called by the Objective-C RFCOMM delegate. It only updates C++ state and
    // wakes waiters; it must never close the channel or join the worker thread.
    void channelClosed() noexcept;

    std::deque<std::vector<unsigned char>> receivedBytes;
    std::mutex receiveDataMutex;
    std::condition_variable receiveDataConditionVariable;
    std::atomic<bool> running = false;
    std::mutex disconnectionMutex;
    std::condition_variable disconnectionConditionVariable;

    // Set on the connectToMac background thread before the connect promise
    // resolves; future::get() synchronizes the subsequent read on callers.
    SonyProtocolVersion protocolVersion = SonyProtocolVersion::V1;

private:
    void* rfcommDevice = nullptr;
    void* rfcommchannel = nullptr;
    std::thread uthread;

    // Guards only local closeChannel requests. A remote RFCOMM closure must
    // not set this value, because disconnect() may still need to detach the
    // delegate and perform one final local close safely.
    std::atomic<bool> channelCloseRequested = false;
};