#pragma once
#include "SuiteUpdateModel.h"
#include "SuiteBranding.h"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#include <wincred.h>
#include <bcrypt.h>
#include <shellapi.h>
#include "GameLaunch.h"
#ifdef _MSC_VER
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "advapi32.lib")
#endif
#endif

namespace suite_update
{
namespace fs = std::filesystem;

struct Snapshot
{
    Phase phase = Phase::Idle;
    std::string message = "Check GitHub for a stable release.", lastResult;
    std::vector<Release> releases;
    Release latest;
    bool credential = false;
};

// One bounded worker owns all network, hashing and staging I/O. Published
// snapshots are immutable; rendering never makes a network or disk request.
class Service
{
    friend struct ServiceContract;
    mutable std::mutex gate;
    std::shared_ptr<const Snapshot> snapshot = std::make_shared<Snapshot>();
    std::thread worker;
    std::atomic<bool> running{false}, cancel{false};
    fs::path directory, stage;
#ifdef _WIN32
    std::atomic<HINTERNET> active{nullptr};
    HANDLE helper = nullptr;
    ULONGLONG pollAt = 0;
    inline static constexpr wchar_t CredentialName[] = L"ItStealsPracticeSuite/GitHubReleases";

    struct Internet
    {
        HINTERNET h = nullptr;

        explicit Internet(HINTERNET value) : h(value)
        {
            if (!h)
                throw std::runtime_error("Could not open a secure update connection.");
        }

        ~Internet()
        {
            if (h)
                WinHttpCloseHandle(h);
        }

        Internet(const Internet &) = delete;
    };

    struct Request
    {
        Service &service;
        HINTERNET h;

        Request(Service &owner, HINTERNET value) : service(owner), h(value)
        {
            if (!h)
                throw std::runtime_error("Could not create an update request.");
            service.active.store(h);
        }

        ~Request()
        {
            auto expected = h;
            if (service.active.compare_exchange_strong(expected, nullptr))
                WinHttpCloseHandle(h);
        }
    };

    struct Secret
    {
        std::string value;

        ~Secret()
        {
            if (!value.empty())
                SecureZeroMemory(value.data(), value.size());
        }
    };

    static std::string Credential()
    {
        PCREDENTIALW found = nullptr;
        if (!CredReadW(CredentialName, CRED_TYPE_GENERIC, 0, &found))
            return {};
        std::string value;
        if (found->CredentialBlobSize > 0 && found->CredentialBlobSize <= TokenLimit)
            value.assign(reinterpret_cast<const char *>(found->CredentialBlob), found->CredentialBlobSize);
        if (found->CredentialBlobSize)
            SecureZeroMemory(found->CredentialBlob, found->CredentialBlobSize);
        CredFree(found);
        if (!ValidToken(value))
        {
            if (!value.empty())
                SecureZeroMemory(value.data(), value.size());
            value.clear();
        }
        return value;
    }

