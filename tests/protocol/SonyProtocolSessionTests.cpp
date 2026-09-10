#include <catch2/catch_test_macros.hpp>
#include "sony/protocol/SonyProtocolSession.h"
#include "sony/protocol/FrameCodec.h"
#include "sony/transport/FakeTransport.h"
#include "sony/transport/SonyError.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

using namespace sony;
using namespace sony::protocol;
using namespace sony::transport;

namespace {

SonyFrame makeAckFrame(uint8_t seq) {
    return SonyFrame{
        .type = DataType::Ack,
        .sequence = seq,
        .payload = {}
    };
}

SonyFrame makeDataFrame(uint8_t seq, std::vector<uint8_t> payload) {
    return SonyFrame{
        .type = DataType::DataMdr,
        .sequence = seq,
        .payload = std::move(payload)
    };
}

void queueFrame(FakeTransport& transport, const SonyFrame& frame) {
    transport.queueIncoming(FrameCodec::encode(frame));
}

} // namespace

TEST_CASE("SonyProtocolSession: ACK before response completes successfully", "[protocol][session]")
{
    FakeTransport fake;
    SonyProtocolSession session(&fake);
    session.connect("11:22:33:44:55:66");
    REQUIRE(session.isConnected());

    SonyFrame request = makeDataFrame(0, {0x02, 0x01});
    SonyFrame ack = makeAckFrame(0);
    SonyFrame response = makeDataFrame(1, {0x02, 0x02, 0x50});

    queueFrame(fake, ack);
    queueFrame(fake, response);

    SonyFrame result = session.sendAndAwaitResponse(request, 0x02, 0x02, std::chrono::milliseconds(1000));
    REQUIRE(result.type == DataType::DataMdr);
    REQUIRE(result.payload == std::vector<uint8_t>{0x02, 0x02, 0x50});

    // Device sent 1 DATA_MDR response, host must have sent 1 request and 1 auto-ACK
    REQUIRE(fake.sentCount() == 2);
    auto sentAck = FrameCodec::decode(fake.lastSentFrame());
    REQUIRE(sentAck.type == DataType::Ack);
    REQUIRE(sentAck.sequence == 0); // 1 - 1 = 0
}

TEST_CASE("SonyProtocolSession: response before unrelated notification", "[protocol][session]")
{
    FakeTransport fake;
    SonyProtocolSession session(&fake);
    session.connect("11:22:33:44:55:66");

    std::vector<SonyFrame> notifications;
    std::mutex notifMtx;
    std::condition_variable notifCv;

    session.onNotification([&](const SonyFrame& frame) {
        std::lock_guard lock(notifMtx);
        notifications.push_back(frame);
        notifCv.notify_all();
    });

    SonyFrame request = makeDataFrame(0, {0x02, 0x01});
    SonyFrame ack = makeAckFrame(0);
    SonyFrame response = makeDataFrame(1, {0x02, 0x02, 0x50});
    SonyFrame unrelatedNotif = makeDataFrame(0, {0x04, 0x01, 0x10});

    queueFrame(fake, ack);
    queueFrame(fake, response);
    queueFrame(fake, unrelatedNotif);

    SonyFrame result = session.sendAndAwaitResponse(request, 0x02, 0x02, std::chrono::milliseconds(1000));
    REQUIRE(result.payload == std::vector<uint8_t>{0x02, 0x02, 0x50});

    std::unique_lock lock(notifMtx);
    bool received = notifCv.wait_for(lock, std::chrono::milliseconds(1000), [&] {
        return !notifications.empty();
    });
    REQUIRE(received);
    REQUIRE(notifications.size() == 1);
    REQUIRE(notifications[0].payload == std::vector<uint8_t>{0x04, 0x01, 0x10});
}

