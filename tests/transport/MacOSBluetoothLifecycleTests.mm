#include <catch2/catch_test_macros.hpp>
#include "macos/MacOSBluetoothConnector.h"

struct MacOSBluetoothConnectorTestAccess {
    static void attach(MacOSBluetoothConnector& connector, void* channel) {
        connector.retainChannel(channel);
        connector.running = true;
    }
    static void finishedWorker(MacOSBluetoothConnector& connector) {
        connector.uthread = std::thread([] {});
    }
};

@interface LifecycleChannel : NSObject {
@public
    int* closes;
    int* destructions;
}
- (void)setDelegate:(id)delegate;
- (IOReturn)closeChannel;
- (BOOL)isOpen;
@end
@implementation LifecycleChannel
- (void)setDelegate:(id)delegate {}
- (IOReturn)closeChannel { ++*closes; return kIOReturnSuccess; }
- (BOOL)isOpen { return YES; }
- (void)dealloc { ++*destructions; [super dealloc]; }
@end

TEST_CASE("macOS channel survives caller release and disconnect is repeatable", "[transport][macos][lifecycle]") {
    int closes = 0, destructions = 0;
    {
        MacOSBluetoothConnector connector;
        LifecycleChannel* channel = [[LifecycleChannel alloc] init];
        channel->closes = &closes;
        channel->destructions = &destructions;
        MacOSBluetoothConnectorTestAccess::attach(connector, (__bridge void*)channel);
        [channel release];
        CHECK(destructions == 0);
        CHECK(connector.isConnected());
        connector.receivedBytes.push_back({1, 2, 3});
        connector.disconnect();
        CHECK(closes == 1);
        CHECK(destructions == 1);
        CHECK_FALSE(connector.isConnected());
        CHECK(connector.receivedBytes.empty());
        connector.disconnect();
        CHECK(closes == 1);
        CHECK(destructions == 1);
    }
    CHECK(destructions == 1);
}

TEST_CASE("macOS destructor joins a failed or remotely closed worker", "[transport][macos][lifecycle]") {
    MacOSBluetoothConnector connector;
    MacOSBluetoothConnectorTestAccess::finishedWorker(connector);
    CHECK_FALSE(connector.isConnected());
}
