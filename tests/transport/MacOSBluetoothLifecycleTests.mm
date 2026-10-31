#include <catch2/catch_test_macros.hpp>
#include "macos/MacOSBluetoothConnector.h"
#import <Foundation/Foundation.h>
#import <IOBluetooth/IOBluetooth.h>
#include <atomic>
#include <future>
#include <chrono>
using namespace std::chrono_literals;

struct MacOSBluetoothConnectorTestAccess {
    static void configure(MacOSBluetoothConnector& connector, void* device, std::chrono::milliseconds timeout = 500ms) {
        connector._deviceFactory = [device](const std::string&) { return device; };
        connector._connectTimeout = timeout;
        connector._writeTimeout = 70ms;
    }
};
struct NativeFixtureState {
    std::atomic<int> closes{0}, writes{0};
    std::atomic<void*> callback{nullptr};
    IOReturn startStatus{kIOReturnSuccess}, openStatus{kIOReturnSuccess};
    IOReturn writeStart{kIOReturnSuccess}, writeStatus{kIOReturnSuccess};
    bool completeOpen{true}, completeWrite{true};
    unsigned mtu{3};
};
@interface FixtureChannel : NSObject {
@public
    std::shared_ptr<NativeFixtureState> fixture;
    id delegate;
}
- (void)setDelegate:(id)value;
- (IOReturn)closeChannel;
- (UInt16)getMTU;
- (void)opened;
- (IOReturn)writeAsync:(void*)data length:(UInt16)length refcon:(void*)refcon;
@end
@implementation FixtureChannel
- (void)setDelegate:(id)value { delegate = value; }
- (IOReturn)closeChannel { ++fixture->closes; return kIOReturnSuccess; }
- (UInt16)getMTU { return fixture->mtu; }
- (void)opened {
    if (delegate) [delegate rfcommChannelOpenComplete:(IOBluetoothRFCOMMChannel*)self status:fixture->openStatus];
}
- (IOReturn)writeAsync:(void*)data length:(UInt16)length refcon:(void*)refcon {
    ++fixture->writes;
    if (fixture->writeStart != kIOReturnSuccess) return fixture->writeStart;
    if (fixture->completeWrite) [delegate rfcommChannelWriteComplete:(IOBluetoothRFCOMMChannel*)self refcon:refcon status:fixture->writeStatus];
    return kIOReturnSuccess;
}
@end
@interface FixtureRecord : NSObject
- (IOReturn)getRFCOMMChannelID:(UInt8*)channel;
@end
@implementation FixtureRecord
- (IOReturn)getRFCOMMChannelID:(UInt8*)channel { *channel = 7; return kIOReturnSuccess; }
@end
@interface FixtureDevice : NSObject {
@public
    FixtureChannel* channel;
    FixtureRecord* record;
}
- (IOBluetoothSDPServiceRecord*)getServiceRecordForUUID:(id)uuid;
- (IOReturn)openRFCOMMChannelAsync:(IOBluetoothRFCOMMChannel**)result withChannelID:(UInt8)channelId delegate:(id)callback;
@end
@implementation FixtureDevice
- (IOBluetoothSDPServiceRecord*)getServiceRecordForUUID:(id)uuid { return (IOBluetoothSDPServiceRecord*)record; }
- (IOReturn)openRFCOMMChannelAsync:(IOBluetoothRFCOMMChannel**)result withChannelID:(UInt8)channelId delegate:(id)callback {
    *result = (IOBluetoothRFCOMMChannel*)channel;
    [channel setDelegate:callback];
    channel->fixture->callback = (__bridge void*)callback;
    if (channel->fixture->completeOpen)
        [channel performSelector:@selector(opened) withObject:nil afterDelay:0.04];
    return channel->fixture->startStatus;
}
@end
namespace {
struct Fixture {
    std::shared_ptr<NativeFixtureState> state{std::make_shared<NativeFixtureState>()};
    FixtureDevice* device{[[FixtureDevice alloc] init]};
    FixtureChannel* channel{[[FixtureChannel alloc] init]};
    FixtureRecord* record{[[FixtureRecord alloc] init]};
    Fixture() { device->channel = channel; device->record = record; channel->fixture = state; }
    ~Fixture() { [device release]; [channel release]; [record release]; }
    void configure(MacOSBluetoothConnector& connector, std::chrono::milliseconds timeout = 500ms) {
        MacOSBluetoothConnectorTestAccess::configure(connector, (__bridge void*)device, timeout);
    }
    id callback() { return (__bridge id)state->callback.load(); }
};
}