TEST_CASE("SonyProtocolSession: notification between ACK and response", "[protocol][session]")
{
    FakeTransport fake;
    SonyProtocolSession session(&fake);
    session.connect("11:22:33:44:55:66");

    std::vector<SonyFrame> notifications;
    std::mutex notifMtx;
    std::condition_variable notifCv;

    session.onNotification([&](const SonyFrame& frame) {
        std::lock_guard lock(notifMtx);
        notifications.push_back(frame);
        notifCv.notify_all();
    });

    SonyFrame request = makeDataFrame(0, {0x02, 0x01});
    SonyFrame ack = makeAckFrame(0);
    SonyFrame notif = makeDataFrame(1, {0x09, 0x01, 0x99});
    SonyFrame response = makeDataFrame(0, {0x02, 0x02, 0x88});

    queueFrame(fake, ack);
    queueFrame(fake, notif);
    queueFrame(fake, response);

    SonyFrame result = session.sendAndAwaitResponse(request, 0x02, 0x02, std::chrono::milliseconds(1000));
    REQUIRE(result.payload == std::vector<uint8_t>{0x02, 0x02, 0x88});

    std::unique_lock lock(notifMtx);
    bool received = notifCv.wait_for(lock, std::chrono::milliseconds(1000), [&] {
        return !notifications.empty();
    });
    REQUIRE(received);
    REQUIRE(notifications.size() == 1);
    REQUIRE(notifications[0].payload == std::vector<uint8_t>{0x09, 0x01, 0x99});
}

TEST_CASE("SonyProtocolSession: dispatches multiple notifications in order", "[protocol][session]")
{
    FakeTransport fake;
    SonyProtocolSession session(&fake);
    session.connect("11:22:33:44:55:66");

    std::vector<SonyFrame> notifications;
    std::mutex notifMtx;
    std::condition_variable notifCv;

    session.onNotification([&](const SonyFrame& frame) {
        std::lock_guard lock(notifMtx);
        notifications.push_back(frame);
        if (notifications.size() >= 3) {
            notifCv.notify_all();
        }
    });

    SonyFrame n1 = makeDataFrame(0, {0x01, 0xAA});
    SonyFrame n2 = makeDataFrame(1, {0x02, 0xBB});
    SonyFrame n3 = makeDataFrame(0, {0x03, 0xCC});

    queueFrame(fake, n1);
    queueFrame(fake, n2);
    queueFrame(fake, n3);

    std::unique_lock lock(notifMtx);
    bool received = notifCv.wait_for(lock, std::chrono::milliseconds(1000), [&] {
        return notifications.size() >= 3;
    });
    REQUIRE(received);
    REQUIRE(notifications.size() == 3);
    REQUIRE(notifications[0].payload == std::vector<uint8_t>{0x01, 0xAA});
    REQUIRE(notifications[1].payload == std::vector<uint8_t>{0x02, 0xBB});
    REQUIRE(notifications[2].payload == std::vector<uint8_t>{0x03, 0xCC});

    REQUIRE(fake.sentCount() == 3);
}

TEST_CASE("SonyProtocolSession: timeouts on missing ACK or missing response", "[protocol][session]")
{
    FakeTransport fake;
    SonyProtocolSession session(&fake);
    session.connect("11:22:33:44:55:66");

    SECTION("send() times out when device fails to ACK")
    {
        SonyFrame frame = makeDataFrame(0, {0x01, 0x02});
        try {
            session.send(frame, std::chrono::milliseconds(50));
            FAIL("Expected Timeout exception");
        } catch (const SonyException& ex) {
            REQUIRE(ex.code() == SonyErrorCode::Timeout);
        }
    }

    SECTION("sendAndAwaitResponse() times out when response is never sent")
    {
        SonyFrame request = makeDataFrame(0, {0x02, 0x01});
        SonyFrame ack = makeAckFrame(0);
        queueFrame(fake, ack);

        try {
            session.sendAndAwaitResponse(request, 0x02, -1, std::chrono::milliseconds(50));
            FAIL("Expected Timeout exception");
        } catch (const SonyException& ex) {
            REQUIRE(ex.code() == SonyErrorCode::Timeout);
        }
    }
}

