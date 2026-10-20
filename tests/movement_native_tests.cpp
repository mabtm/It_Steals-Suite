#include "RecordedTiming.h"
#include "MovementTelemetry.h"
#include "seed_map_fixture.h"
#include <fstream>
#include <iostream>
#include <random>
#include <chrono>
#include <cassert>
using namespace practice;

int main(int argc, char **argv)
{
    assert(argc == 2);
    std::ifstream input(argv[1], std::ios::binary);
    std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    std::string error;
    auto wire = DecodeMap(bytes, error);
    if (!wire)
        throw std::runtime_error(error);
    assert(wire->movement && wire->movement->Ready() && wire->movement->counts[2] > 10000);
    auto roundtrip = DecodeMap(EncodeFixture(*wire), error);
    assert(roundtrip && roundtrip->movement->speeds == wire->movement->speeds &&
           roundtrip->movement->counts == wire->movement->counts &&
           roundtrip->movement->lengths == wire->movement->lengths);
    auto legacy = DecodeMap(EncodeFixture(*wire, 10), error);
    assert(legacy && !legacy->movement && legacy->review->flashes.size() == 2);
    assert(!std::atomic_load(&learnedMovement[MapLevelIndex(wire->scene)]));
    assert(MovementFor(*wire) == wire->movement); // Frozen cumulative data, no registry write.
    MovementData m = *wire->movement;
    for (int k = 0; k < 5; k++)
    {
        m.counts[k] = 100;
        m.lengths[k] = k ? 1.1f : 1.2f;
        m.speeds[k * 3] = k ? 8.0f : 9.6f;
        m.speeds[k * 3 + 1] = k ? 8.5f : 9.85f;
        m.speeds[k * 3 + 2] = k ? 9.f : 9.9f;
    }
    auto straight = EstimateMovementRoute(m, *wire, {{0, 0, 0}, {100, 0, 0}}, true),
         bend = EstimateMovementRoute(m, *wire, {{0, 0, 0}, {50, 0, 0}, {50, 0, 50}}, true);
    assert(straight.valid && bend.valid && bend.seconds > straight.seconds && straight.coverage == 1 &&
           straight.fast <= straight.seconds && straight.slow >= straight.seconds);
    assert(straight.Fraction(0) == 0 && straight.Fraction(straight.seconds) == 1 &&
           straight.Fraction(straight.seconds * .5) > .4);
    assert(std::abs(straight.seconds - (1.1 / 8.5 + 98.9 / 9.85)) < 1e-6);
    std::vector<Vec3> subdivided;
    for (int i = 0; i <= 1000; i++)
        subdivided.push_back({i * .1f, 0, 0});
    auto dense = EstimateMovementRoute(m, *wire, subdivided, true);
    assert(std::abs(dense.seconds - straight.seconds) < 1e-6 &&
           std::abs(dense.length - 100) < 1e-6); // Starting loss spans arbitrarily short first legs.
    auto duplicates = subdivided;
    duplicates.insert(duplicates.begin() + 1, duplicates.front());
    assert(std::abs(EstimateMovementRoute(m, *wire, duplicates, true).seconds - straight.seconds) < 1e-6);
    auto slope = EstimateMovementRoute(m, *wire, {{0, 0, 0}, {100, 50, 0}}, true);
    assert(slope.valid && slope.seconds > straight.seconds && slope.length > 111);
    auto unknown = m;
    unknown.counts[2] = 0;
    unknown.lengths[2] = 0;
    for (int q = 0; q < 3; q++)
        unknown.speeds[6 + q] = 0;
    auto uncertain = EstimateMovementRoute(unknown, *wire, {{0, 0, 0}, {50, 0, 0}, {50, 0, 50}}, true);
    assert(uncertain.valid && uncertain.coverage < 1 &&
           uncertain.slow - uncertain.fast >
               bend.slow - bend.fast); // No pretend sharp-turn data borrowed from gentle bends.
    assert(!EstimateMovementRoute(m, *wire, {{0, 0, 0}, {10, 0, 0}}, false).valid);
    auto wrong = *wire;
    wrong.hash = std::string(64, 'b');
    assert(!MovementMatches(m, wrong));
    wrong = *wire;
    wrong.scene = 13;
    assert(!MovementMatches(m, wrong));
    wrong = *wire;
    wrong.agentRadius = .4f;
    assert(!MovementMatches(m, wrong));
    wrong = *wire;
    wrong.agentHeight = 2;
    assert(!MovementMatches(m, wrong));
    wrong = *wire;
    wrong.movementTick = .025f;
    assert(!MovementMatches(m, wrong));
    wrong = *wire;
    wrong.movementTick = 0;
    assert(!MovementMatches(m, wrong));
    auto insufficient = m;
    insufficient.counts[0] = 23;
    assert(!MovementMatches(insufficient, *wire));
    auto invalid = m;
    invalid.speeds[3] = NAN;
    assert(!ValidMovement(invalid));
    invalid = m;
    invalid.counts[1] = 20001;
    assert(!ValidMovement(invalid));
    invalid = m;
    invalid.speeds[0] = 10;
    assert(!ValidMovement(invalid));
    GameData snapshot{};
    snapshot.movementScene = m.scene;
    snapshot.movementRevision = m.revision + 1;
    snapshot.movementTick = m.tick;
    snapshot.movementRadius = m.radius;
    snapshot.movementHeight = m.height;
    std::memcpy(snapshot.gameAssemblyHash, m.hash.c_str(), 64);
    std::copy(m.counts.begin(), m.counts.end(), snapshot.movementCounts);
    std::copy(m.speeds.begin(), m.speeds.end(), snapshot.movementSpeeds);
    std::copy(m.lengths.begin(), m.lengths.end(), snapshot.movementLengths);
    ObserveMovementSnapshot(snapshot);
    auto live = MovementFor(*wire);
    assert(live && live != wire->movement && live->revision == m.revision + 1);
    ObserveMovementSnapshot(snapshot);
    assert(MovementFor(*wire) == live);
    snapshot.movementSpeeds[0] = NAN;
    snapshot.movementRevision++;
    ObserveMovementSnapshot(snapshot);
    assert(MovementFor(*wire) == live);
    auto bad = *wire;
    auto malformed = std::make_shared<MovementData>(m);
    malformed->scene = 13;
    bad.movement = malformed;
    assert(!DecodeMap(EncodeFixture(bad), error));
    malformed->scene = wire->scene;
    malformed->tick = .025f;
    assert(!DecodeMap(EncodeFixture(bad), error));
    std::mt19937 random(697);
    for (int trial = 0; trial < 2000; trial++)
    {
        std::vector<Vec3> route{{0, 0, 0}};
        for (int i = 0; i < 20; i++)
        {
            auto a = route.back();
            route.push_back({a.x + float(int(random() % 20) - 10), a.y + float(int(random() % 5) - 2),
                             a.z + float(int(random() % 20) - 10)});
        }
        auto t = EstimateMovementRoute(m, *wire, route, true);
        assert(t.valid && std::isfinite(t.seconds) && t.fast <= t.seconds && t.slow >= t.seconds &&
               t.coverage == 1);
    }
    auto start = std::chrono::steady_clock::now();
    double checksum = 0;
    for (int i = 0; i < 20000; i++)
        checksum += EstimateMovementRoute(m, *wire, {{0, 0, 0}, {50, 0, 0}, {50, 0, 50}}, true).seconds;
    assert(checksum > 0);
    std::cout << "20,000 learned forecasts: "
              << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count()
              << " ms (sanitizers; host CPU)\n";
    std::cout
        << "C#/C++ v11 wire parity, v10 preservation, independent live/frozen profiles, five classes, uncertainty, physics/input gates, subdivision-invariant starting distance, NaN/context checks and 2000 route fuzz cases: PASS\n";
}
