#pragma once
#include "ReplayPackage.h"
#include <filesystem>
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#endif

namespace game_launch
{
// Unity reads these settings before MelonLoader loads normal game mods.
// Bind the two offsets to the complete, supplied game's file hash.
struct SplashProfile
{
    size_t size = 73120, show = 4200, player = 68052;
    std::string hash = "32bf65a2ebc50fcae74103293bbfff572374e0c507e38b38cc2cea84d807ec55";
};

class StartupLogos
{
    SplashProfile profile;
    std::filesystem::path file, backup;
    bool pending = false;

    std::vector<uint8_t> Read(const std::filesystem::path &path) const
    {
        if (std::filesystem::file_size(path) != profile.size)
            throw std::runtime_error("Startup settings do not match the supported game build.");
        std::ifstream stream(path, std::ios::binary);
        std::vector<uint8_t> bytes(profile.size);
        stream.read(reinterpret_cast<char *>(bytes.data()), std::streamsize(bytes.size()));
        if (!stream)
            throw std::runtime_error("Cannot read the game's startup settings.");
        return bytes;
    }

    std::vector<uint8_t> Patched(std::vector<uint8_t> bytes) const
    {
        if (profile.show >= bytes.size() || profile.player >= bytes.size() ||
            practice::ReplaySha256(bytes) != profile.hash)
            throw std::runtime_error("Startup settings are modified or from an unsupported game build.");
        bytes[profile.show] = 0;
        bytes[profile.player] = 1;
        return bytes;
    }

    void Replace(const std::vector<uint8_t> &bytes) const
    {
        auto temporary = file;
        temporary += ".practicesuite-tmp";
        try
        {
            std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
            stream.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
            stream.close();
            if (!stream)
                throw std::runtime_error(
                    "Cannot write startup settings. Check the game folder's write access.");
#ifdef _WIN32
            if (!ReplaceFileW(file.c_str(), temporary.c_str(), nullptr, 0, nullptr, nullptr))
                throw std::runtime_error(
                    "Cannot replace startup settings. Close the game and check folder access.");
#else
            std::filesystem::permissions(temporary, std::filesystem::status(file).permissions());
            std::filesystem::rename(temporary, file);
#endif
        }
        catch (...)
        {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            throw;
        }
    }

    void Recover()
    {
        if (backup.empty() || !std::filesystem::exists(backup))
        {
            pending = false;
            return;
        }
        pending = true;
        auto original = Read(backup), patched = Patched(original);
        if (!std::filesystem::exists(file))
            std::filesystem::copy_file(backup, file);
        else if (auto current = Read(file); current != original)
        {
            if (current != patched)
                throw std::runtime_error(
                    "Startup settings changed externally; the original backup was kept.");
            Replace(original);
        }
        std::filesystem::remove(backup);
        pending = false;
    }

  public:
    std::string status = "Not requested";

    explicit StartupLogos(SplashProfile supported = {}) : profile(std::move(supported)) {}

    bool Pending() const
    {
        return pending;
    }

    bool Prepare(const std::filesystem::path &game, bool skip)
    {
        file = game.parent_path() / "It Steals_Data/globalgamemanagers";
        backup = file;
        backup += ".practicesuite-original";
        try
        {
            Recover(); // Also repairs an interrupted earlier Suite launch.
            if (skip)
            {
                auto original = Read(file), patched = Patched(original);
                std::filesystem::copy_file(file, backup);
                pending = true;
                if (Read(backup) != original || Read(file) != original)
                    throw std::runtime_error(
                        "Startup settings changed during preparation; no override was written.");
                Replace(patched);
            }
            status = skip ? "Prepared before game process launch" : "Normal startup logos";
            return true;
        }
        catch (const std::exception &error)
        {
            status = error.what();
            return false;
        }
    }

    bool Restore()
    {
        try
        {
            Recover();
            status = "Original startup settings restored";
            return true;
        }
        catch (const std::exception &error)
        {
            status = error.what();
            return false;
        }
    }
};
} // namespace game_launch
