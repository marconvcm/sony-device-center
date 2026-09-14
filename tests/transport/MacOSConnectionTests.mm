#include <catch2/catch_test_macros.hpp>
#include "macos/MacOSBluetoothConnector.h"
#import <objc/runtime.h>

// Substitute only the OS boundary: exercise the real connector without Bluetooth
// hardware or privacy prompts. Async acceptance must not mean an open channel.
@interface TestRFCOMMChannel : NSObject
@property BOOL open;
@end
@implementation TestRFCOMMChannel
- (BOOL)isOpen { return self.open; }
- (void)setDelegate:(id)delegate {}
- (IOReturn)closeChannel { self.open = NO; return kIOReturnSuccess; }
@end

@interface TestSonyDevice : NSObject
@property BOOL failOpen;
@property(strong) TestRFCOMMChannel *channel;
@end
@implementation TestSonyDevice
- (BOOL)isConnected { return YES; }
- (id)getServiceRecordForUUID:(id)uuid { return self; }
- (IOReturn)getRFCOMMChannelID:(BluetoothRFCOMMChannelID *)channelID {
    *channelID = 9;
    return kIOReturnSuccess;
}
- (IOReturn)openRFCOMMChannelAsync:(IOBluetoothRFCOMMChannel **)channel
                   withChannelID:(BluetoothRFCOMMChannelID)channelID delegate:(id)delegate {
    self.channel = [TestRFCOMMChannel new];
    *channel = (IOBluetoothRFCOMMChannel *)self.channel;
    self.channel.open = !self.failOpen;
    [delegate rfcommChannelOpenComplete:(IOBluetoothRFCOMMChannel *)self.channel
                                 status:self.failOpen ? kIOReturnError : kIOReturnSuccess];
    return kIOReturnSuccess; // Accepted, but establishment has not completed.
}
- (IOReturn)openRFCOMMChannelSync:(IOBluetoothRFCOMMChannel **)channel
                  withChannelID:(BluetoothRFCOMMChannelID)channelID delegate:(id)delegate {
    return [self openRFCOMMChannelAsync:channel withChannelID:channelID delegate:delegate];
}
@end

static TestSonyDevice *testDevice;
static id deviceForTest(id, SEL, NSString *) { return testDevice; }

TEST_CASE("macOS connect waits for channel establishment and propagates failure", "[transport][macos]") {
    @autoreleasepool {
        Method method = class_getClassMethod([IOBluetoothDevice class], @selector(deviceWithAddressString:));
        IMP original = method_setImplementation(method, (IMP)deviceForTest);
        struct Restore {
            Method method; IMP original;
            ~Restore() { method_setImplementation(method, original); }
        } restore{method, original};
        testDevice = [TestSonyDevice new];
        MacOSBluetoothConnector connector;
        SECTION("success is returned only for an open channel") {
            connector.connect("80:99:E7:00:00:01");
            CHECK(connector.isConnected());
        }
        SECTION("establishment failure reaches the caller and permits retry") {
            testDevice.failOpen = YES;
            CHECK_THROWS_AS(connector.connect("80:99:E7:00:00:01"), RecoverableException);
            CHECK_FALSE(connector.isConnected());
            testDevice.failOpen = NO;
            connector.connect("80:99:E7:00:00:01");
            CHECK(connector.isConnected());
        }
        connector.disconnect();
    }
}
