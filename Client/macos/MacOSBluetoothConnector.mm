#include "MacOSBluetoothConnector.h"
#import <IOBluetooth/IOBluetooth.h>
#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <limits>
#include <vector>

struct MacOSBluetoothState {
    struct Write {
        std::vector<unsigned char> bytes;
        bool started{false}, done{false};
        IOReturn status{kIOReturnNotReady};
        size_t length{0};
    };
    std::mutex mutex;
    std::condition_variable changed;
    std::atomic<bool> stop{false}, connected{false};
    bool openDone{false};
    IOReturn openStatus{kIOReturnNotReady};
    SonyProtocolVersion version{SonyProtocolVersion::V1};
    std::string error;
    std::deque<unsigned char> received;
    std::shared_ptr<Write> write;

    void cancel() noexcept {
        stop = true; connected = false;
        changed.notify_all();
    }
};

@interface SonyRfcommDelegate : NSObject <IOBluetoothRFCOMMChannelDelegate> {
@public
    std::shared_ptr<MacOSBluetoothState> state;
}
@end
@implementation SonyRfcommDelegate
- (void)rfcommChannelOpenComplete:(IOBluetoothRFCOMMChannel*)channel status:(IOReturn)status {
    std::lock_guard lock(state->mutex);
    state->openDone = true;
    state->openStatus = status;
    // A late completion must never revive a cancelled connection.
    if (!state->stop && status == kIOReturnSuccess) state->connected = true;
    state->changed.notify_all();
}
- (void)rfcommChannelClosed:(IOBluetoothRFCOMMChannel*)channel {
    // No closeChannel, join, or connector access inside a native callback.
    state->cancel();
}
- (void)rfcommChannelData:(IOBluetoothRFCOMMChannel*)channel data:(void*)data length:(size_t)length {
    std::lock_guard lock(state->mutex);
    if (state->stop) return;
    if (length > 65536 || state->received.size() > 65536 - length) {
        state->cancel(); return;
    }
    const auto* bytes = static_cast<const unsigned char*>(data);
    state->received.insert(state->received.end(), bytes, bytes + length);
    state->changed.notify_all();
}
- (void)rfcommChannelWriteComplete:(IOBluetoothRFCOMMChannel*)channel refcon:(void*)refcon status:(IOReturn)status {
    std::lock_guard lock(state->mutex);
    if (state->write && state->write.get() == refcon) {
        state->write->status = status;
        state->write->done = true;
        if (status != kIOReturnSuccess) state->cancel();
        state->changed.notify_all();
    }
}
@end

namespace {
using Clock = std::chrono::steady_clock;
// IOBluetooth may already have queued a callback when setDelegate:nil runs.
// Keep the callback receiver and its state alive across a main-run-loop turn.
// See also Chromium's FB13705522 workaround in bluetooth_rfcomm_channel_mac.mm.
void releaseLater(CFTypeRef object) {
    if (!object) return;
    dispatch_async(dispatch_get_main_queue(), ^{ CFRelease(object); });
}
void pump(const std::shared_ptr<MacOSBluetoothState>& state) {
    @autoreleasepool {
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.005]];
    }
    // runMode can return immediately when there are no registered sources.
    std::unique_lock lock(state->mutex);
    state->changed.wait_for(lock, std::chrono::milliseconds(5), [&] { return state->stop.load(); });
}
void runConnection(const std::shared_ptr<MacOSBluetoothState>& state, const std::string& address,
                   Clock::time_point deadline, const std::function<void*(const std::string&)>& factory) {
    @autoreleasepool {
        CFTypeRef retainedDevice = nullptr, retainedChannel = nullptr, retainedDelegate = nullptr;
        IOBluetoothRFCOMMChannel* channel = nil;
        try {
            IOBluetoothDevice* device = factory ? (__bridge IOBluetoothDevice*)factory(address)
                : [IOBluetoothDevice deviceWithAddressString:[NSString stringWithUTF8String:address.c_str()]];
            if (!device) throw std::runtime_error("Invalid Bluetooth device address");
            retainedDevice = CFRetain((__bridge CFTypeRef)device);
            auto* v1 = [IOBluetoothSDPUUID uuidWithBytes:(void*)SERVICE_UUID_IN_BYTES length:16];
            auto* v2 = [IOBluetoothSDPUUID uuidWithBytes:(void*)SERVICE_UUID_V2_IN_BYTES length:16];
            auto* record = [device getServiceRecordForUUID:v1];
            if (!record) { record = [device getServiceRecordForUUID:v2]; state->version = SonyProtocolVersion::V2; }
            if (!record) throw std::runtime_error("No cached Sony RFCOMM service; pair the headset in Bluetooth settings");
            UInt8 channelId = 0;
            if ([record getRFCOMMChannelID:&channelId] != kIOReturnSuccess)
                throw std::runtime_error("Sony service has no RFCOMM channel");
            SonyRfcommDelegate* delegate = [[SonyRfcommDelegate alloc] init];
            delegate->state = state;
#if __has_feature(objc_arc)
            retainedDelegate = CFBridgingRetain(delegate);
#else
            retainedDelegate = (CFTypeRef)delegate;
#endif
            // This asynchronous API also opens the underlying connection. Do not
            // precede it with synchronous openConnection on the Qt worker.
            if (!state->stop) {
                const auto status = [device openRFCOMMChannelAsync:&channel withChannelID:channelId delegate:delegate];
                if (channel) retainedChannel = CFRetain((__bridge CFTypeRef)channel);
                if (status != kIOReturnSuccess) throw std::runtime_error("Cannot start RFCOMM opening (" + std::to_string(status) + ")");
            }
            while (!state->stop) {
                {
                    std::lock_guard lock(state->mutex);
                    if (state->openDone && state->openStatus != kIOReturnSuccess)
                        throw std::runtime_error("RFCOMM opening failed (" + std::to_string(state->openStatus) + ")");
                }
                if (!state->connected && Clock::now() >= deadline)
                    throw std::runtime_error("RFCOMM opening timed out");
                std::shared_ptr<MacOSBluetoothState::Write> write;
                {
                    std::lock_guard lock(state->mutex);
                    if (state->connected && state->write && !state->write->started) {
                        write = state->write;
                        write->started = true;
                    }
                }
                if (write) {
                    const auto mtu = channel ? [channel getMTU] : 0;
                    if (!mtu) throw std::runtime_error("Invalid RFCOMM write MTU");
                    write->length = std::min(write->bytes.size(), static_cast<size_t>(mtu));
                    const auto status = [channel writeAsync:write->bytes.data() length:static_cast<UInt16>(write->length) refcon:write.get()];
                    if (status != kIOReturnSuccess) {
                        std::lock_guard lock(state->mutex);
                        write->status = status; write->done = true;
                        state->cancel();
                    }
                }
                pump(state);
            }
        } catch (const std::exception& error) {
            std::lock_guard lock(state->mutex);
            state->error = error.what();
            state->cancel();
        }
        state->cancel();
        // Only the native owner accesses or releases these objects. Detach first
        // so closing cannot recurse into teardown; keep write buffers in state
        // alive until queued native callbacks have drained.
        [channel setDelegate:nil];
        [channel closeChannel];
        releaseLater(retainedChannel);
        releaseLater(retainedDelegate);
        if (retainedDevice) CFRelease(retainedDevice);
    }
}
}

