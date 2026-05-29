// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "CoreEngine.h"
#include "IProtocolService.h"
#include "IStorageService.h"
#include "SessionController.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using recplay::ChannelInfo;
using recplay::CoreEngine;
using recplay::IProtocolService;
using recplay::IStorageService;
using recplay::Packet;
using recplay::PacketCallback;
using recplay::PacketPtr;
using recplay::RpcapHeader;
using recplay::SessionController;
using recplay::SessionState;

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

PacketPtr MakePacket(uint64_t captureTime, uint32_t channelId) {
    auto packet = std::make_shared<Packet>();
    packet->t_capture = captureTime;
    packet->t_origin = captureTime;
    packet->t_record = captureTime;
    packet->channel_id = channelId;
    packet->payload = {0x01, 0x02, 0x03};
    return packet;
}

class FakeStorageService final : public IStorageService {
public:
    bool CreateFile(const std::string& path,
                    const std::vector<ChannelInfo>& channels,
                    const std::string& codec) override {
        create_path = path;
        create_codec = codec;
        created_channels = channels;
        writing = true;
        finalized = false;
        written_packets.clear();
        return true;
    }

    bool WritePacket(PacketPtr pkt) override {
        if (!writing || !pkt) {
            return false;
        }
        written_packets.push_back(pkt);
        return true;
    }

    bool FinalizeFile() override {
        writing = false;
        finalized = true;
        return true;
    }

    bool OpenFile(const std::string& path) override {
        opened_path = path;
        reading = true;
        read_index = 0;
        return true;
    }

    void CloseFile() override {
        reading = false;
        closed = true;
    }

    RpcapHeader GetHeader() const override {
        RpcapHeader header;
        header.duration_ns = playback_duration_ns;
        return header;
    }

    std::vector<ChannelInfo> GetChannels() const override {
        return playback_channels;
    }

    bool SeekTo(uint64_t timestamp_ns) override {
        last_seek_ns = timestamp_ns;
        if (block_seek) {
            std::unique_lock<std::mutex> lock(seek_mutex);
            seek_waiting = true;
            seek_cv.notify_all();
            seek_cv.wait(lock, [this]() { return !block_seek; });
        }
        for (size_t i = 0; i < playback_packets.size(); ++i) {
            if (playback_packets[i] && playback_packets[i]->t_capture >= timestamp_ns) {
                read_index = i;
                return true;
            }
        }
        read_index = playback_packets.size();
        return true;
    }

    PacketPtr ReadNext() override {
        if (!reading || read_index >= playback_packets.size()) {
            return nullptr;
        }
        return playback_packets[read_index++];
    }

    bool HasMore() const override {
        return reading && read_index < playback_packets.size();
    }

    std::vector<uint64_t> GetKeyframeTimestamps() const override {
        return {};
    }

    bool IsWriting() const override {
        return writing;
    }

    bool IsReading() const override {
        return reading;
    }

    void WaitForBlockedSeek() {
        std::unique_lock<std::mutex> lock(seek_mutex);
        seek_cv.wait(lock, [this]() { return seek_waiting; });
    }

    void ReleaseBlockedSeek() {
        {
            std::lock_guard<std::mutex> lock(seek_mutex);
            block_seek = false;
        }
        seek_cv.notify_all();
    }

    std::string create_path;
    std::string create_codec;
    std::vector<ChannelInfo> created_channels;
    std::vector<PacketPtr> written_packets;
    std::vector<PacketPtr> playback_packets;
    std::vector<ChannelInfo> playback_channels;
    std::string opened_path;
    uint64_t playback_duration_ns = 0;
    uint64_t last_seek_ns = 0;
    bool writing = false;
    bool reading = false;
    bool finalized = false;
    bool closed = false;
    size_t read_index = 0;
    bool block_seek = false;
    bool seek_waiting = false;
    std::mutex seek_mutex;
    std::condition_variable seek_cv;
};

class FakeProtocolService final : public IProtocolService {
public:
    explicit FakeProtocolService(std::string protocolName)
        : name(std::move(protocolName)) {}

    std::string GetName() const override { return name; }
    std::string GetVersion() const override { return "1.0.0"; }
    int GetPriority() const override { return 100; }

    bool StartCapture(const std::string& configJson, PacketCallback cb) override {
        capture_started = true;
        last_capture_config = configJson;
        capture_callback = std::move(cb);
        return true;
    }

