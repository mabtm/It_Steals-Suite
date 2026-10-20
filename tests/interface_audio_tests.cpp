#include "InterfaceAudio.h"
#include <iostream>
#include <cassert>

template <class Condition> void Await(Condition done)
{
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!done())
    {
        if (std::chrono::steady_clock::now() > deadline)
            throw std::runtime_error("Audio adapter timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

int main()
{
    {
        InterfaceAudio audio;
        audio.Submit(suite_ui::Tone::Click);
        assert(fakeWrites == 0 && audio.Status() == 0);
        audio.Policy(true, .16f);
        Await([&] { return audio.Status() == 1; });
        audio.Submit(suite_ui::Tone::Click);
        Await([] { return fakeWrites == 1 && fakePrepared == 0; });
        {
            std::lock_guard lock(fakeAudioGate);
            int peak = 0;
            for (auto sample : fakeLastSamples)
                peak = std::max(peak, std::abs(int(sample)));
            assert(peak > 100 && peak < 1000);
        }
        audio.Submit(suite_ui::Tone::InspectorSelect, .5f);
        Await([] { return fakeWrites == 2 && fakePrepared == 0; });
        {
            std::lock_guard lock(fakeAudioGate);
            assert(fakeLastSamples == suite_ui::ToneSamples(suite_ui::Tone::InspectorSelect, .16f * .5f));
        }
        audio.Submit(suite_ui::Tone::InspectorSelect, NAN);
        audio.Submit(suite_ui::Tone::InspectorSelect, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        assert(fakeWrites == 2);
        fakeHold = true;
        int old = fakeWrites;
        audio.Submit(suite_ui::Tone::Confirm);
        Await([&] { return fakeWrites == old + 1 && fakePrepared == 1; });
        int resets = fakeResets;
        audio.Policy(false, .16f);
        Await([&] { return fakePrepared == 0 && fakeResets > resets; });
        audio.Submit(suite_ui::Tone::Click);
        assert(fakeWrites == old + 1);
        audio.Policy(true, .1f);
        audio.Submit(suite_ui::Tone::Tab);
        Await([&] { return fakeWrites == old + 2 && fakePrepared == 1; });
        for (int i = 0; i < 100; i++)
            audio.Submit(suite_ui::Tone::Click);
        audio.Policy(false, .1f);
        Await([] { return fakePrepared == 0; });
        int muted = fakeWrites;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        assert(fakeWrites == muted);
        fakeHold = false;
        audio.Policy(true, .1f);
        audio.Submit(suite_ui::Tone::Complete);
        Await([&] { return fakeWrites == muted + 1 && fakePrepared == 0; });
        audio.Submit(suite_ui::Tone::PersonalBest);
        Await([&] { return fakeWrites == muted + 2 && fakePrepared == 0; });
        audio.Shutdown();
        audio.Shutdown();
        assert(fakePrepared == 0 && fakeHandles == 0 && fakeOpens == fakeCloses);
    }
    fakeFailOpen = true;
    {
        InterfaceAudio audio;
        audio.Policy(true, .2f);
        Await([&] { return audio.Status() == 2; });
        int count = fakeWrites;
        audio.Submit(suite_ui::Tone::Confirm);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        assert(fakeWrites == count);
        audio.Shutdown();
    }
    fakeFailOpen = false;
    for (bool prepare : {false, true})
    {
        InterfaceAudio audio;
        fakeFailPrepare = prepare;
        fakeFailWrite = !prepare;
        audio.Policy(true, .16f);
        Await([&] { return audio.Status() == 1; });
        audio.Submit(suite_ui::Tone::Click);
        Await([&] { return audio.Status() == 2; });
        audio.Shutdown();
        assert(fakePrepared == 0 && fakeHandles == 0);
    }
    fakeFailPrepare = fakeFailWrite = false;
    assert(fakeOpens == fakeCloses);
    std::cout
        << "Production audio worker: asynchronous original PCM, private sample gain, focus/run mute cancellation, bounded pending bursts, interrupted buffer lifetime, device/prepare/write failures, event-driven waits and idempotent shutdown: PASS\n";
}
