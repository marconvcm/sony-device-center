#include "BluetoothWrapper.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>

#include <algorithm>
#include <deque>
#include <utility>

namespace
{
    // Local receive script for the existing interface, not the Phase 3/4 transport abstraction.
    class ScriptedConnector final : public IBluetoothConnector
    {
    public:
        explicit ScriptedConnector(std::deque<Buffer> reads) : reads(std::move(reads)) {}

        int send(char* buffer, size_t length) override
        {
            writes.emplace_back(buffer, buffer + length);
            return static_cast<int>(length);
        }

        int recv(char* buffer, size_t length) override
        {
            ++receiveCalls;
            if (reads.empty())
                throw RecoverableException("Receive script exhausted", false);
            auto& next = reads.front();
            const auto count = std::min(length, next.size());
            std::copy_n(next.begin(), count, buffer);
            next.erase(next.begin(), next.begin() + count);
            if (next.empty())
                reads.pop_front();
            return static_cast<int>(count);
        }

        void connect(const std::string&) override {}
        void disconnect() noexcept override {}
        bool isConnected() noexcept override { return true; }
        std::vector<BluetoothDevice> getConnectedDevices() override { return {}; }
        SonyProtocolVersion getProtocolVersion() noexcept override { return SonyProtocolVersion::V1; }

        std::deque<Buffer> reads;
        std::vector<Buffer> writes;
        size_t receiveCalls = 0;
    };

    const Buffer response = {0x3e, 0x0c, 0, 0, 0, 0, 2, 0x67, 2, 0x77, 0x3c};
    const Buffer nextResponse = {0x3e, 0x0c, 1, 0, 0, 0, 2, 0x67, 2, 0x78, 0x3c};
    const Buffer hostAck = {0x3e, 1, 1, 0, 0, 0, 0, 2, 0x3c};

    Buffer readAmbient(BluetoothWrapper& wrapper)
    {
        return wrapper.sendCommandAndReadResponse({0x66, 2}, 0x67, 2);
    }
}

TEST_CASE("Wrapper parses a complete frame and ACKs its data", "[stream][framing]")
{
    auto connector = std::make_unique<ScriptedConnector>(std::deque<Buffer>{response});
    auto* script = connector.get();
    BluetoothWrapper wrapper(std::move(connector));
    REQUIRE(readAmbient(wrapper) == Buffer{0x67, 2});
    REQUIRE(script->receiveCalls == 1);
    REQUIRE(script->writes.size() == 2);
    REQUIRE(script->writes[0] == Buffer{0x3e, 0x0c, 0, 0, 0, 0, 2, 0x66, 2, 0x76, 0x3c});
    REQUIRE(script->writes[1] == hostAck);
}

TEST_CASE("Wrapper assembles every two-part fragmentation including escape pairs", "[stream][fragmentation]")
{
    const Buffer escaped = {0x3e, 0x0c, 0, 0, 0, 0, 5, 0x67, 2, 0x3d, 0x2c, 0x3d, 0x2d, 0x3d, 0x2e, 0x31, 0x3c};
    for (size_t split = 1; split < escaped.size(); ++split)
    {
        DYNAMIC_SECTION("Split at byte " << split)
        {
            auto connector = std::make_unique<ScriptedConnector>(std::deque<Buffer>{
                Buffer(escaped.begin(), escaped.begin() + split),
                Buffer(escaped.begin() + split, escaped.end())});
            BluetoothWrapper wrapper(std::move(connector));
            REQUIRE(readAmbient(wrapper) == Buffer{0x67, 2, 0x3c, 0x3d, 0x3e});
        }
    }
}

TEST_CASE("Wrapper assembles a frame delivered one byte per receive", "[stream][fragmentation]")
{
    std::deque<Buffer> reads;
    for (auto byte : response)
        reads.push_back(Buffer{byte});
    BluetoothWrapper wrapper(std::make_unique<ScriptedConnector>(std::move(reads)));
    REQUIRE(readAmbient(wrapper) == Buffer{0x67, 2});
}

TEST_CASE("Wrapper preserves two frames in one receive buffer", "[stream][coalescing]")
{
    Buffer both = response;
    both.insert(both.end(), nextResponse.begin(), nextResponse.end());
    auto connector = std::make_unique<ScriptedConnector>(std::deque<Buffer>{both});
    auto* script = connector.get();
    BluetoothWrapper wrapper(std::move(connector));
    REQUIRE(readAmbient(wrapper) == Buffer{0x67, 2});
    REQUIRE(readAmbient(wrapper) == Buffer{0x67, 2});
    REQUIRE(script->receiveCalls == 1);
    REQUIRE(script->writes.size() == 4);
    REQUIRE(script->writes[1] == hostAck);
    REQUIRE(script->writes[3] == Buffer{0x3e, 1, 0, 0, 0, 0, 0, 1, 0x3c});
}