    static std::wstring Wide(std::string_view text)
    {
        if (text.empty())
            return {};
        int length =
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), int(text.size()), nullptr, 0);
        if (length <= 0)
            throw std::runtime_error("Update metadata contains invalid UTF-8.");
        std::wstring out(size_t(length), 0);
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), int(text.size()), out.data(), length);
        return out;
    }

    static std::string Utf8(const fs::path &path)
    {
        auto text = path.u8string();
        return {reinterpret_cast<const char *>(text.data()), text.size()};
    }

    static std::string Resource(int id)
    {
        auto resource = FindResourceW(nullptr, MAKEINTRESOURCEW(id), RT_RCDATA);
        auto loaded = resource ? LoadResource(nullptr, resource) : nullptr;
        auto bytes = loaded ? LockResource(loaded) : nullptr;
        auto size = resource ? SizeofResource(nullptr, resource) : 0;
        if (!bytes || !size || size > 1024 * 1024)
            throw std::runtime_error("Embedded update components are missing. Reinstall Elite-Suite.");
        return {static_cast<const char *>(bytes), size};
    }

    static uint64_t Created(HANDLE process)
    {
        FILETIME created{}, exit{}, kernel{}, user{};
        if (!GetProcessTimes(process, &created, &exit, &kernel, &user))
            throw std::runtime_error("Could not identify a process for the update.");
        return uint64_t(created.dwLowDateTime) | (uint64_t(created.dwHighDateTime) << 32);
    }

    static fs::path Image(HANDLE process)
    {
        std::wstring image(32768, 0);
        DWORD size = DWORD(image.size());
        if (!QueryFullProcessImageNameW(process, 0, image.data(), &size))
            throw std::runtime_error("Could not identify the game executable.");
        image.resize(size);
        return fs::path(image);
    }

    void CheckCancelled() const
    {
        if (cancel.load())
            throw std::runtime_error("Update cancelled. The installed files were not changed.");
    }

    template <class Sink> void Receive(std::string url, bool binary, uint64_t limit, Sink sink)
    {
        auto agent = Wide(std::string("Elite-Suite/") + suite_brand::Version);
        Internet session(WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
        WinHttpSetTimeouts(session.h, 5000, 8000, 8000, 8000);
        DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        if (!WinHttpSetOption(session.h, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect)))
            throw std::runtime_error("Could not enforce safe update redirects.");
        Secret token{Credential()};
        ULONGLONG started = GetTickCount64();
        for (int redirects = 0; redirects < 5; redirects++)
        {
            CheckCancelled();
            auto address = Wide(url);
            URL_COMPONENTS parts{};
            parts.dwStructSize = sizeof(parts);
            parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength =
                parts.dwUserNameLength = parts.dwPasswordLength = DWORD(-1);
            if (!WinHttpCrackUrl(address.c_str(), DWORD(address.size()), 0, &parts) ||
                parts.nScheme != INTERNET_SCHEME_HTTPS || parts.nPort != 443 || parts.dwUserNameLength ||
                parts.dwPasswordLength)
                throw std::runtime_error("The update URL is not a permitted HTTPS address.");
            std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
            std::transform(host.begin(), host.end(), host.begin(),
                           [](wchar_t c) { return c >= L'A' && c <= L'Z' ? c + 32 : c; });
            bool api = host == L"api.github.com";
            if ((!api && (!binary || (host != L"release-assets.githubusercontent.com" &&
                                      host != L"objects.githubusercontent.com"))) ||
                (redirects && api))
                throw std::runtime_error("The update redirect is outside GitHub release storage.");
            std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
            if (parts.dwExtraInfoLength)
                path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
            Internet connection(WinHttpConnect(session.h, host.c_str(), 443, 0));
            Request request(*this, WinHttpOpenRequest(connection.h, L"GET", path.c_str(), nullptr,
                                                      WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                      WINHTTP_FLAG_SECURE));
            CheckCancelled();
            std::wstring headers =
                binary ? L"Accept: application/octet-stream\r\n" : L"Accept: application/vnd.github+json\r\n";
            headers += L"X-GitHub-Api-Version: 2022-11-28\r\n";
            if (api && !token.value.empty())
                headers += L"Authorization: Bearer " + Wide(token.value) + L"\r\n";
            BOOL sent = WinHttpSendRequest(request.h, headers.c_str(), DWORD(headers.size()),
                                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
            if (!headers.empty())
                SecureZeroMemory(headers.data(), headers.size() * sizeof(wchar_t));
            if (!sent || !WinHttpReceiveResponse(request.h, nullptr))
            {
                CheckCancelled();
                throw std::runtime_error("GitHub could not be reached. Check your connection and try again.");
            }
            DWORD status = 0, length = sizeof(status);
            if (!WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                     WINHTTP_HEADER_NAME_BY_INDEX, &status, &length, WINHTTP_NO_HEADER_INDEX))
                throw std::runtime_error("The update response has no status.");
            if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308)
            {
                wchar_t location[8192]{};
                length = sizeof(location);
                if (!binary ||
                    !WinHttpQueryHeaders(request.h, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX,
                                         location, &length, WINHTTP_NO_HEADER_INDEX))
                    throw std::runtime_error("GitHub returned an unsupported redirect.");
                int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, location, -1, nullptr, 0,
                                                nullptr, nullptr);
                if (count <= 1)
                    throw std::runtime_error("GitHub returned an empty redirect.");
                std::string next(size_t(count), 0);
                WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, location, -1, next.data(), count, nullptr,
                                    nullptr);
                next.pop_back();
                url = std::move(next);
                continue;
            }
            // A revoked private-release credential must not block public releases.
            // Retry this same API request anonymously once; never forward the token
            // to a release-storage redirect.
            if (status == 401 && api && !token.value.empty())
            {
                SecureZeroMemory(token.value.data(), token.value.size());
                token.value.clear();
                --redirects;
                continue;
            }
            if (status == 401 || status == 404)
                throw std::runtime_error(
                    "Release access is unavailable. For private builds, connect a GitHub token with Contents: read access to this repository.");
            if (status == 403 || status == 429)
                throw std::runtime_error(
                    "GitHub denied the request or its rate limit was reached. Check repository access and try again later.");
            if (status != 200)
                throw std::runtime_error("GitHub could not provide the release (HTTP " +
                                         std::to_string(status) + "). Try again later.");
            uint64_t total = 0;
            std::array<char, 65536> bytes{};
            for (;;)
            {
                CheckCancelled();
                if (GetTickCount64() - started > 300000)
                    throw std::runtime_error("The update download timed out. Try again later.");
                DWORD read = 0;
                if (!WinHttpReadData(request.h, bytes.data(), DWORD(bytes.size()), &read))
                {
                    CheckCancelled();
                    throw std::runtime_error("The update download was interrupted. Retry the download.");
                }
                if (!read)
                    return;
                total += read;
                if (total > limit)
                    throw std::runtime_error("The update response exceeds its declared size.");
                sink(bytes.data(), read);
            }
        }
        throw std::runtime_error("GitHub returned too many update redirects.");
    }

    void Download(const Release &release)
    {
        std::error_code ec;
        fs::create_directories(directory / L"Updates", ec);
        if (ec)
            throw std::runtime_error("The update staging folder is not writable.");
        if (!stage.empty())
            fs::remove_all(stage, ec);
        GUID guid{};
        if (FAILED(CoCreateGuid(&guid)))
            throw std::runtime_error("Could not allocate an update job.");
        wchar_t name[40]{};
        StringFromGUID2(guid, name, 40);
        stage = directory / L"Updates" / name;
        if (!fs::create_directory(stage))
            throw std::runtime_error("Could not create a private update job folder.");
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        try
        {
            if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
                BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) < 0)
                throw std::runtime_error("SHA-256 verification is unavailable.");
            std::ofstream file(stage / L"package.zip", std::ios::binary);
            if (!file)
                throw std::runtime_error("The update package could not be created.");
            Receive(std::string(ApiRoot) + "/releases/assets/" + std::to_string(release.asset), true,
                    release.size, [&](const char *bytes, DWORD count) {
                        file.write(bytes, count);
                        if (!file || BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char *>(bytes)),
                                                    count, 0) < 0)
                            throw std::runtime_error("The update package could not be saved or verified.");
                        received.fetch_add(count);
                    });
            file.close();
            std::array<UCHAR, 32> digest{};
            if (!file || received.load() != release.size ||
                BCryptFinishHash(hash, digest.data(), DWORD(digest.size()), 0) < 0)
                throw std::runtime_error("The update download is incomplete.");
            std::string actual;
            for (auto c : digest)
            {
                actual += "0123456789abcdef"[c >> 4];
                actual += "0123456789abcdef"[c & 15];
            }
            if (actual != release.sha256)
                throw std::runtime_error(
                    "The update checksum does not match GitHub. No installed files were changed.");
            BCryptDestroyHash(hash);
            hash = nullptr;
            BCryptCloseAlgorithmProvider(algorithm, 0);
            algorithm = nullptr;
            Set(Phase::Ready, "Download verified. Ready to restart and install.");
        }
        catch (...)
        {
            if (hash)
                BCryptDestroyHash(hash);
            if (algorithm)
                BCryptCloseAlgorithmProvider(algorithm, 0);
            fs::remove_all(stage, ec);
            stage.clear();
            throw;
        }
    }
