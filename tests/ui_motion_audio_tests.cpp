#include "ExperienceModel.h"
#include "PerformanceTiming.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <set>

using namespace suite_ui;

int main()
{
    // The value at the same elapsed time is independent of draw frequency.
    for (int fps : {30, 60, 144})
    {
        MotionTable table;
        table.Value(1, 0, 0, 0, true);
        table.Value(1, 1, 1, 0, true);
        int frame = 1;
        for (double t = 1. / fps; t < .1; t += 1. / fps)
            table.Value(1, 1, ++frame, t, true);
        float value = table.Value(1, 1, ++frame, .1, true);
        assert(std::abs(value - Ease(.1f / .16f)) < 1e-6f);
        assert(table.Value(1, 1, ++frame, .2, true) == 1);
    }
    MotionTable table;
    table.Value(1, 0, 0, 0, true);
    table.Value(1, 1, 1, .01, true);
    assert(table.Value(1, 1, 2, .02, false) == 1);
    assert(table.Value(1, 1, 3, .03, true) == 1); // No stale tween after reduced motion.
    assert(table.Value(1, 2, 4, .04, true, true, 0) == 2);
    assert(table.Value(1, 3, 5, .05, true, true, NAN) == 3);
    assert(table.Value(1, 4, 6, .06, true, true, INFINITY) == 4);
    assert(table.Value(1, 5, 7, NAN, true) == 5);
    assert(table.Value(1, NAN, 8, .08, true, false, 0) == 0);
    table.Value(1, 1, 9, 1, true);
    assert(table.Value(1, 2, 10, .5, true) == 2); // Clock rollback cannot carry an old transition.

    ToneCadence gate;
    assert(!gate.Accept(Tone::None, 0) && !gate.Accept(Tone::Click, NAN));
    assert(gate.Accept(Tone::Hover, 0));
    assert(!gate.Accept(Tone::Hover, .09) && gate.Accept(Tone::Hover, .101));
    assert(gate.Accept(Tone::Click, .102));
    assert(!gate.Accept(Tone::Hover, .17) && gate.Accept(Tone::Hover, .203));
    assert(gate.Accept(Tone::Warning, .21));
    assert(gate.Accept(Tone::Click, .3));
    assert(!gate.Accept(Tone::Warning, .4)); // Other actions do not defeat warning cooldown.
    assert(!gate.Accept(Tone::Warning, .5) && gate.Accept(Tone::Warning, .561));
    gate.Reset();
    assert(gate.Accept(Tone::Click, 0));
    assert(!gate.Accept(Tone::Click, .02) && gate.Accept(Tone::Click, .036));
    assert(gate.Accept(Tone::Confirm, .037)); // Meaningful new events are not held by click debounce.
    assert(gate.Accept(Tone::Hover, 0)); // Clock reset also resets the cooldown baseline.

    const Tone tones[] = {Tone::Hover, Tone::Click, Tone::Tab, Tone::On, Tone::Off, Tone::Warning,
                          Tone::Confirm, Tone::Complete, Tone::PersonalBest, Tone::InspectorOpen,
                          Tone::InspectorSelect, Tone::InspectorPin, Tone::InspectorClose};
    std::set<std::vector<int16_t>> distinct;
    int hoverPeak = 0, clickPeak = 0;
    for (Tone tone : tones)
    {
        for (int rate : {8000, 44100, 192000})
        {
            auto full = ToneSamples(tone, 1, rate), half = ToneSamples(tone, .5f, rate);
            assert(full.size() > 2 && full.size() <= size_t(rate / 5 + 2));
            assert(full.front() == 0 && full.back() == 0 && full.size() == half.size());
            int peak = 0;
            double mean = 0;
            for (size_t i = 0; i < full.size(); i++)
            {
                peak = std::max(peak, std::abs(int(full[i])));
                mean += full[i];
                assert(std::abs(int(full[i]) - 2 * int(half[i])) <= 4);
                if (i)
                    assert(std::abs(int(full[i]) - int(full[i - 1])) < 9000); // No hard waveform edge.
            }
            assert(peak > 300 && peak < 6000 && std::abs(mean / full.size()) < 50);
            if (rate == 44100)
            {
                assert(distinct.insert(full).second);
                if (tone == Tone::Hover) hoverPeak = peak;
                if (tone == Tone::Click) clickPeak = peak;
            }
        }
    }
    assert(hoverPeak < clickPeak * .45);
    assert(ToneSamples(Tone(1000), 1).empty() && ToneSamples(Tone::Click, 1, 7999).empty());

    // A full animation table at 60 Hz. Report CPU work; this does not claim GPU
    // or monitor performance on Windows. No allocations occur inside Value.
    MotionTable loaded;
    FrameCost cost;
    double checksum = 0;
    for (int frame = 0; frame < 600; frame++)
    {
        auto begin = std::chrono::steady_clock::now();
        for (uint32_t widget = 0; widget < MotionTable::Capacity(); widget++)
            checksum += loaded.Value(widget, frame % 60 < 30 ? 1.f : 0.f, frame, frame / 60., true);
        cost.Observe(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count());
    }
    assert(std::isfinite(checksum) && checksum > 0 && sizeof(loaded) < 32768);
    std::cout << "512 animated widgets / 600 simulated 60-Hz frames: mean " << cost.Mean()
              << " ms, p95 " << cost.P95() << " ms CPU/frame (host; not displayed FPS)\n";
    std::cout << "Motion: cadence-independent, reversible, reduced-motion resume, finite/zero-duration guards; "
                 "audio: distinct gain-scaled original cues, quiet hover, bounded cooldowns: PASS\n";
}