    void StopCapture() override { capture_started = false; }
    bool IsCapturing() const override { return capture_started; }

    bool StartReplay(const std::string& configJson) override {
        replay_started = true;
        last_replay_config = configJson;
        return true;
    }

    bool SendPacket(PacketPtr pkt) override {
        if (!replay_started || !pkt) {
            return false;
        }
        replayed_packets.push_back(pkt);
        return true;
    }

    void StopReplay() override { replay_started = false; }
    bool IsReplaying() const override { return replay_started; }

    std::string GetConfigSchema() const override { return "{}"; }
    std::vector<ChannelInfo> GetChannels() const override { return channels; }

    void Emit(PacketPtr pkt) const {
        if (capture_callback) {
            capture_callback(std::move(pkt));
        }
    }

    std::string name;
    std::vector<ChannelInfo> channels;
    mutable PacketCallback capture_callback;
    std::vector<PacketPtr> replayed_packets;
    std::string last_capture_config;
    std::string last_replay_config;
    bool capture_started = false;
    bool replay_started = false;
};

void TestRecordingLifecycle() {
    CoreEngine engine;
    FakeStorageService storage;
    FakeProtocolService udp("UDP");
    udp.channels.push_back(ChannelInfo{7, "udp-src", "UDP", "udp/topic", {}});

    engine.SetStorageService(&storage);
    engine.AttachProtocol("UDP", &udp);

    SessionController controller(&engine);
    const bool started = controller.StartRecording(
        R"({"output_path":"capture.rpcap","codec":"zstd","protocols":["UDP"],"protocol_config":{"port":5000}})");

    Expect(started,
           "recording should start: create_path=" + storage.create_path +
           ", capture_started=" + std::string(udp.capture_started ? "true" : "false"));
    Expect(controller.GetState() == SessionState::Recording, "state should be Recording");
    Expect(storage.create_path == "capture.rpcap", "storage should receive output path");
    Expect(storage.create_codec == "zstd", "storage should receive codec");
    Expect(storage.created_channels.size() == 1, "storage should receive protocol channels");
    Expect(udp.capture_started, "protocol capture should start");
    Expect(udp.last_capture_config.find("\"port\":5000") != std::string::npos, "protocol config should be forwarded");

    udp.Emit(MakePacket(100, 7));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    Expect(storage.written_packets.size() == 1, "captured packet should be written through core engine");

    controller.PauseRecording();
    Expect(controller.GetState() == SessionState::RecordingPaused, "state should pause recording");
    controller.ResumeRecording();
    Expect(controller.GetState() == SessionState::Recording, "state should resume recording");

    controller.StopRecording();
    Expect(controller.GetState() == SessionState::Stopped, "state should stop recording");
    Expect(storage.finalized, "storage should finalize file");
    Expect(!udp.capture_started, "protocol capture should stop");
}

void TestPlaybackLifecycle() {
    CoreEngine engine;
    FakeStorageService storage;
    FakeProtocolService udp("UDP");
    FakeProtocolService tcp("TCP");

    storage.playback_duration_ns = 200;
    storage.playback_channels = {ChannelInfo{7, "udp-src", "UDP", "udp/topic", {}}};
    storage.playback_packets = {MakePacket(10, 3), MakePacket(40, 3), MakePacket(90, 3)};

    engine.SetStorageService(&storage);
    engine.AttachProtocol("UDP", &udp);
    engine.AttachProtocol("TCP", &tcp);

    SessionController controller(&engine);
    const bool opened = controller.OpenForPlayback("capture.rpcap");

    Expect(opened,
           "playback file should open: opened_path=" + storage.opened_path +
           ", replay_started=" + std::string(udp.replay_started ? "true" : "false") +
           ", state=" + std::string(recplay::SessionStateToString(controller.GetState())));
    Expect(controller.GetState() == SessionState::Stopped, "open should leave controller in Stopped pre-play state");
    Expect(storage.opened_path == "capture.rpcap", "storage should receive playback path");
    Expect(udp.replay_started, "protocol replay should start");
    Expect(!tcp.replay_started, "unrelated protocols should not be started for playback");
    Expect(controller.GetDuration() == 200, "duration should come from header");

    controller.Play(2.0);
    Expect(controller.GetState() == SessionState::Playing, "state should move to Playing");
    Expect(controller.GetCurrentSpeed() == 2.0, "speed should update");

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    Expect(!udp.replayed_packets.empty(), "scheduler should dispatch playback packets");

    controller.Pause();
    Expect(controller.GetState() == SessionState::PlaybackPaused, "state should pause playback");

    controller.SeekTo(40);
    Expect(storage.last_seek_ns == 40, "seek should delegate to storage");
    Expect(controller.GetCurrentPosition() == 40, "seek should update current position");

    controller.Stop();
    Expect(controller.GetState() == SessionState::Stopped, "stop should end playback");
    Expect(storage.closed, "storage should close playback file");
    Expect(!udp.replay_started, "protocol replay should stop");
}

