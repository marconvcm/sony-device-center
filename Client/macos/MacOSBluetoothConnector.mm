#include "MacOSBluetoothConnector.h"
#include "SonyDiscovery.h"

MacOSBluetoothConnector::MacOSBluetoothConnector()
{
    
}
MacOSBluetoothConnector::~MacOSBluetoothConnector()
{
    // onclose event
    if (isConnected()){
        disconnect();
    }
}

@interface AsyncCommDelegate : NSObject <IOBluetoothRFCOMMChannelDelegate> {
@public
    MacOSBluetoothConnector* delegateCPP;
}
@end

@implementation AsyncCommDelegate {
}
- (void)rfcommChannelClosed:(IOBluetoothRFCOMMChannel *)rfcommChannel{
#ifdef SHC_DEBUG_PROTOCOL
    fprintf(stderr, "[connect] rfcommChannelClosed\n");
#endif
    delegateCPP->disconnect();
}

#ifdef SHC_DEBUG_PROTOCOL
- (void)rfcommChannelOpenComplete:(IOBluetoothRFCOMMChannel *)rfcommChannel status:(IOReturn)error {
    fprintf(stderr, "[connect] rfcommChannelOpenComplete status=0x%x\n", error);
}
#endif

-(void)rfcommChannelData:(IOBluetoothRFCOMMChannel *)rfcommChannel data:(void *)dataPointer length:(size_t)dataLength
{
    std::lock_guard<std::mutex> g(delegateCPP->receiveDataMutex);
    
    unsigned char* buffer = (unsigned char*)dataPointer;
    std::vector<unsigned char> vectorBuffer(buffer, buffer+dataLength);
    
    delegateCPP->receivedBytes.push_back(vectorBuffer);
    delegateCPP->receiveDataConditionVariable.notify_one();
}


@end

#ifdef SHC_DEBUG_PROTOCOL
static void _debugHexDump(const char* label, const char* buf, size_t length)
{
    fprintf(stderr, "[%s] ", label);
    for (size_t i = 0; i < length; i++)
    {
        fprintf(stderr, "%02x ", (unsigned char)buf[i]);
    }
    fprintf(stderr, "\n");
}
#endif

int MacOSBluetoothConnector::send(char* buf, size_t length)
{
#ifdef SHC_DEBUG_PROTOCOL
    _debugHexDump("send", buf, length);
#endif
    [(__bridge IOBluetoothRFCOMMChannel*)rfcommchannel writeSync:(void*)buf length:length];
    return (int)length;
}


