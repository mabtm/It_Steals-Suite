#pragma once
#include <windows.h>
#include <mmsystem.h>
#include "ExperienceModel.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <deque>
#include <chrono>

// Event-driven private audio worker. It never changes Windows/game volume.
// Driver buffer ownership follows Microsoft's CALLBACK_EVENT / WHDR_DONE rules:
// https://learn.microsoft.com/windows/win32/multimedia/using-an-callback-to-process-driver-messages
class InterfaceAudio
{
    using Clock = std::chrono::steady_clock;

    struct Request
    {
        suite_ui::Tone tone;
        Clock::time_point at;
        uint64_t epoch;
        float gain;
    };

    HANDLE wake = nullptr;
    std::thread worker;
    std::mutex gate;
    std::deque<Request> pending;
    suite_ui::ToneCadence cadence;
    bool stopping = false, allowed = false;
    float volume = .16f;
    uint64_t epoch = 0;
    std::atomic<int> status{0};

    void Work()
    {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        HANDLE done = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        HWAVEOUT output = nullptr;
        if (!done)
            status = 2;
        auto close = [&] {
            if (output)
            {
                waveOutReset(output);
                waveOutClose(output);
                output = nullptr;
            }
            if (done)
                CloseHandle(done);
        };
        try
        {
            while (true)
            {
                Request request{};
                float gain = 0;
                bool have = false, canOpen = false;
                {
                    std::lock_guard lock(gate);
                    if (stopping)
                        break;
                    canOpen = allowed;
                    if (allowed && !pending.empty())
                    {
                        request = pending.front();
                        pending.pop_front();
                        gain = volume;
                        have = true;
                    }
                }
                if (!canOpen && output)
                {
                    waveOutReset(output);
                    waveOutClose(output);
                    output = nullptr;
                    status = 0;
                }
                if (canOpen && !output && done)
                {
                    WAVEFORMATEX format{};
                    format.wFormatTag = WAVE_FORMAT_PCM;
                    format.nChannels = 1;
                    format.nSamplesPerSec = 44100;
                    format.wBitsPerSample = 16;
                    format.nBlockAlign = 2;
                    format.nAvgBytesPerSec = 88200;
                    status = waveOutOpen(&output, WAVE_MAPPER, &format, reinterpret_cast<DWORD_PTR>(done), 0,
                                         CALLBACK_EVENT) == MMSYSERR_NOERROR
                                 ? 1
                                 : 2;
                }
                if (!have)
                {
                    WaitForSingleObject(wake, INFINITE);
                    continue;
                }
                bool current;
                {
                    std::lock_guard lock(gate);
                    current = allowed && !stopping && request.epoch == epoch;
                }
                if (!current || !output || Clock::now() - request.at > std::chrono::milliseconds(180))
                    continue;
                auto samples = suite_ui::ToneSamples(request.tone, gain * request.gain);
                if (samples.empty())
                    continue;
                WAVEHDR header{};
                header.lpData = reinterpret_cast<LPSTR>(samples.data());
                header.dwBufferLength = DWORD(samples.size() * sizeof(int16_t));
                ResetEvent(done);
                if (waveOutPrepareHeader(output, &header, sizeof(header)) != MMSYSERR_NOERROR)
                {
                    status = 2;
                    continue;
                }
                if (waveOutWrite(output, &header, sizeof(header)) == MMSYSERR_NOERROR)
                {
                    HANDLE events[] = {done, wake};
                    auto deadline = Clock::now() + std::chrono::milliseconds(500);
                    while (!(header.dwFlags & WHDR_DONE))
                    {
                        {
                            std::lock_guard lock(gate);
                            current = allowed && !stopping && request.epoch == epoch;
                        }
                        if (!current || Clock::now() >= deadline)
                        {
                            waveOutReset(output);
                            break;
                        }
                        WaitForMultipleObjects(2, events, FALSE, 50);
                    }
                }
                else
                {
                    status = 2;
                    waveOutReset(output);
                }
                // Reset synchronously releases all queued buffers before the
                // memory may be destroyed, including cancellation/error paths.
                if (!(header.dwFlags & WHDR_DONE))
                    waveOutReset(output);
                if (waveOutUnprepareHeader(output, &header, sizeof(header)) != MMSYSERR_NOERROR)
                {
                    waveOutReset(output);
                    waveOutUnprepareHeader(output, &header, sizeof(header));
                    status = 2;
                }
            }
        }
        catch (...)
        {
            status = 2;
        }
        close();
    }

  public:
    InterfaceAudio() = default;
    InterfaceAudio(const InterfaceAudio &) = delete;

    ~InterfaceAudio()
    {
        Shutdown();
    }

    int Status() const
    {
        return status.load();
    }

    void Policy(bool enabled, float gain)
    {
        std::lock_guard lock(gate);
        gain = std::isfinite(gain) ? std::clamp(gain, 0.f, 1.f) : 0;
        enabled = enabled && gain > 0;
        if (stopping)
            return;
        bool changed = allowed != enabled || gain != volume;
        allowed = enabled;
        volume = gain;
        if (changed)
        {
            epoch++;
            pending.clear();
            cadence.Reset();
        }
        if (enabled && !worker.joinable())
        {
            if (!wake)
                wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            if (!wake)
            {
                status = 2;
                allowed = false;
                return;
            }
            try
            {
                worker = std::thread([this] { Work(); });
            }
            catch (...)
            {
                status = 2;
                allowed = false;
            }
        }
        if (changed && wake)
            SetEvent(wake);
    }

    void Submit(suite_ui::Tone tone, float gain = 1)
    {
        std::lock_guard lock(gate);
        if (!allowed || stopping || tone == suite_ui::Tone::None || !worker.joinable() ||
            !std::isfinite(gain) || gain <= 0)
            return;
        int priority = suite_ui::ToneImportance(tone);
        if (std::any_of(pending.begin(), pending.end(), [&](const Request &r) {
                return suite_ui::ToneImportance(r.tone) > priority;
            }))
            return;
        auto now = Clock::now();
        if (!cadence.Accept(tone, std::chrono::duration<double>(now.time_since_epoch()).count()))
            return;
        // A click/confirmation replaces queued decorative cues, so fast pointer
        // movement cannot delay a useful action sound behind a hover backlog.
        pending.erase(std::remove_if(pending.begin(), pending.end(), [&](const Request &r) {
                          return suite_ui::ToneImportance(r.tone) < priority;
                      }), pending.end());
        if (pending.size() >= 2)
            pending.pop_front();
        pending.push_back({tone, now, epoch, std::clamp(gain, 0.f, 1.f)});
        SetEvent(wake);
    }

    void Shutdown()
    {
        {
            std::lock_guard lock(gate);
            if (stopping)
                return;
            stopping = true;
            allowed = false;
            epoch++;
            pending.clear();
            if (wake)
                SetEvent(wake);
        }
        if (worker.joinable())
            worker.join();
        if (wake)
        {
            CloseHandle(wake);
            wake = nullptr;
        }
    }
};