void TestPlaybackQueueIsRefilledIncrementally() {
    CoreEngine engine;
    FakeStorageService storage;
    FakeProtocolService udp("UDP");

    storage.playback_duration_ns = 500;
    storage.playback_packets = {
        MakePacket(10, 3),
        MakePacket(20, 3),
        MakePacket(30, 3),
        MakePacket(40, 3),
        MakePacket(50, 3)
    };

    engine.SetStorageService(&storage);
    engine.AttachProtocol("UDP", &udp);

    SessionController controller(&engine);
    const bool opened = controller.OpenForPlayback("capture.rpcap");

    Expect(opened, "playback file should open for incremental queue test");
    Expect(storage.read_index < storage.playback_packets.size(),
           "open should not drain entire playback file into scheduler");
}

void TestPlaybackCanInferUdpProtocolWhenSchemaIsEmpty() {
    CoreEngine engine;
    FakeStorageService storage;
    FakeProtocolService udp("UDP");
    FakeProtocolService tcp("TCP");

    storage.playback_duration_ns = 200;
    storage.playback_packets = {MakePacket(10, 3), MakePacket(40, 3), MakePacket(90, 3)};
    storage.playback_channels.clear();

    engine.SetStorageService(&storage);
    engine.AttachProtocol("UDP", &udp);
    engine.AttachProtocol("TCP", &tcp);

    SessionController controller(&engine);
    const bool opened = controller.OpenForPlayback(
        "capture.rpcap",
        R"({"address":"239.1.1.1","port":5000,"interface":"0.0.0.0"})");

    Expect(opened, "playback should open when schema is empty but replay config identifies UDP");
    Expect(udp.replay_started, "UDP replay should be inferred from replay config");
    Expect(!tcp.replay_started, "TCP replay should stay stopped when replay config identifies UDP");

    controller.Stop();
}

void TestLoopPlaybackReturnsToLoopStart() {
    CoreEngine engine;
    FakeStorageService storage;
    FakeProtocolService udp("UDP");

    storage.playback_duration_ns = 500;
    storage.playback_packets = {
        MakePacket(10, 3),
        MakePacket(40, 3),
        MakePacket(70, 3),
        MakePacket(110, 3),
        MakePacket(150, 3)
    };

    engine.SetStorageService(&storage);
    engine.AttachProtocol("UDP", &udp);

    SessionController controller(&engine);
    Expect(controller.OpenForPlayback("capture.rpcap"), "playback file should open for loop test");

    controller.SetLoopRange(40, 80);
    controller.SeekTo(40);
    controller.Play(1000.0);

    std::this_thread::sleep_for(std::chrono::milliseconds(120));

    controller.Pause();
    Expect(storage.last_seek_ns == 40, "loop playback should seek back to loop start after reaching loop end");

    controller.Stop();
}

void TestInvalidIdleActionsAreNoOps() {
    SessionController controller;

    Expect(controller.GetState() == SessionState::Idle, "controller should start in Idle");

    controller.PauseRecording();
    Expect(controller.GetState() == SessionState::Idle,
           "PauseRecording from Idle should remain Idle");

    controller.ResumeRecording();
    Expect(controller.GetState() == SessionState::Idle,
           "ResumeRecording from Idle should remain Idle");

    controller.Pause();
    Expect(controller.GetState() == SessionState::Idle,
           "Pause from Idle should remain Idle");

    controller.Stop();
    Expect(controller.GetState() == SessionState::Idle,
           "Stop from Idle should remain Idle");
}