void MacOSBluetoothConnector::connectToMac(MacOSBluetoothConnector* macOSBluetoothConnector, std::promise<void> connectPromise)
{
    // get device
    IOBluetoothDevice *device = (__bridge IOBluetoothDevice *)macOSBluetoothConnector->rfcommDevice;
    // create new channel
    IOBluetoothRFCOMMChannel *channel = [[IOBluetoothRFCOMMChannel alloc] init];

    // try the v1 service UUID first, then fall back to the v2 (newer-generation) UUID.
    IOBluetoothSDPUUID *sppServiceUUIDV1 = [IOBluetoothSDPUUID uuidWithBytes:(void*)SERVICE_UUID_IN_BYTES length: 16];
    IOBluetoothSDPServiceRecord *sppServiceRecord = [device getServiceRecordForUUID:sppServiceUUIDV1];
    SonyProtocolVersion protocolVersion = SonyProtocolVersion::V1;
#ifdef SHC_DEBUG_PROTOCOL
    fprintf(stderr, "[connect] v1 getServiceRecordForUUID -> %s\n", sppServiceRecord == nil ? "nil" : "found");
#endif

    if (sppServiceRecord == nil) {
        IOBluetoothSDPUUID *sppServiceUUIDV2 = [IOBluetoothSDPUUID uuidWithBytes:(void*)SERVICE_UUID_V2_IN_BYTES length: 16];
        sppServiceRecord = [device getServiceRecordForUUID:sppServiceUUIDV2];
        protocolVersion = SonyProtocolVersion::V2;
#ifdef SHC_DEBUG_PROTOCOL
        fprintf(stderr, "[connect] v2 getServiceRecordForUUID -> %s\n", sppServiceRecord == nil ? "nil" : "found");
#endif
    }

    if (sppServiceRecord == nil) {
        RecoverableException exc = RecoverableException("Couldn't find the Sony service record on this device (neither protocol version) - is this a supported headset?", false);
        std::exception_ptr excPtr = std::make_exception_ptr(exc);
        connectPromise.set_exception(excPtr);
        return;
    }

    // get rfcommChannelID from sppServiceRecord
    UInt8 rfcommChannelID = 0;
    IOReturn channelIdStatus = [sppServiceRecord getRFCOMMChannelID:&rfcommChannelID];
#ifdef SHC_DEBUG_PROTOCOL
    fprintf(stderr, "[connect] protocolVersion=%s getRFCOMMChannelID -> 0x%x, channelID=%u\n", protocolVersion == SonyProtocolVersion::V2 ? "V2" : "V1", channelIdStatus, (unsigned)rfcommChannelID);
#endif
    if (channelIdStatus != kIOReturnSuccess) {
        RecoverableException exc = RecoverableException("Found the Sony service record, but it has no RFCOMM channel.", false);
        std::exception_ptr excPtr = std::make_exception_ptr(exc);
        connectPromise.set_exception(excPtr);
        return;
    }
    // setup delegate
    AsyncCommDelegate* asyncCommDelegate = [[AsyncCommDelegate alloc] init];
    asyncCommDelegate->delegateCPP = macOSBluetoothConnector;
    // try to open channel
    IOReturn openResult = [device openRFCOMMChannelAsync:&channel withChannelID:rfcommChannelID delegate:asyncCommDelegate];
#ifdef SHC_DEBUG_PROTOCOL
    fprintf(stderr, "[connect] openRFCOMMChannelAsync -> 0x%x\n", openResult);
#endif
    if ( openResult != kIOReturnSuccess ) {
        RecoverableException exc = RecoverableException("Could not open the rfcomm.", false);
        std::exception_ptr excPtr = std::make_exception_ptr(exc);
        connectPromise.set_exception(excPtr);
        return;
    }
    // store the channel
    macOSBluetoothConnector->rfcommchannel = (__bridge void*) channel;
    macOSBluetoothConnector->protocolVersion = protocolVersion;

    macOSBluetoothConnector->running = true;

    // tell the other tread that we are done connecting
    connectPromise.set_value();

    // keep thread running, until we are disconnected
    std::unique_lock<std::mutex> lk(macOSBluetoothConnector->disconnectionMutex);
    while (macOSBluetoothConnector->running) {
        [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.1]];

        macOSBluetoothConnector->disconnectionConditionVariable.wait_for(
            lk, std::chrono::milliseconds(1000),
            [&]() { return !macOSBluetoothConnector->running; });
    }

    lk.unlock();
}
void MacOSBluetoothConnector::connect(const std::string& addrStr){
    // A failed attempt ends its thread without a join. Assigning a new thread
    // over a joinable one calls std::terminate(), so release the old one first.
    disconnect();
    // convert mac address to nsstring
    NSString *addressNSString = [NSString stringWithCString:addrStr.c_str() encoding:[NSString defaultCStringEncoding]];
    // get device based on mac address
    IOBluetoothDevice *device = [IOBluetoothDevice deviceWithAddressString:addressNSString];
    // if device is not connected
    if (![device isConnected]) {
        [device openConnection];
    }
    std::promise<void> connectPromise;
    std::future<void> connectFuture = connectPromise.get_future();

    // store the device in a variable
    rfcommDevice = (__bridge void*) device;
    uthread = std::thread(MacOSBluetoothConnector::connectToMac, this, std::move(connectPromise));
    
    // wait till the device is connected
    connectFuture.get();
}

