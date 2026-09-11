#include <catch2/catch_test_macros.hpp>
#include "sony/core/DeviceService.h"
#include "sony/core/JsonProtocol.h"
#include "sony/core/IpcServer.h"
#include "sony/core/IpcClient.h"
#include "../support/PrivateSocket.h"
#include "../support/ReplyTransport.h"
#include <future>
#include <fstream>
#ifndef _WIN32
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <cstring>
#endif
using namespace sony;
using namespace sony::core;
using namespace sony::transport;
using Json = JsonProtocol::Json;

TEST_CASE("Lifecycle retries with backoff and rediscovers without a restart", "[core][recovery]") {
    auto now = DeviceService::Clock::time_point{};
    auto transport = std::make_shared<ReplyTransport>();
    auto discovery = std::make_shared<FakeDeviceDiscovery>();
    DeviceService service(transport, discovery, [&] { return now; });
    service.startAutoConnect(); service.tick();
    CHECK(service.connectionState() == "retrying");
    discovery->addDevice({"WH-1000XM3", DeviceAddress("11:22:33:44:55:66"), true, true});
    service.tick(); CHECK(transport->attempts.empty());
    now += std::chrono::seconds(1); service.tick();
    REQUIRE(service.isConnected());
    CHECK(transport->attempts.size() == 1);
    transport->simulateDisconnect(); service.tick();
    REQUIRE(service.isConnected());
    CHECK(transport->attempts.size() == 2);
    service.disconnect(); now += std::chrono::hours(1); service.tick();
    CHECK_FALSE(service.isConnected()); CHECK(transport->attempts.size() == 2);
    CHECK(service.connectionState() == "manually_disconnected");
    for (const auto& raw : transport->sentFrames()) {
        const auto frame = protocol::FrameCodec::decode(raw);
        if (!frame.payload.empty()) CHECK(frame.payload[0] != 0x22);
    }
}
TEST_CASE("Lifecycle prefers connected candidates and honors explicit selection", "[core][recovery]") {
    auto transport = std::make_shared<ReplyTransport>();
    auto discovery = std::make_shared<FakeDeviceDiscovery>();
    discovery->setDevices({{"WH-1000XM3",DeviceAddress("11:22:33:44:55:66"),true,false},
        {"WH-1000XM3",DeviceAddress("22:22:33:44:55:66"),true,true}});
    DeviceService service(transport, discovery);
    SECTION("Connected candidate first") {
        service.startAutoConnect(); service.tick();
        CHECK(service.selectedAddress() == "22:22:33:44:55:66");
    }
    SECTION("Failed candidate falls through") {
        transport->failAddress = "22:22:33:44:55:66";
        service.startAutoConnect(); service.tick();
        CHECK(service.selectedAddress() == "11:22:33:44:55:66");
        CHECK(transport->attempts.size() == 2);
    }
    SECTION("Explicit address is the only candidate") {
        service.startAutoConnect("11:22:33:44:55:66"); service.tick();
        CHECK(service.selectedAddress() == "11:22:33:44:55:66");
        CHECK(transport->attempts.size() == 1);
    }
}
TEST_CASE("Retry delay doubles and is capped at thirty seconds", "[core][recovery]") {
    auto now = DeviceService::Clock::time_point{};
    auto transport = std::make_shared<ReplyTransport>();
    transport->failAddress = "11:22:33:44:55:66";
    DeviceService service(transport, {}, [&] { return now; });
    service.startAutoConnect(transport->failAddress); service.tick();
    size_t count = 1;
    for (int delay : {1,2,4,8,16,30,30}) {
        now += std::chrono::milliseconds(delay * 1000 - 1); service.tick();
        CHECK(transport->attempts.size() == count);
        now += std::chrono::milliseconds(1); service.tick();
        CHECK(transport->attempts.size() == ++count);
    }
}
TEST_CASE("Structured snapshots are escaped versioned and truthful", "[core][json]") {
    auto transport = std::make_shared<ReplyTransport>();
    auto discovery = std::make_shared<FakeDeviceDiscovery>();
    discovery->addDevice({"name\n\"|<img> 日本",DeviceAddress("11:22:33:44:55:66")});
    DeviceService service(transport, discovery);
    auto request = Json{{"version",1},{"id","test"},{"method","devices"}};
    auto response = Json::parse(JsonProtocol::executeLine(request.dump(), service));
    CHECK(response["id"] == "test");
    CHECK(response["data"][0]["name"] == "name\n\"|<img> 日本");
    request["version"] = 9;
    CHECK(JsonProtocol::execute(request,service)["error"]["code"] == "VersionMismatch");
    CHECK(Json::parse(JsonProtocol::executeLine("{broken",service))["ok"] == false);
    const auto deep = std::string(40, '[') + "0" + std::string(40, ']');
    CHECK(Json::parse(JsonProtocol::executeLine(deep,service))["error"]["code"] == "InvalidRequest");
    service.connect(DeviceAddress("11:22:33:44:55:66"),"WH-1000XM5");
    const auto snapshot = JsonProtocol::snapshot(service);
    CHECK(snapshot["connected"] == true);
    CHECK(snapshot["equalizer"]["bands"] == Json::array({1,2,3,4,5}));
    CHECK(snapshot["features"]["noiseControl"]["availability"] == "valid");
    CHECK(snapshot["codec"] == "Unknown");
    CHECK(JsonProtocol::execute({{"version",1},{"id",1},{"method","ambient"},{"params",{{"level",21}}}},service)["error"]["code"] == "InvalidRequest");
    service.disconnect();
    CHECK(JsonProtocol::snapshot(service)["features"]["noiseControl"]["availability"] == "stale");
    auto status = IpcProtocol::execute(IpcProtocol::parseCommand("status"),service);
    CHECK_FALSE(status.success); CHECK(status.message == "Device disconnected");
}
TEST_CASE("Notification callbacks may read state from another thread", "[core][events]") {
    auto transport = std::make_shared<ReplyTransport>();
    SonyDevice device(transport); device.connect(DeviceAddress("11:22:33:44:55:66"),"WH-1000XM5");
    std::promise<bool> completed;
    auto result = completed.get_future();
    auto id = device.events().onStateChanged([&](const auto&) {
        auto read = std::async(std::launch::async, [&] { return device.snapshot()->noiseControl.ambientLevel; });
        completed.set_value(read.get() == 12);
    });
    transport->notify({0x69,0x17,1,1,1,0,12});
    REQUIRE(result.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    CHECK(result.get()); device.events().removeListener(id);
}
#ifndef _WIN32
namespace {
struct ClientFd {
    int fd{-1};
    explicit ClientFd(const std::string& path) {
        fd = ::socket(AF_UNIX,SOCK_STREAM,0);
        sockaddr_un address{}; address.sun_family=AF_UNIX;
        std::strcpy(address.sun_path,path.c_str());
        REQUIRE(::connect(fd,reinterpret_cast<sockaddr*>(&address),sizeof(address)) == 0);
    }
    ~ClientFd() { if (fd >= 0) ::close(fd); }
};
}
TEST_CASE("IPC isolates idle oversized and disappearing clients", "[core][ipc][security]") {
    PrivateSocket socket;
    auto service = std::make_shared<DeviceService>(std::make_shared<FakeTransport>());
    IpcServer server(service,socket.path); server.start();
    IpcClient client(socket.path);
    SECTION("Idle client does not delay other clients") {
        ClientFd idle(socket.path);
        auto result = client.sendCommand("devices",std::chrono::milliseconds(300));
        CHECK(result.success);
    }
    SECTION("Oversized request is disconnected") {
        ClientFd attacker(socket.path);
        std::string bytes(17*1024,'x');
#ifdef MSG_NOSIGNAL
        ::send(attacker.fd,bytes.data(),bytes.size(),MSG_NOSIGNAL);
#else
        ::write(attacker.fd,bytes.data(),bytes.size());
#endif
        pollfd pfd{attacker.fd,POLLIN,0}; REQUIRE(::poll(&pfd,1,2000) > 0);
        char b; CHECK(::read(attacker.fd,&b,1) <= 0);
        CHECK(client.sendCommand("devices").success);
    }
    SECTION("Client disappears before response") {
        { ClientFd vanished(socket.path); ::write(vanished.fd,"devices\n",8); }
        CHECK(client.sendCommand("devices").success);
    }
    SECTION("JSON and legacy clients coexist") {
        auto response = Json::parse(client.request(R"({"version":1,"id":7,"method":"snapshot"})"));
        CHECK(response["ok"] == true); CHECK(response["id"] == 7);
        CHECK(client.sendCommand("devices").success);
    }
    SECTION("Second daemon cannot evict the first") {
        IpcServer second(service,socket.path); CHECK_THROWS(second.start());
        second.stop(); CHECK(client.sendCommand("devices").success);
    }
    struct stat st{}; REQUIRE(::lstat(socket.path.c_str(),&st) == 0);
    CHECK((st.st_mode & 0777) == 0600);
}
TEST_CASE("IPC refuses insecure paths and recovers an owned stale socket", "[core][ipc][security]") {
    PrivateSocket socket;
    auto service = std::make_shared<DeviceService>(std::make_shared<FakeTransport>());
    SECTION("Private parent required") {
        ::chmod(socket.directory.c_str(),0755);
        IpcServer server(service,socket.path); CHECK_THROWS(server.start());
    }
    SECTION("Never replaces a regular file") {
        std::ofstream(socket.path) << "keep";
        IpcServer server(service,socket.path); CHECK_THROWS(server.start());
        CHECK(std::filesystem::file_size(socket.path) == 4);
    }
    SECTION("Never follows a symlink") {
        std::filesystem::create_symlink(socket.directory / "missing",socket.path);
        IpcServer server(service,socket.path); CHECK_THROWS(server.start());
        CHECK(std::filesystem::is_symlink(socket.path));
    }
    SECTION("Stale socket can be reclaimed") {
        int fd = ::socket(AF_UNIX,SOCK_STREAM,0);
        sockaddr_un address{}; address.sun_family=AF_UNIX; std::strcpy(address.sun_path,socket.path.c_str());
        REQUIRE(::bind(fd,reinterpret_cast<sockaddr*>(&address),sizeof(address)) == 0); ::close(fd);
        IpcServer server(service,socket.path); REQUIRE_NOTHROW(server.start());
        CHECK(IpcClient(socket.path).sendCommand("devices").success);
    }
}
#endif