TEST_CASE("SonyProtocolSession: disconnect during pending request throws Disconnected", "[protocol][session]")
{
    FakeTransport fake;
    SonyProtocolSession session(&fake);
    session.connect("11:22:33:44:55:66");

    SonyFrame request = makeDataFrame(0, {0x02, 0x01});
    std::optional<SonyErrorCode> caughtCode;

    std::thread worker([&] {
        try {
            session.sendAndAwaitResponse(request, 0x02, -1, std::chrono::milliseconds(5000));
        } catch (const SonyException& ex) {
            caughtCode = ex.code();
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    fake.simulateDisconnect();
    worker.join();

    REQUIRE(caughtCode.has_value());
    REQUIRE(*caughtCode == SonyErrorCode::Disconnected);
}

TEST_CASE("SonyProtocolSession: ignores duplicate frame without duplicate dispatch but ACKs it", "[protocol][session]")
{
    FakeTransport fake;
    SonyProtocolSession session(&fake);
    session.connect("11:22:33:44:55:66");

    std::atomic<int> notifCount{0};
    session.onNotification([&](const SonyFrame&) {
        ++notifCount;
    });

    SonyFrame n1 = makeDataFrame(0, {0x01, 0xAA});
    // Queue identical frame twice
    queueFrame(fake, n1);
    queueFrame(fake, n1);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    REQUIRE(notifCount.load() == 1);
    REQUIRE(fake.sentCount() == 2);

    SonyFrame n2 = makeDataFrame(1, {0x01, 0xBB});
    queueFrame(fake, n2);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    REQUIRE(notifCount.load() == 2);
    REQUIRE(fake.sentCount() == 3);
}

TEST_CASE("SonyProtocolSession: rejects invalid frame without crashing and recovers", "[protocol][session]")
{
    FakeTransport fake;
    SonyProtocolSession session(&fake);
    session.connect("11:22:33:44:55:66");

    std::vector<SonyFrame> received;
    std::mutex mtx;
    std::condition_variable cv;

    session.onNotification([&](const SonyFrame& frame) {
        std::lock_guard lock(mtx);
        received.push_back(frame);
        cv.notify_all();
    });

    // Corrupted checksum
    SonyFrame validFrame = makeDataFrame(0, {0x50, 0x01});
    auto badBytes = FrameCodec::encode(validFrame);
    badBytes[badBytes.size() - 2] ^= 0xff;

    // Garbage bytes
    std::vector<uint8_t> garbage = {0x12, 0x34, 0x56};

    // Valid notification
    SonyFrame goodFrame = makeDataFrame(1, {0x50, 0x02});
    auto goodBytes = FrameCodec::encode(goodFrame);

    fake.queueIncoming(badBytes);
    fake.queueIncoming(garbage);
    fake.queueIncoming(goodBytes);

    std::unique_lock lock(mtx);
    bool ok = cv.wait_for(lock, std::chrono::milliseconds(1000), [&] {
        return !received.empty();
    });

    REQUIRE(ok);
    REQUIRE(received.size() == 1);
    REQUIRE(received[0].payload == std::vector<uint8_t>{0x50, 0x02});
}

TEST_CASE("SonyProtocolSession: manages sequence numbers and handles rollover at 255", "[protocol][session]")
{
    FakeTransport fake;
    SonyProtocolSession session(&fake);
    session.connect("11:22:33:44:55:66");

    REQUIRE(session.currentSequenceNumber() == 0);
    session.setSequenceNumber(254);
    REQUIRE(session.currentSequenceNumber() == 254);

    REQUIRE(session.nextSequenceNumber() == 254);
    REQUIRE(session.currentSequenceNumber() == 255);

    REQUIRE(session.nextSequenceNumber() == 255);
    REQUIRE(session.currentSequenceNumber() == 0);

    REQUIRE(session.nextSequenceNumber() == 0);
    REQUIRE(session.currentSequenceNumber() == 1);

    // Verify wire sequence rollover across send calls
    session.setSequenceNumber(255);

    queueFrame(fake, makeAckFrame(255));
    session.send(makeDataFrame(0, {0x01}));

    queueFrame(fake, makeAckFrame(0));
    session.send(makeDataFrame(0, {0x02}));

    REQUIRE(fake.sentCount() == 2);
    auto sent1 = FrameCodec::decode(fake.sentFrames()[0]);
    REQUIRE(sent1.sequence == 255);

    auto sent2 = FrameCodec::decode(fake.sentFrames()[1]);
    REQUIRE(sent2.sequence == 0);
}

TEST_CASE("SonyProtocolSession: supports transport ownership models", "[protocol][session]")
{
    SECTION("Owns transport via unique_ptr")
    {
        auto fakePtr = std::make_unique<FakeTransport>();
        auto* raw = fakePtr.get();
        SonyProtocolSession session(std::move(fakePtr));
        REQUIRE(session.transport() == raw);
        session.connect("11:22:33:44:55:66");
        REQUIRE(session.isConnected());
        session.disconnect();
        REQUIRE_FALSE(session.isConnected());
    }

    SECTION("Non-owning reference via pointer")
    {
        FakeTransport fake;
        SonyProtocolSession session(&fake);
        REQUIRE(session.transport() == &fake);
        session.connect("11:22:33:44:55:66");
        REQUIRE(session.isConnected());
        session.disconnect();
        REQUIRE_FALSE(session.isConnected());
    }
}
