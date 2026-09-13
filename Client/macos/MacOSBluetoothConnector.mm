#include "MacOSBluetoothConnector.h"

MacOSBluetoothConnector::MacOSBluetoothConnector()
{
}

MacOSBluetoothConnector::~MacOSBluetoothConnector()
{
    // A joinable std::thread in a destructor calls std::terminate(). Always
    // request shutdown and join from the owning thread before members die.
    disconnect();
}

@interface AsyncCommDelegate : NSObject <IOBluetoothRFCOMMChannelDelegate> {
@public
    MacOSBluetoothConnector* delegateCPP;
}
@end

@implementation AsyncCommDelegate

- (void)rfcommChannelClosed:(IOBluetoothRFCOMMChannel *)rfcommChannel
{
#ifdef SHC_DEBUG_PROTOCOL
    fprintf(stderr, "[connect] rfcommChannelClosed\n");
#endif

    // This callback may execute on the RFCOMM worker thread or while a local
    // close is in progress. Do not recurse into disconnect() or call join().
    delegateCPP->channelClosed();
}

#ifdef SHC_DEBUG_PROTOCOL
- (void)rfcommChannelOpenComplete:(IOBluetoothRFCOMMChannel *)rfcommChannel
                            status:(IOReturn)error
{
    fprintf(stderr, "[connect] rfcommChannelOpenComplete status=0x%x\n", error);
}
#endif

- (void)rfcommChannelData:(IOBluetoothRFCOMMChannel *)rfcommChannel
                     data:(void *)dataPointer
                   length:(size_t)dataLength
{
#ifdef SHC_DEBUG_PROTOCOL
    fprintf(stderr, "[recv-callback] %zu bytes\n", dataLength);
#endif

    std::lock_guard<std::mutex> guard(delegateCPP->receiveDataMutex);

    unsigned char* buffer = static_cast<unsigned char*>(dataPointer);
    std::vector<unsigned char> vectorBuffer(buffer, buffer + dataLength);

    delegateCPP->receivedBytes.push_back(std::move(vectorBuffer));
    delegateCPP->receiveDataConditionVariable.notify_one();
}

@end

#ifdef SHC_DEBUG_PROTOCOL
static void _debugHexDump(const char* label, const char* buf, size_t length)
{
    fprintf(stderr, "[%s] ", label);
    for (size_t i = 0; i < length; ++i) {
        fprintf(stderr, "%02x ", static_cast<unsigned char>(buf[i]));
    }
    fprintf(stderr, "\n");
}
#endif

int MacOSBluetoothConnector::send(char* buf, size_t length)
{
#ifdef SHC_DEBUG_PROTOCOL
    _debugHexDump("send", buf, length);
#endif

    IOBluetoothRFCOMMChannel* channel =
        (__bridge IOBluetoothRFCOMMChannel*)rfcommchannel;

    if (!running || channel == nil || !channel.isOpen) {
        throw RecoverableException("Bluetooth RFCOMM channel is not connected", false);
    }

    [channel writeSync:buf length:length];
    return static_cast<int>(length);
}