TEST_CASE("Wrapper preserves a trailing partial next frame", "[stream][coalescing]")
{
    for (size_t split = 1; split < nextResponse.size(); ++split)
    {
        DYNAMIC_SECTION("Trailing prefix of " << split << " bytes")
        {
            Buffer firstRead = response;
            firstRead.insert(firstRead.end(), nextResponse.begin(), nextResponse.begin() + split);
            auto connector = std::make_unique<ScriptedConnector>(std::deque<Buffer>{
                firstRead, Buffer(nextResponse.begin() + split, nextResponse.end())});
            auto* script = connector.get();
            BluetoothWrapper wrapper(std::move(connector));
            REQUIRE(readAmbient(wrapper) == Buffer{0x67, 2});
            REQUIRE(readAmbient(wrapper) == Buffer{0x67, 2});
            REQUIRE(script->receiveCalls == 2);
        }
    }
}

TEST_CASE("Wrapper handles an ACK followed by a response in one read", "[stream][ack]")
{
    Buffer combined = hostAck;
    combined.insert(combined.end(), response.begin(), response.end());
    auto connector = std::make_unique<ScriptedConnector>(std::deque<Buffer>{combined, nextResponse});
    auto* script = connector.get();
    BluetoothWrapper wrapper(std::move(connector));
    REQUIRE(readAmbient(wrapper) == Buffer{0x67, 2});
    REQUIRE(script->receiveCalls == 1);
    REQUIRE(readAmbient(wrapper) == Buffer{0x67, 2});
    REQUIRE(script->writes[2] == Buffer{0x3e, 0x0c, 1, 0, 0, 0, 2, 0x66, 2, 0x77, 0x3c});
}

TEST_CASE("Trailing bytes do not invalidate a complete frame", "[stream][framing]")
{
    auto received = response;
    received.insert(received.end(), {0x55, 0x66});
    BluetoothWrapper wrapper(std::make_unique<ScriptedConnector>(std::deque<Buffer>{received}));
    REQUIRE(readAmbient(wrapper) == Buffer{0x67, 2});
}

TEST_CASE("Noise before START in the same receive is skipped", "[stream][baseline]")
{
    Buffer received = {0x55, 0x3c};
    received.insert(received.end(), response.begin(), response.end());
    BluetoothWrapper wrapper(std::make_unique<ScriptedConnector>(std::deque<Buffer>{received}));
    REQUIRE(readAmbient(wrapper) == Buffer{0x67, 2});
}

TEST_CASE("Missing or invalid delimiters exhaust the receive script", "[stream][malformed]")
{
    auto received = response;
    SECTION("Invalid START") { received.front() = 0x55; }
    SECTION("Invalid END") { received.back() = 0x55; }
    SECTION("Truncated frame") { received.pop_back(); }
    // Production keeps reading until a transport timeout/error; the script never sleeps.
    BluetoothWrapper wrapper(std::make_unique<ScriptedConnector>(std::deque<Buffer>{received}));
    REQUIRE_THROWS_MATCHES(readAmbient(wrapper), RecoverableException,
        Catch::Matchers::Message("Receive script exhausted"));
}

TEST_CASE("Nested START markers are rejected", "[stream][malformed]")
{
    BluetoothWrapper wrapper(std::make_unique<ScriptedConnector>(std::deque<Buffer>{{0x3e, 0x0c, 0x3e, 0x3c}}));
    REQUIRE_THROWS_MATCHES(readAmbient(wrapper), RecoverableException,
        Catch::Matchers::Message("Invalid: Multiple start markers without an end marker"));
}

TEST_CASE("Invalid framed bodies are rejected without a host ACK", "[stream][malformed]")
{
    auto received = response;
    SECTION("Declared length too large") { received[6] = 3; }
    SECTION("Declared length too small") { received[6] = 1; }
    SECTION("Invalid checksum") { received[9] ^= 1; }
    SECTION("Corrupted payload") { received[7] ^= 1; }
    SECTION("Truncated body") { received.erase(received.end() - 2); }
    auto connector = std::make_unique<ScriptedConnector>(std::deque<Buffer>{received});
    auto* script = connector.get();
    BluetoothWrapper wrapper(std::move(connector));
    REQUIRE_THROWS_AS(readAmbient(wrapper), RecoverableException);
    REQUIRE(script->writes.size() == 1);
}
