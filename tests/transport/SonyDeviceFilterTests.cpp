#include <catch2/catch_test_macros.hpp>
#include "sony/transport/FakeTransport.h"
#include "sony/transport/SonyDeviceFilter.h"

#include <memory>

using namespace sony::transport;

TEST_CASE("addressOui reads the prefix in every platform format", "[transport][discovery]")
{
    REQUIRE(addressOui("AC:80:0A:12:34:56") == 0xAC800Au); // Linux
    REQUIRE(addressOui("ac:80:0a:12:34:56") == 0xAC800Au); // Windows
    REQUIRE(addressOui("ac-80-0a-12-34-56") == 0xAC800Au); // macOS
    REQUIRE(addressOui("00:01:4a:00:00:00") == 0x00014Au);

    REQUIRE_FALSE(addressOui(""));
    REQUIRE_FALSE(addressOui("AC:80:0A"));
    REQUIRE_FALSE(addressOui("AC:80:0A:12:34:5G"));
    REQUIRE_FALSE(addressOui("AC.80.0A.12.34.56"));
    REQUIRE_FALSE(addressOui("AC800A123456"));
    REQUIRE_FALSE(addressOui("AC:80:0A:12:34:567"));
}

TEST_CASE("hasSonyOui accepts Sony audio companies only", "[transport][discovery]")
{
    REQUIRE(hasSonyOui("ac-80-0a-12-34-56"));  // Sony Corporation
    REQUIRE(hasSonyOui("CC:98:8B:12:34:56"));  // SONY Visual Products
    REQUIRE(hasSonyOui("00:01:4A:12:34:56"));  // Sony Corporation, first table entry

    REQUIRE_FALSE(hasSonyOui("88:C6:26:12:34:56")); // Logitech (UE BOOM)
    REQUIRE_FALSE(hasSonyOui("74:77:86:12:34:56")); // Apple
    REQUIRE_FALSE(hasSonyOui("00:04:1F:12:34:56")); // Sony Interactive Entertainment
    REQUIRE_FALSE(hasSonyOui("24:A1:0D:22:34:56")); // Sony Honda Mobility, a 28-bit block
    REQUIRE_FALSE(hasSonyOui("not an address"));
}

TEST_CASE("hasSonyName matches Sony model prefixes in any case", "[transport][discovery]")
{
    REQUIRE(hasSonyName("WH-1000XM4"));
    REQUIRE(hasSonyName("wf-1000xm5"));
    REQUIRE(hasSonyName("WI-C100"));
    REQUIRE(hasSonyName("MDR-1000X"));
    REQUIRE(hasSonyName("LinkBuds S"));
    REQUIRE(hasSonyName("ULT WEAR"));
    REQUIRE(hasSonyName("Sony XM4"));

    REQUIRE_FALSE(hasSonyName("UE BOOM 2"));
    REQUIRE_FALSE(hasSonyName("AirPods Pro"));
    REQUIRE_FALSE(hasSonyName(""));
}

TEST_CASE("isSonyCandidate accepts a Sony prefix or a Sony name", "[transport][discovery]")
{
    // A renamed device keeps its Sony address prefix.
    REQUIRE(isSonyCandidate({.name = "Living room headphones", .address = "AC:80:0A:12:34:56"}));
    // A Sony model with an address outside the table keeps its model name.
    REQUIRE(isSonyCandidate({.name = "WH-1000XM4", .address = "12:34:56:78:9A:BC"}));
    // A PlayStation controller has a Sony Interactive prefix and a generic name.
    REQUIRE_FALSE(isSonyCandidate({.name = "Wireless Controller", .address = "00:04:1F:12:34:56"}));
    REQUIRE_FALSE(isSonyCandidate({.name = "UE BOOM 2", .address = "88:C6:26:12:34:56"}));
}

TEST_CASE("SonyDeviceDiscovery keeps only Sony devices in their order", "[transport][discovery]")
{
    auto inner = std::make_unique<FakeDeviceDiscovery>();
    inner->setDevices({
        {.name = "Phone", .address = "6c-3a-ff-12-34-56", .paired = true, .connected = false},
        {.name = "AirPods Pro", .address = "74-77-86-12-34-56", .paired = true, .connected = true},
        {.name = "UE BOOM 2", .address = "88-c6-26-12-34-56", .paired = true, .connected = false},
        {.name = "Sony XM4", .address = "ac-80-0a-12-34-56", .paired = true, .connected = false},
        {.name = "MX Master 3", .address = "d4-7f-fe-12-34-56", .paired = true, .connected = true},
        {.name = "WF-1000XM5", .address = "12-34-56-78-9a-bc", .paired = true, .connected = true},
    });
    SonyDeviceDiscovery discovery(std::move(inner));

    const auto devices = discovery.discover();

    REQUIRE(devices.size() == 2);
    REQUIRE(devices[0] == DiscoveredDevice{.name = "Sony XM4", .address = "ac-80-0a-12-34-56", .paired = true, .connected = false});
    REQUIRE(devices[1] == DiscoveredDevice{.name = "WF-1000XM5", .address = "12-34-56-78-9a-bc", .paired = true, .connected = true});
}

TEST_CASE("SonyDeviceDiscovery without an inner discovery lists nothing", "[transport][discovery]")
{
    SonyDeviceDiscovery discovery(nullptr);
    REQUIRE(discovery.discover().empty());
}