void TestStopDuringSeekingStillCleansUpPlayback() {
    CoreEngine engine;
    FakeStorageService storage;
    FakeProtocolService udp("UDP");

    storage.playback_duration_ns = 200;
    storage.playback_packets = {MakePacket(10, 3), MakePacket(40, 3), MakePacket(90, 3)};
    storage.block_seek = true;

    engine.SetStorageService(&storage);
    engine.AttachProtocol("UDP", &udp);

    SessionController controller(&engine);
    Expect(controller.OpenForPlayback("capture.rpcap"), "playback file should open for seeking stop test");

    controller.Play(1.0);
    Expect(controller.GetState() == SessionState::Playing, "playback should enter Playing before seeking");

    std::thread seek_thread([&controller]() { controller.SeekTo(40); });
    storage.WaitForBlockedSeek();

    controller.Stop();
    storage.ReleaseBlockedSeek();
    seek_thread.join();

    const auto stopped_during_seek = controller.GetState() == SessionState::Stopped;
    if (!stopped_during_seek) {
        controller.Stop();
    }

    Expect(stopped_during_seek,
           "Stop during Seeking should leave playback stopped");
    Expect(storage.closed, "Stop during Seeking should close playback file");
    Expect(!storage.reading, "Stop during Seeking should end storage reading");
    Expect(!udp.replay_started, "Stop during Seeking should stop replay protocols");
}

void TestStopAfterOpenForPlaybackStillCleansUpPlayback() {
    CoreEngine engine;
    FakeStorageService storage;
    FakeProtocolService udp("UDP");

    storage.playback_duration_ns = 200;
    storage.playback_packets = {MakePacket(10, 3), MakePacket(40, 3), MakePacket(90, 3)};

    engine.SetStorageService(&storage);
    engine.AttachProtocol("UDP", &udp);

    SessionController controller(&engine);
    Expect(controller.OpenForPlayback("capture.rpcap"),
           "playback file should open for staged stop test");
    Expect(controller.GetState() == SessionState::Stopped,
           "OpenForPlayback should leave controller in staged Stopped state");
    Expect(storage.reading, "OpenForPlayback should leave storage reading active");
    Expect(udp.replay_started, "OpenForPlayback should start replay protocols");

    controller.Stop();

    Expect(storage.closed, "Stop after OpenForPlayback should close playback file");
    Expect(!storage.reading, "Stop after OpenForPlayback should end storage reading");
    Expect(!udp.replay_started, "Stop after OpenForPlayback should stop replay protocols");
    Expect(controller.GetState() == SessionState::Stopped,
           "Stop after OpenForPlayback should remain Stopped");
}

void TestOnStateChangedSupportsMultipleCallbacks() {
    CoreEngine engine;
    FakeStorageService storage;
    FakeProtocolService udp("UDP");
    udp.channels.push_back(ChannelInfo{7, "udp-src", "UDP", "udp/topic", {}});

    engine.SetStorageService(&storage);
    engine.AttachProtocol("UDP", &udp);

    SessionController controller(&engine);
    std::vector<std::pair<SessionState, SessionState>> transitions_a;
    std::vector<std::pair<SessionState, SessionState>> transitions_b;

    controller.OnStateChanged([&](SessionState from, SessionState to) {
        transitions_a.emplace_back(from, to);
    });
    controller.OnStateChanged([&](SessionState from, SessionState to) {
        transitions_b.emplace_back(from, to);
    });

    const bool started = controller.StartRecording(
        R"({"output_path":"capture.rpcap","protocols":["UDP"],"protocol_config":{"port":5000}})");
    Expect(started, "recording should start for callback fanout test");

    controller.StopRecording();

    Expect(transitions_a.size() == 2, "first callback should receive both state transitions");
    Expect(transitions_b.size() == 2, "second callback should receive both state transitions");
    Expect(transitions_a == transitions_b, "all callbacks should observe identical transition sequence");
    Expect(transitions_a[0].first == SessionState::Idle &&
               transitions_a[0].second == SessionState::Recording,
           "first transition should be Idle -> Recording");
    Expect(transitions_a[1].first == SessionState::Recording &&
               transitions_a[1].second == SessionState::Stopped,
           "second transition should be Recording -> Stopped");
}

} // namespace

int main() {
    try {
        TestRecordingLifecycle();
        TestPlaybackLifecycle();
        TestPlaybackQueueIsRefilledIncrementally();
        TestPlaybackCanInferUdpProtocolWhenSchemaIsEmpty();
        TestLoopPlaybackReturnsToLoopStart();
        TestInvalidIdleActionsAreNoOps();
        TestStopDuringSeekingStillCleansUpPlayback();
        TestStopAfterOpenForPlaybackStillCleansUpPlayback();
        TestOnStateChangedSupportsMultipleCallbacks();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
}