MacOSBluetoothConnector::~MacOSBluetoothConnector() { disconnect(); }
std::shared_ptr<MacOSBluetoothState> MacOSBluetoothConnector::state() const {
    std::lock_guard lock(_stateMutex);
    return _state;
}
void MacOSBluetoothConnector::cancelPending() noexcept {
    if (auto current = state()) current->cancel();
}
void MacOSBluetoothConnector::disconnect() noexcept {
    cancelPending();
    std::lock_guard lifecycle(_lifecycleMutex);
    // Native callbacks never call this function, so there is no self-join path.
    if (_thread.joinable()) _thread.join();
}
void MacOSBluetoothConnector::connect(const std::string& address) {
    std::lock_guard lifecycle(_lifecycleMutex);
    cancelPending();
    if (_thread.joinable()) _thread.join();
    auto current = std::make_shared<MacOSBluetoothState>();
    {
        std::lock_guard lock(_stateMutex);
        _state = current;
    }
    const auto deadline = Clock::now() + _connectTimeout;
    _thread = std::thread(runConnection, current, address, deadline, _deviceFactory);
    std::unique_lock lock(current->mutex);
    const bool completed = current->changed.wait_until(lock, deadline, [&] { return current->connected || current->stop; });
    if (!completed || !current->connected) {
        const auto message = current->error.empty() ? (completed ? "RFCOMM opening cancelled or closed" : "RFCOMM opening timed out") : current->error;
        current->cancel();
        lock.unlock();
        _thread.join();
        throw RecoverableException(message, true);
    }
}
int MacOSBluetoothConnector::send(char* buffer, size_t length) {
    if (!length) return 0;
    std::lock_guard sending(_sendMutex);
    auto current = state();
    if (!current || !current->connected) throw RecoverableException("Bluetooth disconnected", true);
    auto write = std::make_shared<MacOSBluetoothState::Write>();
    write->bytes.assign(buffer, buffer + length);
    std::unique_lock lock(current->mutex);
    current->write = write;
    current->changed.notify_all();
    const bool completed = current->changed.wait_for(lock, _writeTimeout, [&] { return write->done || current->stop; });
    if (!completed || !write->done || write->status != kIOReturnSuccess) {
        current->cancel();
        throw RecoverableException(completed ? "Bluetooth write failed or cancelled" : "Bluetooth write timed out", true);
    }
    current->write.reset();
    return static_cast<int>(write->length);
}
int MacOSBluetoothConnector::recv(char* buffer, size_t length) {
    if (!length) return 0;
    auto current = state();
    if (!current) throw RecoverableException("Bluetooth disconnected", true);
    std::unique_lock lock(current->mutex);
    if (!current->changed.wait_for(lock, std::chrono::milliseconds(2500), [&] { return current->stop || !current->received.empty(); }))
        throw RecoverableException("recv timed out", false);
    if (current->stop) throw RecoverableException("Bluetooth disconnected", true);
    const auto count = std::min(length, current->received.size());
    std::copy_n(current->received.begin(), count, buffer);
    current->received.erase(current->received.begin(), current->received.begin() + count);
    return static_cast<int>(count);
}
bool MacOSBluetoothConnector::isConnected() noexcept {
    auto current = state();
    return current && current->connected;
}
SonyProtocolVersion MacOSBluetoothConnector::getProtocolVersion() noexcept {
    auto current = state();
    return current && current->connected ? current->version : SonyProtocolVersion::V1;
}
std::vector<BluetoothDevice> MacOSBluetoothConnector::getConnectedDevices() {
    @autoreleasepool {
        std::vector<BluetoothDevice> result;
        // Preserve renamed Sony discovery via the OUI/name filter in sony-transport,
        // plus paired audio/BLE devices that isConnected does not report as live.
        for (IOBluetoothDevice* device in [IOBluetoothDevice pairedDevices]) {
            if (![device addressString]) continue;
            result.push_back({[device name] ? [[device name] UTF8String] : "Unknown Device",
                [[device addressString] UTF8String], true, [device isConnected] == YES});
        }
        return result;
    }
}
