#include <catch2/catch_test_macros.hpp>
#include "sony/transport/Logger.h"

#include <string>
#include <vector>

using namespace sony;

TEST_CASE("Logger categories match expected names", "[logger]") {
    CHECK(LogCategory::Transport == "sony.transport");
    CHECK(LogCategory::Protocol == "sony.protocol");
    CHECK(LogCategory::Session == "sony.session");
    CHECK(LogCategory::Device == "sony.device");
    CHECK(LogCategory::Capabilities == "sony.capabilities");
    CHECK(LogCategory::State == "sony.state");
}

TEST_CASE("Logger levels and sink redirection", "[logger]") {
    struct LogEntry {
        LogLevel level;
        std::string category;
        std::string message;
    };

    std::vector<LogEntry> entries;
    Logger::setLogSink([&](LogLevel level, std::string_view cat, std::string_view msg) {
        entries.push_back({level, std::string(cat), std::string(msg)});
    });

    Logger::setLogLevel(LogLevel::Info);
    Logger::debug(LogCategory::Protocol, "Should be ignored");
    CHECK(entries.empty());

    Logger::info(LogCategory::Transport, "Connection established");
    CHECK(entries.size() == 1);
    CHECK(entries[0].level == LogLevel::Info);
    CHECK(entries[0].category == "sony.transport");
    CHECK(entries[0].message == "Connection established");

    Logger::warn(LogCategory::Device, "Low battery warning");
    CHECK(entries.size() == 2);
    CHECK(entries[1].level == LogLevel::Warn);

    Logger::error(LogCategory::Session, "Session reset");
    CHECK(entries.size() == 3);
    CHECK(entries[2].level == LogLevel::Error);

    // Turn off developer mode and verify log filtering
    Logger::setDeveloperMode(true);
    CHECK(Logger::isDeveloperMode());
    CHECK(Logger::getLogLevel() == LogLevel::Debug);

    Logger::debug(LogCategory::Protocol, "Now visible in dev mode");
    CHECK(entries.size() == 4);
    CHECK(entries[3].message == "Now visible in dev mode");

    // Clean up
    Logger::resetLogSink();
    Logger::setDeveloperMode(false);
    Logger::setLogLevel(LogLevel::Off);
}

TEST_CASE("Developer diagnostic payload description", "[logger]") {
    // BATTERY_GET
    std::vector<uint8_t> batGet = {0x22, 0x00};
    CHECK(Logger::describePayload(batGet) == "BATTERY_GET");

    // BATTERY_RET level=87
    std::vector<uint8_t> batRet = {0x23, 0x00, 87, 0x00};
    CHECK(Logger::describePayload(batRet) == "BATTERY_RET level=87");

    // BATTERY_RET L=90 R=85
    std::vector<uint8_t> twsRet = {0x23, 0x09, 90, 0, 85, 0};
    CHECK(Logger::describePayload(twsRet) == "BATTERY_RET L=90 R=85");

    // BATTERY_NTFY
    std::vector<uint8_t> batNtf = {0x25, 0x00, 82, 0x01};
    CHECK(Logger::describePayload(batNtf) == "BATTERY_NTFY level=82");

    // NCASM_SET mode=Ambient level=10
    // Payload layout: 0x68, 0x00, 0x01 (Ambient), 0x00, 10
    std::vector<uint8_t> ncasmAmbient = {0x68, 0x00, 0x01, 0x00, 10};
    CHECK(Logger::describePayload(ncasmAmbient) == "NCASM_SET mode=Ambient level=10");

    // NCASM_SET mode=NoiseCancelling
    std::vector<uint8_t> ncasmNc = {0x68, 0x00, 0x00, 0x00, 0};
    CHECK(Logger::describePayload(ncasmNc) == "NCASM_SET mode=NoiseCancelling");

    // EQ_GET and EQ_RET
    std::vector<uint8_t> eqGet = {0x56, 0x00};
    CHECK(Logger::describePayload(eqGet) == "EQ_GET");
    std::vector<uint8_t> eqRet = {0x57, 0x00, 0x16};
    CHECK(Logger::describePayload(eqRet) == "EQ_RET preset=22");

    // DSEE
    std::vector<uint8_t> dseeGet = {0xe6, 0x01};
    CHECK(Logger::describePayload(dseeGet) == "DSEE_GET");
    std::vector<uint8_t> dseeSet = {0xe8, 0x01, 0x01};
    CHECK(Logger::describePayload(dseeSet) == "DSEE_SET enabled=1");
}

TEST_CASE("Developer logging TX and RX formatting", "[logger]") {
    std::vector<std::string> logged;
    Logger::setLogSink([&](LogLevel, std::string_view, std::string_view msg) {
        logged.push_back(std::string(msg));
    });
    Logger::setLogLevel(LogLevel::Debug);

    std::vector<uint8_t> sampleFrame = {0x3e, 0x0c, 0x00, 0x00, 0x00, 0x02, 0x22, 0x00, 0x24, 0x3c};
    Logger::logTx(sampleFrame, "BATTERY_GET");
    CHECK(logged.size() == 1);
    CHECK(logged[0].rfind("TX  3e 0c 00", 0) == 0);
    CHECK(logged[0].find("BATTERY_GET") != std::string::npos);

    Logger::logRx(sampleFrame);
    CHECK(logged.size() == 2);
    CHECK(logged[1].rfind("RX  3e 0c 00", 0) == 0);

    Logger::resetLogSink();
    Logger::setLogLevel(LogLevel::Off);
}