TEST_CASE("macOS waits for native open completion and releases failed workers", "[transport][macos][lifecycle]") {
    Fixture fixture;
    MacOSBluetoothConnector connector;
    fixture.configure(connector);
    SECTION("Delayed success is not reported early") {
        auto connection = std::async(std::launch::async, [&] { connector.connect("11:22:33:44:55:66"); });
        CHECK(connection.wait_for(10ms) == std::future_status::timeout);
        REQUIRE_NOTHROW(connection.get());
        CHECK(connector.isConnected());
    }
    SECTION("Immediate start error") {
        fixture.state->startStatus = kIOReturnError;
        CHECK_THROWS_AS(connector.connect("11:22:33:44:55:66"), RecoverableException);
    }
    SECTION("Delayed open error") {
        fixture.state->openStatus = kIOReturnError;
        CHECK_THROWS_AS(connector.connect("11:22:33:44:55:66"), RecoverableException);
    }
    SECTION("Missing callback has a deadline") {
        fixture.configure(connector, 100ms);
        fixture.state->completeOpen = false;
        const auto start = std::chrono::steady_clock::now();
        CHECK_THROWS_AS(connector.connect("11:22:33:44:55:66"), RecoverableException);
        CHECK(std::chrono::steady_clock::now() - start < 1s);
    }
    connector.disconnect();
    connector.disconnect();
    CHECK_FALSE(connector.isConnected());
    CHECK(fixture.state->closes == 1);
}

TEST_CASE("macOS queued callbacks outlive connector destruction safely", "[transport][macos][lifecycle]") {
    Fixture fixture;
    id callback;
    {
        MacOSBluetoothConnector connector;
        fixture.configure(connector);
        connector.connect("11:22:33:44:55:66");
        callback = [fixture.callback() retain];
    }
    [callback rfcommChannelClosed:(IOBluetoothRFCOMMChannel*)fixture.channel];
    [callback rfcommChannelOpenComplete:(IOBluetoothRFCOMMChannel*)fixture.channel status:kIOReturnSuccess];
    [callback release];
    CHECK(fixture.state->closes == 1);
}

TEST_CASE("macOS remote close wakes receive and old callbacks cannot revive a session", "[transport][macos][lifecycle]") {
    Fixture oldFixture, newFixture;
    MacOSBluetoothConnector connector;
    oldFixture.configure(connector);
    connector.connect("11:22:33:44:55:66");
    id oldCallback = [oldFixture.callback() retain];
    unsigned char payload[]{1,2,3};
    [oldCallback rfcommChannelData:(IOBluetoothRFCOMMChannel*)oldFixture.channel data:payload length:3];
    char received[3]{};
    CHECK(connector.recv(received, 2) == 2);
    CHECK(connector.recv(received + 2, 1) == 1);
    CHECK(received[0] == 1); CHECK(received[2] == 3);
    auto waiting = std::async(std::launch::async, [&] { try { connector.recv(received, 1); return false; } catch (const RecoverableException&) { return true; } });
    [oldCallback rfcommChannelClosed:(IOBluetoothRFCOMMChannel*)oldFixture.channel];
    CHECK(waiting.wait_for(200ms) == std::future_status::ready);
    CHECK(waiting.get());
    connector.disconnect();
    newFixture.configure(connector);
    connector.connect("22:22:33:44:55:66");
    [oldCallback rfcommChannelOpenComplete:(IOBluetoothRFCOMMChannel*)oldFixture.channel status:kIOReturnSuccess];
    [oldCallback rfcommChannelClosed:(IOBluetoothRFCOMMChannel*)oldFixture.channel];
    CHECK(connector.isConnected());
    [oldCallback release];
    connector.disconnect();
    CHECK(oldFixture.state->closes == 1);
    CHECK(newFixture.state->closes == 1);
}

TEST_CASE("macOS async writes respect MTU, report errors and time out", "[transport][macos][lifecycle]") {
    Fixture fixture;
    MacOSBluetoothConnector connector;
    fixture.configure(connector);
    connector.connect("11:22:33:44:55:66");
    char payload[]{1,2,3,4,5};
    SECTION("MTU bounds each write") { CHECK(connector.send(payload, 5) == 3); }
    SECTION("Immediate native error") {
        fixture.state->writeStart = kIOReturnError;
        CHECK_THROWS_AS(connector.send(payload, 5), RecoverableException);
    }
    SECTION("Completion error") {
        fixture.state->writeStatus = kIOReturnError;
        CHECK_THROWS_AS(connector.send(payload, 5), RecoverableException);
    }
    SECTION("Missing write completion") {
        fixture.state->completeWrite = false;
        const auto start = std::chrono::steady_clock::now();
        CHECK_THROWS_AS(connector.send(payload, 5), RecoverableException);
        CHECK(std::chrono::steady_clock::now() - start < 1s);
    }
    CHECK(fixture.state->writes == 1);
}

TEST_CASE("macOS cancellation interrupts a pending native open", "[transport][macos][lifecycle]") {
    Fixture fixture;
    fixture.state->completeOpen = false;
    MacOSBluetoothConnector connector;
    fixture.configure(connector, 5s);
    auto connection = std::async(std::launch::async, [&] { try { connector.connect("11:22:33:44:55:66"); return false; } catch (const RecoverableException&) { return true; } });
    const auto deadline = std::chrono::steady_clock::now() + 1s;
    while (!fixture.state->callback && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(1ms);
    connector.cancelPending();
    CHECK(connection.wait_for(200ms) == std::future_status::ready);
    CHECK(connection.get());
    connector.disconnect();
    CHECK(fixture.state->closes == 1);
}
