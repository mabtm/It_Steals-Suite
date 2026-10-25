#pragma once
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

// Startup discovery is bounded and runs once, never in a frame or polling loop.
namespace game_launch
{
namespace fs = std::filesystem;
inline constexpr char AppId[] = "1349060";
inline constexpr size_t MetadataLimit = 512 * 1024;

inline std::string Lower(std::string value)
{
    for (auto &c : value)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}

inline fs::path Utf8Path(const std::string &text)
{
    return fs::path(std::u8string(text.begin(), text.end()));
}

inline std::string PathKey(const fs::path &path)
{
    auto utf8 = path.generic_u8string();
    return Lower(std::string(utf8.begin(), utf8.end()));
}

inline std::string Read(const fs::path &file)
{
    std::error_code error;
    auto size = fs::file_size(file, error);
    if (error || size > MetadataLimit)
        return {};
    std::ifstream input(file, std::ios::binary);
    std::string text(static_cast<size_t>(size), '\0');
    if (!input.read(text.data(), static_cast<std::streamsize>(text.size())))
        return {};
    if (text.starts_with("\xef\xbb\xbf"))
        text.erase(0, 3);
    return text;
}

struct Value
{
    std::string key, text;
    std::vector<Value> children;

    const Value *Find(const std::string &name) const
    {
        for (const auto &child : children)
            if (Lower(child.key) == Lower(name))
                return &child;
        return nullptr;
    }
};

// Steam uses quoted KeyValues. Reject truncated/oversized metadata instead of
// partially accepting an ambiguous library or installation location.
class KeyValues
{
    const std::string &text;
    size_t at = 0, tokens = 0;
    bool valid = true;

    std::optional<std::string> Token()
    {
        while (at < text.size())
        {
            if (std::isspace(static_cast<unsigned char>(text[at])))
                ++at;
            else if (text[at] == '/' && at + 1 < text.size() && text[at + 1] == '/')
            {
                while (at < text.size() && text[at] != '\n')
                    ++at;
            }
            else
                break;
        }
        if (at == text.size())
            return {};
        if (++tokens > 16384)
        {
            valid = false;
            return {};
        }
        char c = text[at++];
        if (c == '{' || c == '}')
            return std::string(1, c);
        if (c != '"')
        {
            valid = false;
            return {};
        }
        std::string out;
        while (at < text.size() && out.size() <= 4096)
        {
            c = text[at++];
            if (c == '"')
                return out;
            if (c == '\\' && at < text.size() && (text[at] == '\\' || text[at] == '"'))
                c = text[at++];
            out += c;
        }
        valid = false;
        return {};
    }

    bool Block(std::vector<Value> &values, int depth, bool nested)
    {
        if (depth > 8)
            return false;
        while (auto key = Token())
        {
            if (*key == "}")
                return nested;
            if (*key == "{")
                return false;
            auto value = Token();
            if (!value || *value == "}")
                return false;
            Value entry{*key, {}, {}};
            if (*value == "{")
            {
                if (!Block(entry.children, depth + 1, true))
                    return false;
            }
            else
                entry.text = *value;
            values.push_back(std::move(entry));
        }
        return !nested && valid;
    }

  public:
    explicit KeyValues(const std::string &source) : text(source) {}

    std::optional<Value> Parse()
    {
        Value root;
        if (text.size() > MetadataLimit || !Block(root.children, 0, false) || !valid)
            return {};
        return root;
    }
};

inline bool IsGame(const fs::path &executable)
{
    if (Lower(executable.filename().string()) != "it steals.exe")
        return false;
    std::error_code error;
    if (!fs::is_regular_file(executable.parent_path() / "It Steals_Data/Managed/Assembly-CSharp.dll", error))
        return false;
    std::ifstream input(executable, std::ios::binary);
    std::array<unsigned char, 64> dos{};
    if (!input.read(reinterpret_cast<char *>(dos.data()), dos.size()) || dos[0] != 'M' || dos[1] != 'Z')
        return false;
    uint32_t offset =
        uint32_t(dos[60]) | uint32_t(dos[61]) << 8 | uint32_t(dos[62]) << 16 | uint32_t(dos[63]) << 24;
    if (offset < 64 || offset > 1024 * 1024)
        return false;
    std::array<unsigned char, 6> pe{};
    input.seekg(offset);
    return input.read(reinterpret_cast<char *>(pe.data()), pe.size()) && pe[0] == 'P' && pe[1] == 'E' &&
           pe[2] == 0 && pe[3] == 0 && pe[4] == 0x64 && pe[5] == 0x86;
}

inline void AddLibrary(std::vector<fs::path> &libraries, const fs::path &path)
{
    if (path.empty() || libraries.size() >= 32)
        return;
    auto normalized = path.lexically_normal();
    auto key = PathKey(normalized);
    for (const auto &known : libraries)
        if (PathKey(known) == key)
            return;
    libraries.push_back(std::move(normalized));
}

inline std::vector<fs::path> Libraries(const fs::path &steam)
{
    std::vector<fs::path> libraries;
    AddLibrary(libraries, steam);
    auto source = Read(steam / "steamapps/libraryfolders.vdf");
    auto parsed = KeyValues(source).Parse();
    auto *folders = parsed ? parsed->Find("libraryfolders") : nullptr;
    if (folders)
        for (const auto &entry : folders->children)
        {
            if (entry.key.empty() || !std::all_of(entry.key.begin(), entry.key.end(),
                                                  [](unsigned char c) { return std::isdigit(c); }))
                continue;
            auto *path = entry.Find("path");
            auto value = path ? path->text : entry.text;
            if (!value.empty())
                AddLibrary(libraries, Utf8Path(value));
        }
    return libraries;
}

inline fs::path GameInLibrary(const fs::path &library)
{
    auto text = Read(library / "steamapps/appmanifest_1349060.acf");
    auto parsed = KeyValues(text).Parse();
    auto *state = parsed ? parsed->Find("AppState") : nullptr;
    auto *id = state ? state->Find("appid") : nullptr;
    auto *directory = state ? state->Find("installdir") : nullptr;
    if (!id || id->text != AppId || !directory || directory->text.empty() || directory->text == "." ||
        directory->text == ".." || directory->text.find_first_of("/\\:") != std::string::npos)
        return {};
    auto executable = library / "steamapps/common" / Utf8Path(directory->text) / "It Steals.exe";
    return IsGame(executable) ? executable : fs::path{};
}

inline fs::path ConfiguredGame(const fs::path &application)
{
    auto source = Read(application / "installation.ini");
    size_t begin = 0;
    while (begin < source.size())
    {
        auto end = source.find('\n', begin);
        std::string line = source.substr(begin, end == std::string::npos ? end : end - begin);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.starts_with("gameExecutable="))
        {
            auto path = Utf8Path(line.substr(15));
            if (path.is_absolute() && IsGame(path))
                return path;
            return {};
        }
        if (end == std::string::npos)
            break;
        begin = end + 1;
    }
    return {};
}

