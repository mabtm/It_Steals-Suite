#include "../Trainer/StartupLogos.h"
#include <cassert>
#include <iostream>

namespace fs = std::filesystem;
using namespace game_launch;

static void Write(const fs::path &path, const std::vector<uint8_t> &bytes)
{
    fs::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
}

static std::vector<uint8_t> Read(const fs::path &path)
{
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), {}};
}

int main(int argc, char **argv)
{
    assert(argc == 2 || argc == 3);
    const auto root = fs::absolute(argv[1]) / "startup-logos";
    const auto game = root / "It Steals.exe", file = root / "It Steals_Data/globalgamemanagers";
    auto backup = file;
    backup += ".practicesuite-original";
    std::vector<uint8_t> original(256);
    for (size_t i = 0; i < original.size(); i++)
        original[i] = uint8_t(i);
    original[41] = 1;
    original[205] = 0;
    SplashProfile fixture{original.size(), 41, 205, practice::ReplaySha256(original)};
    Write(file, original);
    StartupLogos logos(fixture);
    assert(logos.Prepare(game, false) && !logos.Pending() && !fs::exists(backup));
    assert(logos.Prepare(game, true) && logos.Pending());
    auto patched = Read(file);
    size_t changed = 0;
    for (size_t i = 0; i < original.size(); i++)
        changed += original[i] != patched[i];
    assert(changed == 2 && patched[41] == 0 && patched[205] == 1);
    assert(Read(backup) == original);
    assert(logos.Restore() && !logos.Pending() && Read(file) == original && !fs::exists(backup));
    assert(logos.Prepare(game, true));
    fs::remove(file);
    assert(logos.Restore() && Read(file) == original && !fs::exists(backup));

    assert(logos.Prepare(game, true)); // Interrupted launch, then restart with skip disabled.
    StartupLogos restarted(fixture);
    assert(restarted.Prepare(game, false) && Read(file) == original && !fs::exists(backup));
    assert(logos.Prepare(game, true));
    patched[5] ^= 1;
    Write(file, patched);
    assert(!logos.Restore() && Read(file) == patched && Read(backup) == original);
    Write(file, original);
    assert(logos.Restore() && !fs::exists(backup));
    auto unknown = original;
    unknown[7] ^= 1;
    Write(file, unknown);
    assert(!logos.Prepare(game, true) && Read(file) == unknown && !fs::exists(backup));
    Write(file, {1, 2, 3});
    assert(!logos.Prepare(game, true) && !fs::exists(backup));
    fs::remove(file);
    assert(!logos.Prepare(game, true));
    Write(file, original);
    auto temporary = file;
    temporary += ".practicesuite-tmp";
    fs::create_directory(temporary); // A failed write must preserve both originals.
    assert(!logos.Prepare(game, true) && Read(file) == original && Read(backup) == original);
    fs::remove_all(temporary);
    assert(logos.Restore() && Read(file) == original && !fs::exists(backup));
    Write(backup, unknown);
    assert(!logos.Restore() && Read(file) == original && Read(backup) == unknown);
    fs::remove(backup);

    if (argc == 3)
    {
        // Optional local verification against the supplied SDK; game assets stay out of Git.
        const auto actual = Read(argv[2]);
        Write(file, actual);
        StartupLogos suppliedGame;
        assert(suppliedGame.Prepare(game, true));
        assert(practice::ReplaySha256(Read(file)) ==
               "de13aa2df1085ae10c0f12d90571682165544f8c2f5e074ec369d4e51d94c0a6");
        Write(root / "verified-splash-disabled.asset", Read(file));
        assert(suppliedGame.Restore() && Read(file) == actual);
        std::cout
            << "Supplied Unity 2021.2.19f1 settings: exact hash, two-byte patch and byte-identical restoration: PASS\n";
    }
    std::cout << "Pre-launch logo settings, exact byte changes, hash guards and original recovery: PASS\n";
}