#endif
    void Set(Phase phase, std::string message)
    {
        std::lock_guard lock(gate);
        auto next = std::make_shared<Snapshot>(*snapshot);
        next->phase = phase;
        next->message = std::move(message);
        snapshot = std::move(next);
    }

    template <class Action> bool Start(Action action)
    {
        if (running.load())
            return false;
        if (worker.joinable())
            worker.join();
        cancel.store(false);
        running.store(true);
        worker = std::thread([this, action] {
#ifdef _WIN32
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#endif
            try
            {
                action();
            }
            catch (const std::exception &e)
            {
                Set(Phase::Error, e.what());
            }
            catch (...)
            {
                Set(Phase::Error, "The update could not be completed. The installed version is retained.");
            }
            running.store(false);
        });
        return true;
    }

    static std::string Read(const fs::path &path, uint64_t limit = 65536)
    {
        std::error_code ec;
        auto size = fs::file_size(path, ec);
        if (ec || size > limit)
            return {};
        std::ifstream file(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(file), {}};
    }

    static void Write(const fs::path &path, std::string_view bytes)
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(bytes.data(), std::streamsize(bytes.size()));
        file.close();
        if (!file)
            throw std::runtime_error("Update state could not be saved.");
    }

  public:
    Presentation presentation;
    bool autoCheck = true, installBlocked = false;
    std::string stateError;
    std::vector<Release> bundled;
    std::atomic<uint64_t> received{0};
    Service() = default;
    Service(const Service &) = delete;

    ~Service()
    {
        Cancel();
        if (worker.joinable())
            worker.join();
#ifdef _WIN32
        if (helper)
            CloseHandle(helper);
#endif
    }

    std::shared_ptr<const Snapshot> View() const
    {
        std::lock_guard lock(gate);
        return snapshot;
    }

    void Init(const fs::path &folder)
    {
        directory = folder;
        auto state = Read(directory / L"updates.ini");
        auto value = [&](std::string_view key) {
            auto at = state.find(std::string(key) + '=');
            if (at == std::string::npos || (at && state[at - 1] != '\n'))
                return std::string{};
            at += key.size() + 1;
            auto end = state.find_first_of("\r\n", at);
            return state.substr(at, end == std::string::npos ? end : end - at);
        };
        auto seen = value("seenVersion");
        if (Version::Parse(seen))
            presentation.seenVersion = seen;
        presentation.promptedSession.clear(); // A trainer restart may show the session prompt again.
        autoCheck = value("autoCheck") != "0";
#ifdef _WIN32
        try
        {
            bundled = Changelog(Resource(103));
        }
        catch (const std::exception &e)
        {
            stateError = e.what();
        }
        auto next = std::make_shared<Snapshot>();
        Secret secret{Credential()};
        next->credential = !secret.value.empty();
        auto result = Read(directory / L"Updates" / L"result.txt");
        next->lastResult = result.substr(0, 2048);
        std::lock_guard lock(gate);
        snapshot = next;
#endif
    }

    bool SaveState()
    {
        try
        {
            fs::create_directories(directory);
            auto temporary = directory / L"updates.ini.tmp";
            Write(temporary, "autoCheck=" + std::to_string(autoCheck) +
                                 "\nseenVersion=" + presentation.seenVersion + "\n");
#ifdef _WIN32
            if (!MoveFileExW(temporary.c_str(), (directory / L"updates.ini").c_str(),
                             MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                throw std::runtime_error("Update preferences could not be committed.");
#else
            fs::rename(temporary, directory / "updates.ini");
#endif
            stateError.clear();
            return true;
        }
        catch (const std::exception &e)
        {
            stateError = e.what();
            return false;
        }
    }

    void MarkRead()
    {
        presentation.seenVersion = suite_brand::Version;
        presentation.changelog = false;
        SaveState();
    }

    bool Check()
    {
        if (running.load() || Busy(View()->phase) || View()->phase == Phase::Ready)
            return false;
        Set(Phase::Checking, "Checking stable releases on GitHub...");
        return Start([this] {
#ifdef _WIN32
            std::string bytes;
            Receive(std::string(ApiRoot) + "/releases?per_page=100", false, 4 * 1024 * 1024,
                    [&](const char *data, DWORD size) { bytes.append(data, size); });
            auto releases = Releases(bytes);
            // Frequent preview builds can fill the first metadata page. GitHub's
            // latest endpoint still selects the stable channel in that case.
            if (releases.empty())
            {
                bytes.clear();
                Receive(std::string(ApiRoot) + "/releases/latest", false, 4 * 1024 * 1024,
                        [&](const char *data, DWORD size) { bytes.append(data, size); });
                releases = Releases("[" + bytes + "]");
            }
            if (releases.empty())
                throw std::runtime_error("No published stable releases are available yet.");
            auto next = std::make_shared<Snapshot>();
            next->releases = std::move(releases);
            if (!next->releases.empty())
                next->latest = next->releases.front();
            Secret secret{Credential()};
            next->credential = !secret.value.empty();
            bool available = Newer(next->latest, suite_brand::Version, suite_brand::Preview);
            next->phase = available ? Phase::Available : Phase::Current;
            next->message = available ? "A new Elite-Suite update is available."
                                      : suite_brand::Preview ? "Preview build. No newer stable release is available."
                                                             : "You are using the latest stable version.";
            if (available && !next->latest.Installable())
                next->message =
                    "A newer release exists, but its verified update package is not ready. Try again later.";
            std::lock_guard lock(gate);
            next->lastResult = snapshot->lastResult;
            snapshot = std::move(next);
#endif
        });
    }

    bool DownloadLatest()
    {
        auto current = View();
        if (running.load() || Busy(current->phase) || !Newer(current->latest, suite_brand::Version, suite_brand::Preview) ||
            !current->latest.Installable())
            return false;
        received.store(0);
        auto release = current->latest;
        Set(Phase::Downloading, "Downloading the complete Suite. You can keep playing.");
        return Start([this, release] {
#ifdef _WIN32
            Download(release);
#endif
        });
    }

    void Cancel()
    {
        cancel.store(true);
#ifdef _WIN32
        auto handle = active.exchange(nullptr);
        if (handle)
            WinHttpCloseHandle(handle);
#endif
    }

    bool StoreCredential(std::string_view token)
    {
#ifdef _WIN32
        if (Busy(View()->phase))
            return false;
        if (token.empty())
        {
            if (!CredDeleteW(CredentialName, CRED_TYPE_GENERIC, 0) && GetLastError() != ERROR_NOT_FOUND)
            {
                stateError = "Windows could not remove the saved credential.";
                return false;
            }
        }
        else
        {
            if (!ValidToken(token))
            {
                stateError = "Enter a valid GitHub token, without spaces.";
                return false;
            }
            CREDENTIALW entry{};
            entry.Type = CRED_TYPE_GENERIC;
            entry.TargetName = const_cast<wchar_t *>(CredentialName);
            entry.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char *>(token.data()));
            entry.CredentialBlobSize = DWORD(token.size());
            entry.Persist = CRED_PERSIST_LOCAL_MACHINE;
            entry.UserName = const_cast<wchar_t *>(L"GitHub");
            if (!CredWriteW(&entry, 0))
            {
                stateError = "Windows Credential Manager could not save this token (Windows error " +
                             std::to_string(GetLastError()) + ").";
                return false;
            }
        }
        {
            std::lock_guard lock(gate);
            auto next = std::make_shared<Snapshot>(*snapshot);
            next->credential = !token.empty();
            snapshot = std::move(next);
        }
        stateError.clear();
        return true;
#else
        (void)token;
        return false;
#endif
    }

    static std::string Session(uint32_t pid)
    {
#ifdef _WIN32
        if (!pid)
            return {};
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!process)
            return {};
        try
        {
            auto key = std::to_string(pid) + ":" + std::to_string(Created(process));
            CloseHandle(process);
            return key;
        }
        catch (...)
        {
            CloseHandle(process);
            return {};
        }
#else
        return std::to_string(pid);
#endif
    }

    bool Install(uint32_t gamePid)
    {
#ifdef _WIN32
        if (running.load() || View()->phase != Phase::Ready || helper)
            return false;
        Set(Phase::Installing, "Preparing verified installation and the Windows permission prompt...");
        return Start([this, gamePid] {
            auto release = View()->latest;
            auto application = game_launch::ApplicationDirectory();
            fs::path game;
            uint64_t gameCreated = 0;
            if (gamePid)
            {
                HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, gamePid);
                if (!process)
                    throw std::runtime_error("The game closed before the update. Try Install again.");
                try
                {
                    game = Image(process);
                    gameCreated = Created(process);
                    CloseHandle(process);
                }
                catch (...)
                {
                    CloseHandle(process);
                    throw;
                }
            }
            else
                game = game_launch::Resolve(application, game_launch::SteamRoots()).game;
            if (game.empty() || !game_launch::IsGame(game))
                throw std::runtime_error("Locate It Steals using Setup before updating.");
            Write(stage / L"Update.ps1", Resource(102));
            Write(stage / L"Install.ps1", Resource(104));
            std::string request =
                "{\"version\":" + Quote(release.version) + ",\"sha256\":" + Quote(release.sha256) +
                ",\"size\":" + std::to_string(release.size) + ",\"application\":" + Quote(Utf8(application)) +
                ",\"game\":" + Quote(Utf8(game)) +
                ",\"trainerPid\":" + std::to_string(GetCurrentProcessId()) +
                ",\"trainerCreated\":" + Quote(std::to_string(Created(GetCurrentProcess()))) +
                ",\"gamePid\":" + std::to_string(gamePid) +
                ",\"gameCreated\":" + Quote(std::to_string(gameCreated)) + "}";
            Write(stage / L"request.json", request);
            wchar_t system[32768]{};
            UINT count = GetSystemDirectoryW(system, DWORD(std::size(system)));
            if (!count || count >= std::size(system))
                throw std::runtime_error("Windows PowerShell could not be located.");
            auto executable = fs::path(system) / L"WindowsPowerShell/v1.0/powershell.exe";
            std::wstring arguments =
                L"-NoProfile -NonInteractive -ExecutionPolicy RemoteSigned -WindowStyle Hidden -File \"" +
                (stage / L"Update.ps1").wstring() + L"\" -RequestFile \"" +
                (stage / L"request.json").wstring() + L"\"";
            SHELLEXECUTEINFOW launch{};
            launch.cbSize = sizeof(launch);
            launch.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
            launch.lpFile = executable.c_str();
            launch.lpParameters = arguments.c_str();
            launch.nShow = SW_HIDE;
            if (!ShellExecuteExW(&launch) || !launch.hProcess)
                throw std::runtime_error("The update helper could not be started.");
            {
                std::lock_guard lock(gate);
                helper = launch.hProcess;
            }
            Set(Phase::Installing,
                "Preparing installation. Accept the Windows permission prompt; the game and trainer will restart.");
        });
#else
        (void)gamePid;
        return false;
#endif
    }

    void Poll()
    {
#ifdef _WIN32
        if (GetTickCount64() < pollAt)
            return;
        pollAt = GetTickCount64() + 500;
        HANDLE process;
        {
            std::lock_guard lock(gate);
            process = helper;
        }
        if (process && WaitForSingleObject(process, 0) == WAIT_OBJECT_0)
        {
            CloseHandle(process);
            {
                std::lock_guard lock(gate);
                helper = nullptr;
            }
            auto detail = Read(stage / L"status.txt");
            Set(Phase::Error, detail.empty()
                                  ? "The update helper stopped before the restart. Try installing again."
                                  : detail.substr(0, 2048));
        }
#endif
    }
};

inline Service service;
} // namespace suite_update
