#include "ReplayHealth.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace practice;

static std::vector<uint8_t> Read(const std::filesystem::path &p)
{
    std::ifstream f(p, std::ios::binary);
    assert(f);
    return {std::istreambuf_iterator<char>(f), {}};
}

static ReplayMotion Complete()
{
    ReplayMotion m;
    m.id = 790;
    m.scene = 5;
    m.seed = 42;
    m.seconds = 1;
    m.inputFormat = 6;
    m.coverage.recorded = true;
    for (auto &s : m.coverage.spans)
    {
        s.started = true;
        s.observations = 101;
        s.sources = 1;
        s.first = 0;
        s.through = 1;
        s.maxGap = .01;
    }
    for (int n = 0; n <= 50; n++)
    {
        ReplayPhysicsFrame f;
        f.seconds = n / 50.;
        m.ticks.push_back(f);
    }
    for (int n = 0; n <= 120; n++)
    {
        ReplayViewFrame f;
        f.seconds = n / 120.;
        m.frames.push_back(f);
    }
    m.devices.push_back({});
    m.deviceThrough = 1;
    m.colliders.recorded = true;
    m.colliders.count = 1;
    ReplayColliderTrack shape;
    shape.samples.push_back({});
    m.colliders.tracks.push_back(shape);
    m.visualsRecorded = true;
    m.visualSamples = 2;
    m.visualTracks = 1;
    m.experience.recorded = true;
    ReplayAnimationTrack animation;
    animation.frames.push_back({});
    m.experience.animations.push_back(animation);
    m.experience.audio.recorded = true;
    m.experience.audio.rate = 48000;
    m.experience.audio.channels = 2;
    m.experience.audio.frames = 48000;
    m.audioFile = "verified-by-archive.isaudio";
    return m;
}

