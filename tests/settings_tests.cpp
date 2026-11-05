// Executes the real settings worker/recovery logic with filesystem-backed
// Win32 file-operation adapters. This does not emulate Windows focus or D3D.
#include <filesystem>
#include <fstream>
#include <string>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cassert>
#include <iostream>
#include <cwchar>
using DWORD = unsigned long;
constexpr int TRUE = 1, FALSE = 0, MOVEFILE_REPLACE_EXISTING = 1, MOVEFILE_WRITE_THROUGH = 8;
static std::filesystem::path appData;
static std::mutex fileGate;
static std::condition_variable fileWake;
static bool blockWrites = false, failMove = false;

DWORD GetEnvironmentVariableW(const wchar_t *, wchar_t *buffer, DWORD capacity)
{
    auto text = appData.wstring();
    if (text.size() >= capacity)
        return capacity;
    std::wcscpy(buffer, text.c_str());
    return static_cast<DWORD>(text.size());
}

DWORD GetModuleFileNameW(void *, wchar_t *, DWORD)
{
    return 0;
}

unsigned long long GetTickCount64()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

int CopyFileW(const std::filesystem::path::value_type *source,
              const std::filesystem::path::value_type *target, int failIfExists)
{
    std::error_code e;
    std::filesystem::copy_file(std::filesystem::path(source), std::filesystem::path(target),
                               failIfExists ? std::filesystem::copy_options::none
                                            : std::filesystem::copy_options::overwrite_existing,
                               e);
    return !e;
}

int MoveFileExW(const std::filesystem::path::value_type *source,
                const std::filesystem::path::value_type *target, DWORD)
{
    {
        std::unique_lock lock(fileGate);
        fileWake.wait(lock, [] { return !blockWrites; });
        if (failMove)
            return FALSE;
    }
    std::error_code e;
    std::filesystem::rename(std::filesystem::path(source), std::filesystem::path(target), e);
    return !e;
}

#include "TrainerSettings.h"

static std::string Read(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

static void Write(const std::filesystem::path &path, const std::string &bytes)
{
    std::ofstream out(path, std::ios::binary);
    out << bytes;
}

static void Await(TrainerSettings &settings)
{
    auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (settings.Pending() && std::chrono::steady_clock::now() < end)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(!settings.Pending());
    settings.Poll();
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    appData = std::filesystem::absolute(argv[1]);
    std::filesystem::create_directories(appData);
    auto path = appData / "ItStealsPracticeSuite/trainer.ini";
    std::filesystem::create_directories(path.parent_path());
    for (auto &entry : std::filesystem::directory_iterator(path.parent_path()))
        if (entry.is_regular_file())
            std::filesystem::remove(entry);
    assert(TrainerPreferences::Decode("version=18\nseed=11\n").skipGameIntro);
    TrainerPreferences p;
    p.seed = 123;
    p.captureOverlay = true;
    p.replay.libraryLevel = 7; p.replay.libraryCategory = 1; p.replay.libraryStatus = 2;
    p.replay.librarySort = 6; p.replay.libraryAscending = true; p.replay.libraryFavorites = true;
    auto library = TrainerPreferences::Decode(p.Encode()).replay;
    assert(library.libraryLevel == 7 && library.libraryCategory == 1 && library.libraryStatus == 2 &&
        library.librarySort == 6 && library.libraryAscending && library.libraryFavorites);
    p.hotkeys.bindings[suite_keys::Menu] = {'J', suite_keys::Control | suite_keys::Shift};
    p.skipGameIntro = false;
    p.appearance.fontIds = {"Suite:München.ttf#0", "User:custom=face.ttc#2", "Windows:consola.ttf#0"};
    auto customFonts = TrainerPreferences::Decode(p.Encode());
    assert(customFonts.appearance.fontIds == p.appearance.fontIds);
    assert(TrainerPreferences::Decode("appearance.fontId0=bad\rname\n").appearance.fontIds[0].empty());
    p.experience.volume = .34f;
    p.experience.motion = false;
    p.experience.sounds = false;
    p.map.zoom = 2;
    p.planner.seed = 456;
    p.planner.minimapRoute = true;
    {
        TrainerSettings settings;
        assert(settings.Load().seed == 0);
        assert(settings.Save(p));
        Await(settings);
        assert(settings.error.empty());
        p.seed = 124;
        assert(settings.Save(p));
        settings.Shutdown();
    }
    {
        TrainerSettings settings;
        auto loaded = settings.Load();
        assert(loaded.seed == 124 && loaded.captureOverlay && !loaded.skipGameIntro &&
               loaded.hotkeys == p.hotkeys && !loaded.experience.motion && !loaded.experience.sounds &&
               std::abs(loaded.experience.volume - .34f) < 1e-6);
        assert(std::filesystem::exists(path.string() + ".bak"));
    }
    Write(path, std::string(270000, 'X'));
    {
        TrainerSettings settings;
        auto restored = settings.Load();
        assert(restored.seed == 123 && !settings.loadWarning.empty());
        restored.seed = 125;
        assert(settings.Save(restored));
        settings.Shutdown();
        assert(settings.error.empty());
    }
    int preserved = 0;
    for (auto &entry : std::filesystem::directory_iterator(path.parent_path()))
        if (entry.path().string().find(".corrupt-") != std::string::npos)
            preserved++;
    assert(preserved == 1);
    Write(path, "version=999\nseed=777\n");
    {
        TrainerSettings settings;
        settings.Load();
        assert(!settings.Save(p));
        assert(!settings.error.empty());
    }
    assert(Read(path) == "version=999\nseed=777\n");
    Write(path, p.Encode());
    {
        TrainerSettings settings;
        settings.Load();
        {
            std::lock_guard lock(fileGate);
            blockWrites = true;
        }
        for (int i = 0; i < 100; i++)
        {
            p.seed = i;
            assert(settings.Save(p));
        }
        {
            std::lock_guard lock(fileGate);
            blockWrites = false;
            fileWake.notify_all();
        }
        settings.Shutdown();
        assert(settings.error.empty());
    }
    {
        TrainerSettings settings;
        assert(settings.Load().seed == 99);
    }
    {
        TrainerSettings settings;
        settings.Load();
        {
            std::lock_guard lock(fileGate);
            failMove = true;
        }
        p.seed = 500;
        settings.Save(p);
        Await(settings);
        assert(!settings.error.empty());
        {
            std::lock_guard lock(fileGate);
            failMove = false;
        }
        assert(settings.Save(p));
        Await(settings);
        assert(settings.error.empty());
        settings.Shutdown();
    }
    {
        TrainerSettings settings;
        assert(settings.Load().seed == 500);
    }
    std::cout
        << "Real settings worker, coalescing, backup recovery, future-version protection and failed-commit retry: PASS\n";
}