void MacOSBluetoothConnector::connectToMac(
    MacOSBluetoothConnector* macOSBluetoothConnector,
    std::promise<void> connectPromise)
{
    IOBluetoothDevice* device =
        (__bridge IOBluetoothDevice*)macOSBluetoothConnector->rfcommDevice;
    IOBluetoothRFCOMMChannel* channel = [[IOBluetoothRFCOMMChannel alloc] init];

    IOBluetoothSDPUUID* sppServiceUUIDV1 =
        [IOBluetoothSDPUUID uuidWithBytes:(void*)SERVICE_UUID_IN_BYTES length:16];
    IOBluetoothSDPServiceRecord* sppServiceRecord =
        [device getServiceRecordForUUID:sppServiceUUIDV1];
    SonyProtocolVersion protocolVersion = SonyProtocolVersion::V1;

#ifdef SHC_DEBUG_PROTOCOL
    fprintf(stderr, "[connect] v1 getServiceRecordForUUID -> %s\n",
        sppServiceRecord == nil ? "nil" : "found");
#endif

    if (sppServiceRecord == nil) {
        IOBluetoothSDPUUID* sppServiceUUIDV2 =
            [IOBluetoothSDPUUID uuidWithBytes:(void*)SERVICE_UUID_V2_IN_BYTES length:16];
        sppServiceRecord = [device getServiceRecordForUUID:sppServiceUUIDV2];
        protocolVersion = SonyProtocolVersion::V2;

#ifdef SHC_DEBUG_PROTOCOL
        fprintf(stderr, "[connect] v2 getServiceRecordForUUID -> %s\n",
            sppServiceRecord == nil ? "nil" : "found");
#endif
    }

    if (sppServiceRecord == nil) {
        connectPromise.set_exception(std::make_exception_ptr(RecoverableException(
            "Couldn't find the Sony service record on this device "
            "(neither protocol version) - is this a supported headset?", false)));
        return;
    }

    UInt8 rfcommChannelID = 0;
    IOReturn channelIdStatus = [sppServiceRecord getRFCOMMChannelID:&rfcommChannelID];

#ifdef SHC_DEBUG_PROTOCOL
    fprintf(stderr, "[connect] protocolVersion=%s getRFCOMMChannelID -> 0x%x, channelID=%u\n",
        protocolVersion == SonyProtocolVersion::V2 ? "V2" : "V1",
        channelIdStatus,
        static_cast<unsigned>(rfcommChannelID));
#endif

    if (channelIdStatus != kIOReturnSuccess) {
        connectPromise.set_exception(std::make_exception_ptr(RecoverableException(
            "Found the Sony service record, but it has no RFCOMM channel.", false)));
        return;
    }

    AsyncCommDelegate* asyncCommDelegate = [[AsyncCommDelegate alloc] init];
    asyncCommDelegate->delegateCPP = macOSBluetoothConnector;

    IOReturn openResult = [device openRFCOMMChannelAsync:&channel
                                           withChannelID:rfcommChannelID
                                                delegate:asyncCommDelegate];

#ifdef SHC_DEBUG_PROTOCOL
    fprintf(stderr, "[connect] openRFCOMMChannelAsync -> 0x%x\n", openResult);
#endif

    if (openResult != kIOReturnSuccess) {
        connectPromise.set_exception(std::make_exception_ptr(RecoverableException(
            "Could not open the RFCOMM channel.", false)));
        return;
    }

    macOSBluetoothConnector->rfcommchannel = (__bridge void*)channel;
    macOSBluetoothConnector->protocolVersion = protocolVersion;
    macOSBluetoothConnector->running = true;

    // openRFCOMMChannelAsync returns before the link is usable. Delay the
    // connect promise until the channel reports open (or we time out) so the
    // caller's initDevice()/refreshAll() writes reach the device instead of
    // throwing "channel is not connected" before the open completes and being
    // silently swallowed — which left the initial device state unread.
    const auto openDeadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(8000);
    while (!channel.isOpen && std::chrono::steady_clock::now() < openDeadline) {
        [[NSRunLoop currentRunLoop]
            runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.02]];
    }
    if (!channel.isOpen) {
        connectPromise.set_exception(std::make_exception_ptr(RecoverableException(
            "Timed out waiting for the RFCOMM channel to open.", false)));
        return;
    }

    connectPromise.set_value();

    std::unique_lock<std::mutex> lock(macOSBluetoothConnector->disconnectionMutex);
    while (macOSBluetoothConnector->running) {
        [[NSRunLoop currentRunLoop]
            runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];

        macOSBluetoothConnector->disconnectionConditionVariable.wait_for(
            lock,
            std::chrono::milliseconds(1000),
            [&]() { return !macOSBluetoothConnector->running; });
    }
}

void MacOSBluetoothConnector::connect(const std::string& addrStr)
{
    // A remotely closed prior connection leaves a completed, still-joinable
    // thread. Join it before assigning a new std::thread.
    if (uthread.joinable()) {
        disconnect();
    }

    channelCloseRequested = false;
    running = false;
    rfcommDevice = nullptr;
    rfcommchannel = nullptr;

    NSString* addressNSString = [NSString
        stringWithCString:addrStr.c_str()
                 encoding:[NSString defaultCStringEncoding]];
    IOBluetoothDevice* device =
        [IOBluetoothDevice deviceWithAddressString:addressNSString];

    if (device == nil) {
        throw RecoverableException("Could not resolve the selected Bluetooth device.", false);
    }

    if (![device isConnected]) {
        [device openConnection];
    }

    std::promise<void> connectPromise;
    std::future<void> connectFuture = connectPromise.get_future();

    rfcommDevice = (__bridge void*)device;
    uthread = std::thread(
        MacOSBluetoothConnector::connectToMac,
        this,
        std::move(connectPromise));

    try {
        connectFuture.get();
    } catch (...) {
        channelClosed();

        if (uthread.joinable() &&
            uthread.get_id() != std::this_thread::get_id()) {
            uthread.join();
        }

        rfcommDevice = nullptr;
        rfcommchannel = nullptr;
        throw;
    }
}

