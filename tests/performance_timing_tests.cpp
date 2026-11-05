#include "PerformanceTiming.h"
#include <cassert>
#include <iostream>

int main()
{
    FrameCost cpu;
    cpu.Observe(-1);
    cpu.Observe(NAN);
    assert(cpu.Count() == 0 && cpu.Mean() == 0 && cpu.P95() == 0);
    for (int i = 0; i < 256; i++) cpu.Observe(i);
    assert(cpu.Count() == 128 && cpu.Mean() == 191.5 && cpu.P95() == 248);

    FrameCadence cadence;
    cadence.Observe(NAN, 1000. / 60);
    cadence.Observe(0, 0);
    assert(cadence.Count() == 0);
    for (int i = 0; i <= 128; i++) cadence.Observe(i / 60., 1000. / 60);
    assert(cadence.Count() == 128 && std::abs(cadence.Fps() - 60) < 1e-8);
    assert(std::abs(cadence.P95() - 1000. / 60) < 1e-8 && cadence.LatePercent() == 0);
    cadence.Pause();
    cadence.Observe(100, 1000. / 60); // Hidden duration is not a slow rendered frame.
    assert(cadence.Count() == 128 && std::abs(cadence.Fps() - 60) < 1e-8);
    cadence.Observe(100.05, 1000. / 60);
    assert(cadence.Count() == 128 && cadence.Fps() < 60 && cadence.LatePercent() > 0);
    cadence.Observe(101, 1000. / 30); // A changed limit starts a separate measurement window.
    assert(cadence.Count() == 0 && cadence.Fps() == 0);
    for (int i = 1; i <= 128; i++) cadence.Observe(101 + i / 30., 1000. / 30);
    assert(std::abs(cadence.Fps() - 30) < 1e-8 && cadence.LatePercent() == 0);
    cadence.Observe(0, 1000. / 30); // Clock rollback starts a fresh interval baseline.
    cadence.Observe(1. / 30, 1000. / 30);
    assert(std::abs(cadence.Fps() - 30) < 1e-8);
    assert(sizeof(cadence) < 2048);
    std::cout << "Bounded CPU and successful-output cadence: rolling128, 60/30-Hz sampling, stall detection, "
                 "hidden/resume exclusion, changed limit, invalid clocks: PASS\n";
}
