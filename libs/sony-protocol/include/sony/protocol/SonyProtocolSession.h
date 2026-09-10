#pragma once

#include "DataType.h"
#include "FrameCodec.h"
#include "SonyFrame.h"
#include "sony/transport/ITransport.h"
#include "sony/transport/SonyError.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <thread>
#include <vector>

namespace sony::protocol {

class SonyProtocolSession {
public:
    using NotificationCallback = std::function<void(const SonyFrame&)>;

    explicit SonyProtocolSession(std::unique_ptr<transport::ITransport> transport);
    explicit SonyProtocolSession(transport::ITransport* transport);
    ~SonyProtocolSession();

    SonyProtocolSession(const SonyProtocolSession&) = delete;
    SonyProtocolSession& operator=(const SonyProtocolSession&) = delete;

    void connect(const transport::DeviceAddress& address);
    void start();
    void disconnect() noexcept;
    [[nodiscard]] bool isConnected() const noexcept;

    // Send a frame and wait for device ACK
    void send(const SonyFrame& frame, std::chrono::milliseconds timeout = std::chrono::milliseconds(2000));

    // Send a request and await matching response frame (skipping unrelated notifications and ACK)
    SonyFrame sendAndAwaitResponse(
        const SonyFrame& request,
        uint8_t retOpcode,
        int retSubtype = -1,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(2000));

    // Register a callback for unsolicited notifications
    void onNotification(NotificationCallback callback);

    // Sequence management
    [[nodiscard]] uint8_t nextSequenceNumber() noexcept;
    [[nodiscard]] uint8_t currentSequenceNumber() const noexcept;
    void setSequenceNumber(uint8_t seq) noexcept;

    [[nodiscard]] transport::ITransport* transport() const noexcept;

private:
    void _startReader();
    void _stopReader() noexcept;
    void _readerLoop();
    void _handleIncomingBytes(std::span<const std::byte> bytes);
    void _parseStream(std::vector<SonyFrame>& outFrames);
    void _handleDecodedFrame(const SonyFrame& frame);
    void _sendAck(uint8_t seqNumber);
    void _writeFrame(const SonyFrame& frame);

    std::unique_ptr<transport::ITransport> _ownedTransport;
    transport::ITransport* _transport{nullptr};

    std::atomic<bool> _running{false};
    std::thread _readerThread;

    mutable std::mutex _sendMtx;
    mutable std::mutex _writeMtx;
    mutable std::mutex _sessionMtx;

    std::atomic<uint8_t> _sequence{0};

    // Buffer for assembling stream into frames
    std::vector<uint8_t> _streamBuffer;

    // Request/Response matching state
    struct PendingRequest {
        uint8_t expectedOpcode{0};
        int expectedSubtype{-1};
        bool hasResponse{false};
        SonyFrame response;
        bool hasAck{false};
        uint8_t expectedAckSeq{0};
    };

    std::optional<PendingRequest> _pendingRequest;
    std::deque<SonyFrame> _unmatchedFrames;
    bool _hasAck{false};
    uint8_t _expectedAckSeq{0};

    std::condition_variable _responseCv;
    std::condition_variable _ackCv;

    std::vector<NotificationCallback> _notificationCallbacks;

    // Last seen incoming sequence for duplicate detection
    std::optional<uint8_t> _lastSeenDataSeq;
};

} // namespace sony::protocol