int MacOSBluetoothConnector::recv(char* buf, size_t length)
{
    std::unique_lock<std::mutex> lock(receiveDataMutex);
    const bool woke = receiveDataConditionVariable.wait_for(
        lock,
        std::chrono::milliseconds(2500),
        [this] { return !receivedBytes.empty() || !running; });

    if (!woke) {
#ifdef SHC_DEBUG_PROTOCOL
        fprintf(stderr, "[recv] timed out after 2500 ms; running=%d, queued=%zu\n",
            running.load() ? 1 : 0,
            receivedBytes.size());
#endif
        throw RecoverableException("recv timed out", false);
    }

    if (receivedBytes.empty()) {
        throw RecoverableException("connection closed", false);
    }

    std::vector<unsigned char> receivedVector = std::move(receivedBytes.front());
    receivedBytes.pop_front();

    const size_t lengthCopied = std::min(length, receivedVector.size());
    std::memcpy(buf, receivedVector.data(), lengthCopied);

    if (receivedVector.size() > lengthCopied) {
        receivedVector.erase(
            receivedVector.begin(),
            receivedVector.begin() + lengthCopied);
        receivedBytes.push_front(std::move(receivedVector));
    }

#ifdef SHC_DEBUG_PROTOCOL
    _debugHexDump("recv", buf, lengthCopied);
#endif

    return static_cast<int>(lengthCopied);
}

SonyProtocolVersion MacOSBluetoothConnector::getProtocolVersion() noexcept
{
    return protocolVersion;
}

std::vector<BluetoothDevice> MacOSBluetoothConnector::getConnectedDevices()
{
    std::vector<BluetoothDevice> result;

    for (IOBluetoothDevice* device in [IOBluetoothDevice pairedDevices]) {
        if (![device addressString]) {
            continue;
        }

        BluetoothDevice bluetoothDevice;
        bluetoothDevice.mac = [[device addressString] UTF8String];
        bluetoothDevice.name = [device name]
            ? [[device name] UTF8String]
            : "Unknown Device";
        result.push_back(std::move(bluetoothDevice));
    }

    return result;
}

void MacOSBluetoothConnector::channelClosed() noexcept
{
    // This intentionally does not set channelCloseRequested. That flag belongs
    // only to a local close request in closeConnection().
    running = false;
    receiveDataConditionVariable.notify_all();
    disconnectionConditionVariable.notify_all();
}

void MacOSBluetoothConnector::disconnect() noexcept
{
    channelClosed();
    closeConnection();

    // Joining the current thread throws std::system_error; with noexcept that
    // would become std::terminate(). The RFCOMM delegate never calls this
    // method, but retain the guard as a hard safety boundary.
    if (uthread.joinable() &&
        uthread.get_id() != std::this_thread::get_id()) {
        uthread.join();
    }

    if (!uthread.joinable()) {
        rfcommchannel = nullptr;
        rfcommDevice = nullptr;
    }
}

void MacOSBluetoothConnector::closeConnection()
{
    if (channelCloseRequested.exchange(true)) {
        return;
    }

    IOBluetoothRFCOMMChannel* channel =
        (__bridge IOBluetoothRFCOMMChannel*)rfcommchannel;

    if (channel == nil) {
        return;
    }

    // Removing the delegate before closeChannel prevents a local close from
    // re-entering C++ shutdown via rfcommChannelClosed.
    [channel setDelegate:nil];
    [channel closeChannel];
}

bool MacOSBluetoothConnector::isConnected() noexcept
{
    if (!running) {
        return false;
    }

    IOBluetoothRFCOMMChannel* channel =
        (__bridge IOBluetoothRFCOMMChannel*)rfcommchannel;
    return channel != nil && channel.isOpen;
}