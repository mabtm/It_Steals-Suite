#pragma once
#include "TrainerPreferences.h"
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>

// The UI serializes small immutable snapshots. Disk I/O belongs to one worker;
// diagnostics/error strings are published back to the UI under a short lock.
class TrainerSettings {
    std::filesystem::path directory, file;
    std::string scheduled, committed, pending, writeError;
    std::mutex gate;
    std::condition_variable wake;
    std::thread worker;
    bool stopping = false, writing = false, readOnly = false, preserveDamaged = false;
    bool resultReady = false;
    static int Version(const std::string &bytes) {
        std::istringstream input(bytes);
        std::string line;
        while (std::getline(input, line))
            if (line.rfind("version=", 0) == 0) {
                std::istringstream value(line.substr(8));
                int version = 0;
                if (value >> version)
                    return version;
                return -1;
            }
        return 0; // Old unversioned display settings remain readable.
    }
    static std::string Read(const std::filesystem::path &path) {
        std::error_code ec;
        auto size = std::filesystem::file_size(path, ec);
        if (ec || size > 262144)
            throw std::runtime_error("Settings missing, unreadable or over 256 KiB");
        std::ifstream stream(path, std::ios::binary);
        std::ostringstream bytes;
        bytes << stream.rdbuf();
        if (!stream || stream.bad())
            throw std::runtime_error("Settings could not be read");
        if (bytes.str().find('=') == std::string::npos)
            throw std::runtime_error("Settings contain no readable values");
        return bytes.str();
    }
    std::string Write(const std::string &bytes) {
        try {
            std::error_code ec;
            std::filesystem::create_directories(directory, ec);
            if (ec)
                return "Settings directory is not writable";
            if (preserveDamaged && std::filesystem::exists(file)) {
                auto damaged = file;
                damaged += L".corrupt-" + std::to_wstring(GetTickCount64());
                if (!CopyFileW(file.c_str(), damaged.c_str(), TRUE))
                    return "Damaged settings could not be preserved";
            }
            auto temporary = file;
            temporary += L".tmp";
            {
                std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
                stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
                stream.close();
                if (!stream)
                    return "Settings could not be written";
            }
            if (!preserveDamaged && std::filesystem::exists(file)) {
                auto backup = file;
                backup += L".bak";
                if (!CopyFileW(file.c_str(), backup.c_str(), FALSE))
                    return "Settings backup could not be written";
            }
            if (!MoveFileExW(temporary.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                return "Settings could not be committed";
            preserveDamaged = false;
            return {};
        } catch (const std::exception &e) {
            return e.what();
        }
    }
    void Writer() {
        while (true) {
            std::string bytes;
            {
                std::unique_lock lock(gate);
                wake.wait(lock, [&] { return stopping || !pending.empty(); });
                if (pending.empty() && stopping)
                    return;
                bytes.swap(pending);
                writing = true;
            }
            std::string problem = Write(bytes);
            {
                std::lock_guard lock(gate);
                writeError = problem;
                resultReady = true;
                writing = false;
                if (problem.empty())
                    committed = bytes;
                else
                    scheduled.clear();
            }
        }
    }

  public:
    std::string error, loadWarning;
    TrainerSettings() {
        wchar_t buffer[32768]{};
        DWORD count = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, 32768);
        if (count > 0 && count < 32768)
            directory = std::filesystem::path(buffer) / L"ItStealsPracticeSuite";
        else {
            GetModuleFileNameW(nullptr, buffer, 32768);
            directory = std::filesystem::path(buffer).parent_path() / L"TrainerSettings";
        }
        file = directory / L"trainer.ini";
    }
    TrainerSettings(const TrainerSettings &) = delete;
    ~TrainerSettings() { Shutdown(); }
    const std::filesystem::path &Directory() const { return directory; }
    bool Pending() {
        std::lock_guard lock(gate);
        return writing || !pending.empty();
    }
    void Poll() {
        std::lock_guard lock(gate);
        if (resultReady) {
            error = writeError;
            resultReady = false;
        }
    }
    TrainerPreferences Load() {
        std::string bytes;
        std::error_code ec;
        if (!std::filesystem::exists(file, ec) && !ec)
            return {};
        try {
            bytes = Read(file);
            if (Version(bytes) < 0)
                throw std::runtime_error("Invalid settings version");
        } catch (const std::exception &e) {
            preserveDamaged = true;
            try {
                auto backup = file;
                backup += L".bak";
                bytes = Read(backup);
                loadWarning = std::string("Display settings recovered from backup: ") + e.what();
            } catch (...) {
                loadWarning = "Display settings unreadable; defaults loaded and "
                              "damaged file preserved.";
                return {};
            }
        }
        if (Version(bytes) > 18) {
            readOnly = true;
            error = "Display settings belong to a newer suite; writes are disabled.";
            return {};
        }
        if (Version(bytes) < 0) {
            loadWarning = "Settings backup is invalid; using defaults.";
            return {};
        }
        scheduled = committed = bytes;
        return TrainerPreferences::Decode(bytes);
    }
    bool Save(const TrainerPreferences &preferences) {
        Poll();
        if (readOnly || stopping)
            return false;
        std::string bytes = preferences.Encode();
        std::lock_guard lock(gate);
        if (bytes == scheduled)
            return error.empty();
        try {
            if (!worker.joinable())
                worker = std::thread(&TrainerSettings::Writer, this);
        } catch (const std::exception &e) {
            error = e.what();
            return false;
        }
        scheduled = bytes;
        pending = std::move(bytes);
        wake.notify_one();
        return true;
    }
    void Shutdown() {
        {
            std::lock_guard lock(gate);
            stopping = true;
            wake.notify_one();
        }
        if (worker.joinable())
            worker.join();
        Poll();
    }
};
