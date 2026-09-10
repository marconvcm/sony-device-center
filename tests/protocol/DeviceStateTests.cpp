#include <catch2/catch_test_macros.hpp>
#include "sony/protocol/DeviceState.h"
#include "Headphones.h"
#include "BluetoothWrapper.h"
#include "CommandSerializer.h"

#include <thread>
#include <vector>
#include <atomic>
#include <deque>
#include <mutex>
#include <algorithm>

using namespace sony::protocol;

namespace {

class AutoAckConnector final : public IBluetoothConnector {
public:
    int send(char* /*buffer*/, size_t length) override {
        std::lock_guard lock(_mtx);
        _pendingAcks.push_back(CommandSerializer::packageDataForBt({}, DATA_TYPE::ACK, _seq++));
        return static_cast<int>(length);
    }

    int recv(char* buffer, size_t length) override {
        std::lock_guard lock(_mtx);
        if (_pendingAcks.empty()) {
            auto ack = CommandSerializer::packageDataForBt({}, DATA_TYPE::ACK, _seq);
            size_t toCopy = std::min(length, ack.size());
            std::copy_n(ack.data(), toCopy, buffer);
            return static_cast<int>(toCopy);
        }
        auto& front = _pendingAcks.front();
        size_t toCopy = std::min(length, front.size());
        std::copy_n(front.data(), toCopy, buffer);
        front.erase(front.begin(), front.begin() + toCopy);
        if (front.empty()) {
            _pendingAcks.pop_front();
        }
        return static_cast<int>(toCopy);
    }

    void connect(const std::string&) override { _connected = true; }
    void disconnect() noexcept override { _connected = false; }
    bool isConnected() noexcept override { return _connected; }
    std::vector<BluetoothDevice> getConnectedDevices() override { return {}; }
    SonyProtocolVersion getProtocolVersion() noexcept override { return SonyProtocolVersion::V2; }

private:
    std::mutex _mtx;
    std::deque<std::vector<char>> _pendingAcks;
    unsigned char _seq{0};
    bool _connected{true};
};

} // namespace

TEST_CASE("DeviceState default initialization and immutability", "[device_state]") {
    DeviceState state;

    CHECK_FALSE(state.battery.main.has_value());
    CHECK_FALSE(state.battery.left.has_value());
    CHECK_FALSE(state.battery.right.has_value());
    CHECK_FALSE(state.battery.caseBattery.has_value());
    CHECK_FALSE(state.battery.charging);

    CHECK(state.noiseControl.mode == NoiseControlMode::Off);
    CHECK(state.noiseControl.ambientLevel == 0);
    CHECK_FALSE(state.noiseControl.focusOnVoice);

    CHECK(state.equalizer.preset == 0);
    CHECK(state.equalizer.clearBass == 0);
    for (int band : state.equalizer.bands) {
        CHECK(band == 0);
    }

    CHECK_FALSE(state.dsee);
    CHECK(state.firmware.empty());
    CHECK(state.codec.empty());
    CHECK(state.autoPowerOff == 0);
    CHECK_FALSE(state.speakToChat);
    CHECK_FALSE(state.adaptiveVolume);

    state.battery.main = 85;
    state.battery.charging = true;
    state.noiseControl.mode = NoiseControlMode::NoiseCancelling;
    state.firmware = "2.0.1";

    DeviceStateSnapshot snapshot = std::make_shared<const DeviceState>(state);
    CHECK(snapshot->battery.main.value() == 85);
    CHECK(snapshot->battery.charging);
    CHECK(snapshot->noiseControl.mode == NoiseControlMode::NoiseCancelling);
    CHECK(snapshot->firmware == "2.0.1");

    // Modify local state and verify snapshot is unaffected
    state.battery.main = 40;
    CHECK(snapshot->battery.main.value() == 85);
}

TEST_CASE("Headphones state and snapshot consistency", "[device_state]") {
    auto connector = std::make_unique<AutoAckConnector>();
    BluetoothWrapper conn(std::move(connector));
    Headphones hp(conn);

    auto snap0 = hp.snapshot();
    CHECK_FALSE(snap0->battery.main.has_value());
    CHECK(snap0->battery.main.value_or(-1) == -1);
    CHECK(hp.getBatteryLevel() == -1);

    // Update auto power off
    hp.setAutoPowerOff(3);
    auto snap1 = hp.snapshot();
    CHECK(snap1->autoPowerOff == 3);
    CHECK(snap0->autoPowerOff == 0); // Previous snapshot unchanged

    // Update equalizer custom
    hp.setEqualizerCustom(5, {1, 2, 3, 4, 5});
    auto snap2 = hp.snapshot();
    CHECK(snap2->equalizer.clearBass == 5);
    CHECK(snap2->equalizer.bands[0] == 1);
    CHECK(snap2->equalizer.bands[4] == 5);
    CHECK(snap1->equalizer.clearBass == 0); // Previous snapshot unchanged

    // Update DSEE
    hp.setDsee(true);
    auto snap3 = hp.snapshot();
    CHECK(snap3->dsee == true);
    CHECK(snap2->dsee == false);

    // Update speak to chat & adaptive volume
    hp.setSpeakToChat(true);
    hp.setAdaptiveVolume(true);
    auto snap4 = hp.snapshot();
    CHECK(snap4->speakToChat == true);
    CHECK(snap4->adaptiveVolume == true);
}

TEST_CASE("Headphones concurrent state updates and snapshot reads", "[device_state][concurrency]") {
    auto connector = std::make_unique<AutoAckConnector>();
    BluetoothWrapper conn(std::move(connector));
    Headphones hp(conn);

    std::atomic<bool> running{true};
    std::atomic<bool> validationFailed{false};
    std::atomic<int> snapshotCount{0};

    // Reader threads taking snapshots continuously
    std::vector<std::thread> readers;
    for (int i = 0; i < 4; ++i) {
        readers.emplace_back([&]() {
            while (running.load(std::memory_order_relaxed)) {
                auto snap = hp.snapshot();
                if (!snap) {
                    validationFailed.store(true, std::memory_order_relaxed);
                    break;
                }
                // Verify internal snapshot invariants
                int clearBass = snap->equalizer.clearBass;
                if (clearBass < -10 || clearBass > 10) {
                    validationFailed.store(true, std::memory_order_relaxed);
                    break;
                }
                int apo = snap->autoPowerOff;
                if (apo < 0 || apo > 5) {
                    validationFailed.store(true, std::memory_order_relaxed);
                    break;
                }
                snapshotCount.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    // Writer threads modifying state concurrently
    std::vector<std::thread> writers;
    for (int i = 0; i < 2; ++i) {
        writers.emplace_back([&]() {
            for (int step = 0; step < 200; ++step) {
                hp.setAutoPowerOff(step % 6);
                hp.setDsee(step % 2 == 0);
                hp.setSpeakToChat(step % 3 == 0);
                hp.setAdaptiveVolume(step % 2 == 1);
                hp.setEqualizerCustom((step % 21) - 10, {1, 2, 3, 4, 5});
            }
        });
    }

    for (auto& w : writers) {
        w.join();
    }

    running.store(false, std::memory_order_relaxed);
    for (auto& r : readers) {
        r.join();
    }

    CHECK_FALSE(validationFailed.load());
    CHECK(snapshotCount.load() > 0);
}
