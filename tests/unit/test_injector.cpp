// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "InjectorBus.h"

#if __has_include(<nlohmann/json.hpp>)
#define RECPLAY_TEST_HAS_JSON 1
#else
#define RECPLAY_TEST_HAS_JSON 0
#endif

#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using recplay::InjectorBus;
using recplay::MutablePacketPtr;
using recplay::Packet;
using recplay::PacketPtr;

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::filesystem::path MakeTempPath(const std::string& stem, const std::string& extension) {
    return std::filesystem::temp_directory_path() / (stem + extension);
}

void WriteTextFile(const std::filesystem::path& path, const std::string& content) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    Expect(file.is_open(), "should create injector fixture file: " + path.string());
    file << content;
    Expect(file.good(), "should write injector fixture file: " + path.string());
}

MutablePacketPtr MakePacket(uint32_t channelId, std::vector<uint8_t> payload) {
    auto packet = std::make_shared<Packet>();
    packet->channel_id = channelId;
    packet->payload = std::move(payload);
    return packet;
}

void TestStaticReplaceRulesMutateMatchingPackets() {
    const auto rules_path = MakeTempPath("recplay-injector-replace", ".json");
    WriteTextFile(rules_path, R"([{"channel_id":7,"offset":1,"replace":[170,187]}])");

    InjectorBus injector;
    const bool loaded = injector.LoadStaticRules(rules_path.string());

#if RECPLAY_TEST_HAS_JSON
    Expect(loaded, "replace rules should load from JSON");
    Expect(injector.HasActiveRules(), "loading replace rules should activate injector state");

    const auto original = MakePacket(7, {0x10, 0x20, 0x30, 0x40});
    const PacketPtr input = original;
    const auto processed = injector.Process(input);

    Expect(processed != nullptr, "replace rules should keep packets flowing");
    Expect(processed != input, "mutated packets should be copied before modification");
    Expect(processed->payload == std::vector<uint8_t>({0x10, 0xAA, 0xBB, 0x40}),
           "replace rule should patch payload bytes at the configured offset");
    Expect(original->payload == std::vector<uint8_t>({0x10, 0x20, 0x30, 0x40}),
           "mutating copy should leave the original packet untouched");
#else
    Expect(!loaded, "replace rules should fail cleanly when JSON support is unavailable");
    Expect(!injector.HasActiveRules(), "failed rule load should not activate injector state");
#endif

    std::filesystem::remove(rules_path);
}

void TestDropRulesDiscardMatchingPacketsAndClearRestoresPassthrough() {
    const auto rules_path = MakeTempPath("recplay-injector-drop", ".json");
    WriteTextFile(rules_path, R"({"rules":[{"channel_id":7,"offset":0,"drop":true}]})");

    InjectorBus injector;
    const bool loaded = injector.LoadStaticRules(rules_path.string());

    const auto original = MakePacket(7, {0x01, 0x02, 0x03});
    const PacketPtr input = original;

#if RECPLAY_TEST_HAS_JSON
    Expect(loaded, "drop rules should load from JSON");
    Expect(injector.Process(input) == nullptr, "drop rules should discard matching packets");

    injector.ClearRules();
    Expect(!injector.HasActiveRules(), "ClearRules should remove all active injector rules");
    Expect(injector.Process(input) == input, "with no rules, packets should pass through unchanged");
#else
    Expect(!loaded, "drop rules should fail cleanly when JSON support is unavailable");
    Expect(injector.Process(input) == input, "without JSON support, packets should pass through unchanged");
#endif

    std::filesystem::remove(rules_path);
}

void TestLuaAndJsScriptRegistrationsCountAsActiveRules() {
    const auto lua_path = MakeTempPath("recplay-injector-script", ".lua");
    const auto js_path = MakeTempPath("recplay-injector-script", ".js");
    WriteTextFile(lua_path, "-- noop\n");
    WriteTextFile(js_path, "// noop\n");

    InjectorBus injector;
    Expect(injector.LoadLuaScript(lua_path.string()), "lua script should load when file exists");
    Expect(injector.LoadJsScript(js_path.string()), "js script should load when file exists");
    Expect(injector.HasActiveRules(), "loaded scripts should mark injector as active");

    injector.ClearRules();
    Expect(!injector.HasActiveRules(), "clearing injector should remove script registrations");

    std::filesystem::remove(lua_path);
    std::filesystem::remove(js_path);
}

} // namespace

int main() {
    try {
        TestStaticReplaceRulesMutateMatchingPackets();
        TestDropRulesDiscardMatchingPacketsAndClearRestoresPassthrough();
        TestLuaAndJsScriptRegistrationsCountAsActiveRules();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
}