struct Location
{
    fs::path game, steam;
};

inline std::wstring CommandLine(const Location &location, bool skipIntro)
{
    auto executable = location.steam.empty() ? location.game : location.steam;
    std::wstring command = L"\"" + executable.wstring() + L"\"";
    if (!location.steam.empty())
        command += L" -applaunch 1349060";
    if (skipIntro)
        command += L" --practice-suite-skip-intro --melonloader.disablestartscreen";
    return command;
}

inline Location Resolve(const fs::path &application, const std::vector<fs::path> &steamRoots)
{
    auto configured = ConfiguredGame(application);
    for (const auto &steam : steamRoots)
        for (const auto &library : Libraries(steam))
        {
            auto game = GameInLibrary(library);
            if (game.empty() || (!configured.empty() && PathKey(game) != PathKey(configured)))
                continue;
            std::error_code error;
            auto client = steam / "Steam.exe";
            return {game, fs::is_regular_file(client, error) ? client : fs::path{}};
        }
    if (!configured.empty())
        return {configured, {}};
    for (const auto &folder : {application, application.parent_path()})
    {
        auto game = folder / "It Steals.exe";
        if (IsGame(game))
            return {game, {}};
    }
    return {};
}

struct StartupOnce
{
    bool attempted = false;

    bool Plan(bool enabled, bool alreadyRunning, bool suppressed)
    {
        if (attempted)
            return false;
        attempted = true;
        return enabled && !alreadyRunning && !suppressed;
    }
};

#ifdef _WIN32
inline fs::path ApplicationDirectory()
{
    std::wstring path(32768, '\0');
    auto count = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!count || count >= path.size())
        return {};
    path.resize(count);
    return fs::path(path).parent_path();
}

inline std::vector<fs::path> SteamRoots()
{
    std::vector<fs::path> roots;
    for (auto hive : {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE})
        for (const auto *key : {L"Software\\Valve\\Steam", L"Software\\WOW6432Node\\Valve\\Steam"})
            for (const auto *field : {L"SteamPath", L"InstallPath"})
            {
                wchar_t value[32768]{};
                DWORD bytes = sizeof(value);
                if (RegGetValueW(hive, key, field, RRF_RT_REG_SZ, nullptr, value, &bytes) == ERROR_SUCCESS)
                    AddLibrary(roots, fs::path(value));
            }
    return roots;
}

inline bool RestartAfterUpdateFlag()
{
    int count = 0;
    auto arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    bool found = false;
    if (arguments)
    {
        for (int i = 1; i < count; i++)
            found = found || std::wstring_view(arguments[i]) == L"--restart-after-update";
        LocalFree(arguments);
    }
    return found;
}

inline bool NoLaunchFlag()
{
    int count = 0;
    auto arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    bool suppressed = false;
    if (arguments)
    {
        for (int i = 1; i < count; ++i)
            suppressed |= std::wstring(arguments[i]) == L"--no-launch";
        LocalFree(arguments);
    }
    return suppressed;
}

inline bool Start(const Location &location, bool skipIntro, DWORD &error)
{
    auto executable = location.steam.empty() ? location.game : location.steam;
    auto command = CommandLine(location, skipIntro);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        executable.parent_path().c_str(), &startup, &process))
    {
        error = GetLastError();
        return false;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}
#endif
} // namespace game_launch