int main(int argc, char **argv)
{
    auto good = Complete();
    auto check = BuildRecordingCheck(good);
    assert(check->complete == 6 && check->partial == 0 && check->missing == 0 && check->unknown == 0);
    // Sparse unchanged inputs, constant collider shapes and poses remain complete
    // because successful observation heartbeats are independent of file changes.
    assert(check->channels[2].samples == 1 && check->channels[3].samples == 1 &&
           check->channels[4].samples == 2);
    auto legacy = Complete();
    legacy.coverage = {};
    check = BuildRecordingCheck(legacy);
    assert(check->unknown == 6 && check->complete == 0);
    auto gap = Complete();
    auto &span = gap.coverage.spans[0];
    span.gapCount = 1;
    span.maxGap = .4;
    span.gaps = {{.2, .6}};
    gap.ticks.erase(gap.ticks.begin() + 11, gap.ticks.begin() + 30);
    check = BuildRecordingCheck(gap);
    assert(check->channels[0].status == RecordingStatus::Partial && check->channels[0].gaps.size() == 1 &&
           check->channels[0].gaps[0].begin == .2);
    auto stopped = Complete();
    stopped.coverage.spans[1].through = .5;
    check = BuildRecordingCheck(stopped);
    assert(check->channels[1].status == RecordingStatus::Partial && check->channels[1].gaps.back().end == 1);
    auto error = Complete();
    error.coverage.spans[4].errors = 1;
    error.deviceDropped = 3;
    error.colliders.flags = 1;
    error.visualDropped = 2;
    check = BuildRecordingCheck(error);
    assert(check->partial == 3 && check->complete == 3);
    auto missing = Complete();
    missing.ticks.clear();
    missing.frames.clear();
    missing.devices.clear();
    missing.colliders = {};
    missing.visualSamples = 0;
    missing.visualDropped = 1;
    missing.experience.audio = {};
    check = BuildRecordingCheck(missing);
    assert(check->missing == 6 && check->partial == 0);
    auto unavailable = Complete();
    unavailable.audioFile.clear();
    check = BuildRecordingCheck(unavailable);
    assert(check->channels[5].status == RecordingStatus::Unknown);
    unavailable.audioError = "Audio checksum mismatch";
    unavailable.experience.audio.flags = 4;
    check = BuildRecordingCheck(unavailable);
    assert(check->channels[5].status == RecordingStatus::Missing);
    auto partialAudio = Complete();
    partialAudio.experience.audio.flags = 1 | 4;
    partialAudio.experience.audio.frames = 24000;
    partialAudio.audioNotes = "Begin: listener bypass with a spatial mix or source filter";
    check = BuildRecordingCheck(partialAudio);
    assert(check->channels[5].status == RecordingStatus::Partial && check->channels[5].gaps[0].begin == .5);
    assert(check->channels[5].detail.find("Begin: listener bypass") != std::string::npos &&
           check->channels[5].detail.find("No reported gaps") == std::string::npos);
    auto gapOnly = Complete();
    gapOnly.experience.audio.gaps = 1;
    check = BuildRecordingCheck(gapOnly);
    assert(check->channels[5].status == RecordingStatus::Partial);
    auto optional = Complete();
    optional.coverage.spans[5].sources = 0;
    optional.experience.animations.clear();
    check = BuildRecordingCheck(optional);
    assert(check->channels[4].status == RecordingStatus::Complete);
    auto emptyAnimation = Complete();
    emptyAnimation.experience.animations[0].frames.clear();
    check = BuildRecordingCheck(emptyAnimation);
    assert(check->channels[4].status == RecordingStatus::Partial);
    auto a = RecordingCheckFor(good), b = RecordingCheckFor(good);
    assert(a == b && a->complete == 6);
    auto report = RecordingCheckReport(good, *a);
    assert(report.find("Movement: Complete") != std::string::npos &&
           report.find("separate from run validity") != std::string::npos);
    if (argc == 3)
    {
        std::string reason;
        auto bytes = Read(std::filesystem::path(argv[1]) / "input-v6.isinput");
        auto motion = DecodeMotion(bytes, reason);
        assert(motion && motion->inputFormat == 6 && motion->coverage.recorded &&
               motion->coverage.spans[0].through == motion->seconds);
        auto v7 = bytes;
        v7.resize(v7.size() - 8);
        v7[4] = 7;
        std::string note = "Begin: listener bypass with a spatial mix or source filter\n";
        uint32_t length = uint32_t(note.size());
        auto atNote = v7.size();
        v7.resize(v7.size() + 4);
        std::memcpy(v7.data() + atNote, &length, 4);
        v7.insert(v7.end(), note.begin(), note.end());
        auto hash7 = ReplayHash(v7, v7.size());
        v7.resize(v7.size() + 8);
        std::memcpy(v7.data() + v7.size() - 8, &hash7, 8);
        auto newMotion = DecodeMotion(v7, reason);
        assert(newMotion && newMotion->inputFormat == 7 && newMotion->audioNotes == note &&
               newMotion->coverage.recorded);
        length = 4097;
        std::memcpy(v7.data() + atNote, &length, 4);
        hash7 = ReplayHash(v7, v7.size() - 8);
        std::memcpy(v7.data() + v7.size() - 8, &hash7, 8);
        assert(!DecodeMotion(v7, reason));
        // Corrupt the v6 coverage, then recompute the outer checksum: the new
        // structural checks, not only the existing file hash, must reject it.
        constexpr size_t coverageBytes = 5 + 6 * 45;
        size_t at = bytes.size() - 8 - coverageBytes;
        assert(bytes[at] == 1 && bytes[at + 1] == 6);
        bytes[at] = 2;
        auto hash = ReplayHash(bytes, bytes.size() - 8);
        std::memcpy(bytes.data() + bytes.size() - 8, &hash, 8);
        assert(!DecodeMotion(bytes, reason));
        auto old = DecodeMotion(Read(argv[2]), reason);
        assert(old && old->inputFormat == 5 && !old->coverage.recorded);
        auto oldCheck = BuildRecordingCheck(*old);
        assert(oldCheck->channels[0].status != RecordingStatus::Complete);
    }
    std::cout
        << "Production recording check: complete/partial/missing/historical tracks, constant-value coverage, merged exact gap times, metadata errors, audio identity failures/route loss, cached report, cross-language v6 and legacy v5: PASS\n";
}