int MacOSBluetoothConnector::recv(char* buf, size_t length)
{
    // wait for newly received data, but time out so an unanswered inquiry (probing an unsupported
    // feature) or a dropped link doesn't block the caller forever.
    std::unique_lock<std::mutex> g(receiveDataMutex);
    bool gotData = receiveDataConditionVariable.wait_for(g, std::chrono::milliseconds(2500),
        [this]{ return !receivedBytes.empty(); });
    if (!gotData) {
        throw RecoverableException("recv timed out", false);
    }

    // fill the buf with the new data
    std::vector<unsigned char> receivedVector = receivedBytes.front();
    receivedBytes.pop_front();
    
    size_t lengthCopied = std::min(length, receivedVector.size());

    // copy the first amount of bytes
    std::memcpy(buf, receivedVector.data(), lengthCopied);
    
    // too much data, save it for next time
    if (receivedVector.size() > length){
        receivedVector.erase(receivedVector.begin(), receivedVector.begin() + lengthCopied);
        receivedBytes.push_front(receivedVector);
    }

#ifdef SHC_DEBUG_PROTOCOL
    _debugHexDump("recv", buf, lengthCopied);
#endif

    return (int)lengthCopied;
}

SonyProtocolVersion MacOSBluetoothConnector::getProtocolVersion() noexcept
{
    return protocolVersion;
}

std::vector<BluetoothDevice> MacOSBluetoothConnector::getConnectedDevices()
{
    // create the output vector
    std::vector<BluetoothDevice> res;
    // Keep paired Sony candidates, not just ones macOS reports as connected: [isConnected] returns NO for
    // headsets connected only for audio/BLE (e.g. Sony ULT WEAR), which hid them from the picker. connect()
    // opens the RFCOMM link on demand, so a paired-but-"disconnected" device still works.
    IOBluetoothSDPUUID *v1 = [IOBluetoothSDPUUID uuidWithBytes:(void*)SERVICE_UUID_IN_BYTES length:16];
    IOBluetoothSDPUUID *v2 = [IOBluetoothSDPUUID uuidWithBytes:(void*)SERVICE_UUID_V2_IN_BYTES length:16];
    for (IOBluetoothDevice* device in [IOBluetoothDevice pairedDevices]) {
        if (![device addressString]) continue;
        BluetoothDevice dev;
        dev.mac = [[device addressString] UTF8String];
        dev.name = [device name] ? [[device name] UTF8String] : "Unknown Device";
        // These lookups use cached service records; discovery must not initiate
        // connections or SDP queries against every paired peripheral.
        bool hasSonyService = [device getServiceRecordForUUID:v1] != nil
            || [device getServiceRecordForUUID:v2] != nil;
        if (!isSonyHeadsetCandidate(dev.name, hasSonyService)) continue;
        dev.paired = true;
        dev.connected = [device isConnected] == YES;
        res.push_back(dev);
    }
    
    return res;
}

void MacOSBluetoothConnector::disconnect() noexcept
{
    // close connection
    closeConnection();
    running = false;
    // notify the other thread that we are done disconnecting
    disconnectionConditionVariable.notify_all();
    // wait for the thread to finish. The channel-closed callback calls this
    // function on that thread, and a thread cannot join itself.
    if (uthread.joinable() && uthread.get_id() != std::this_thread::get_id()) {
        uthread.join();
    }
}
void MacOSBluetoothConnector::closeConnection() {
    // get the channel
    IOBluetoothRFCOMMChannel *chan = (__bridge IOBluetoothRFCOMMChannel*) rfcommchannel;
    [chan setDelegate: nil];
    // close the channel
    [chan closeChannel];
}


bool MacOSBluetoothConnector::isConnected() noexcept
{
    if (!running)
        return false;
    IOBluetoothRFCOMMChannel *chan = (__bridge IOBluetoothRFCOMMChannel*) rfcommchannel;
    return chan.isOpen;
}
