#include <catch2/catch_test_macros.hpp>
#include "sony/core/DeviceService.h"
#include "sony/core/SimulatedDevice.h"
#include <chrono>
#include <thread>

using namespace sony;
using namespace sony::core;
using namespace sony::protocol;

namespace {
// Notifications arrive on the device's reader thread; give it a moment.
template <typename Pred>
bool eventually(Pred pred) {
    for (int i = 0; i < 100 && !pred(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return pred();
}
}

TEST_CASE("Simulated device connects and answers the V2 refresh queries", "[core][simulated]") {
    auto simulated = createSimulatedDevice();
    DeviceService service(simulated.transport, simulated.discovery);
    service.startAutoConnect(simulated.address);
    service.tick();
    REQUIRE(service.isConnected());
    CHECK(service.selectedAddress() == "CC:98:8B:00:11:22");

    const auto state = service.snapshot();
    REQUIRE(state);
    CHECK(state->battery.main == 87);
    CHECK_FALSE(state->battery.charging);
    CHECK(state->noiseControl.mode == NoiseControlMode::NoiseCancelling);
    CHECK(state->equalizer.preset == 0x16);
    CHECK(state->equalizer.clearBass == 4);
    CHECK(state->dsee);
}

TEST_CASE("Simulated device applies writes and reports them back", "[core][simulated]") {
    auto simulated = createSimulatedDevice("WH-1000XM5", "00:11:22:33:44:55");
    DeviceService service(simulated.transport, simulated.discovery);
    service.startAutoConnect(simulated.address);
    service.tick();
    REQUIRE(service.isConnected());
    auto* device = service.activeDevice();
    REQUIRE(device);

    NoiseControlState ambient;
    ambient.mode = NoiseControlMode::Ambient;
    ambient.ambientLevel = 12;
    ambient.focusOnVoice = true;
    device->setNoiseControl(ambient);
    device->refreshNoiseControl();
    auto nc = service.snapshot()->noiseControl;
    CHECK(nc.mode == NoiseControlMode::Ambient);
    CHECK(nc.ambientLevel == 12);
    CHECK(nc.focusOnVoice);

    device->setEqualizerCustom(-3, {1, 2, 3, 4, 5});
    device->refreshEqualizer();
    auto eq = service.snapshot()->equalizer;
    CHECK(eq.preset == 0xa0);
    CHECK(eq.clearBass == -3);
    CHECK(eq.bands == std::array<int, 5>{1, 2, 3, 4, 5});

    device->setDsee(false);
    device->refreshDsee();
    CHECK_FALSE(service.snapshot()->dsee);
}

TEST_CASE("Simulated device goes away after power off and stays away", "[core][simulated]") {
    auto simulated = createSimulatedDevice();
    DeviceService service(simulated.transport, simulated.discovery);
    service.startAutoConnect(simulated.address);
    service.tick();
    REQUIRE(service.isConnected());

    service.activeDevice()->powerOff();
    REQUIRE(eventually([&] { service.tick(); return !service.isConnected(); }));
    // Auto-connect keeps trying, but a switched-off headset does not answer.
    for (int i = 0; i < 5; ++i) service.tick();
    CHECK_FALSE(service.isConnected());
    CHECK(service.connectionState() != "manually_disconnected");
}
