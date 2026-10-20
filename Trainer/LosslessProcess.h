#pragma once
#include "EncoderIdentity.h"
#ifdef _WIN32
#include <atomic>
#include <bcrypt.h>
namespace suite_clip {
class WinHandle {
  public:
    HANDLE value = nullptr;
    ~WinHandle() { Reset(); }
    void Reset(HANDLE v = nullptr) {
        if (value && value != INVALID_HANDLE_VALUE)
            CloseHandle(value);
        value = v;
    }
    HANDLE Take() {
        HANDLE h = value;
        value = nullptr;
        return h;
    }
    operator HANDLE() const { return value; }
    WinHandle() = default;
    WinHandle(const WinHandle &) = delete;
    WinHandle &operator=(const WinHandle &) = delete;
};
inline std::wstring QuoteArgument(const std::wstring &value) {
    std::wstring out = L"\"";
    unsigned slashes = 0;
    for (wchar_t c : value) {
        if (c == L'\\') {
            slashes++;
            continue;
        }
        if (c == L'\"')
            out.append(slashes * 2 + 1, L'\\');
        else
            out.append(slashes, L'\\');
        slashes = 0;
        out += c;
    }
    out.append(slashes * 2, L'\\');
    return out + L"\"";
}
inline std::filesystem::path EncoderExecutable() {
    wchar_t path[32768];
    DWORD n = GetModuleFileNameW(nullptr, path, 32768);
    if (!n || n >= 32768)
        throw std::runtime_error("Cannot resolve suite encoder path");
    return std::filesystem::path(path).parent_path() / L"PracticeSuiteEncoder.exe";
}
inline void VerifyEncoder(HANDLE file) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD length = 0, read = 0;
    std::vector<uint8_t> object;
    uint8_t digest[32], bytes[65536];
    try {
        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
            BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&length),
                              sizeof(length), &read, 0) < 0)
            throw std::runtime_error("Cannot initialize encoder checksum");
        object.resize(length);
        if (BCryptCreateHash(algorithm, &hash, object.data(), length, nullptr, 0, 0) < 0)
            throw std::runtime_error("Cannot verify suite encoder");
        uint64_t total = 0;
        while (true) {
            if (!ReadFile(file, bytes, sizeof(bytes), &read, nullptr))
                throw std::runtime_error("Cannot read suite encoder");
            if (!read)
                break;
            total += read;
            if (total > 16 * 1024 * 1024 || BCryptHashData(hash, bytes, read, 0) < 0)
                throw std::runtime_error("Invalid suite encoder file");
        }
        if (BCryptFinishHash(hash, digest, 32, 0) < 0)
            throw std::runtime_error("Cannot verify suite encoder");
        std::string actual;
        const char *hex = "0123456789abcdef";
        for (auto b : digest) {
            actual += hex[b >> 4];
            actual += hex[b & 15];
        }
        if (actual != EncoderSha256)
            throw std::runtime_error("Video encoder checksum mismatch; reinstall "
                                     "the matching suite update");
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
    } catch (...) {
        if (hash)
            BCryptDestroyHash(hash);
        if (algorithm)
            BCryptCloseAlgorithmProvider(algorithm, 0);
        throw;
    }
}
class LosslessWriter {
    WinHandle pipe, process, job, event, log;
    std::filesystem::path logPath;
    const std::atomic<bool> &cancelled;
    bool complete = false;
    bool streaming = true;
    void CheckCancelled() {
        if (cancelled)
            throw std::runtime_error("Export cancelled");
    }
    std::string Failure() {
        std::ifstream input(logPath, std::ios::binary);
        std::string text(2048, '\0');
        input.read(text.data(), std::streamsize(text.size()));
        text.resize(size_t(input.gcount()));
        return text.empty() ? "Video encoder stopped unexpectedly" : text;
    }

  public:
    LosslessWriter(const std::filesystem::path &output, const Options &o, const std::atomic<bool> &abort,
                   const std::wstring &command = {}, bool raw = true)
        : cancelled(abort), streaming(raw) {
        try {
            auto executable = EncoderExecutable();
            WinHandle verified;
            verified.Reset(CreateFileW(executable.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
            if (verified.value == INVALID_HANDLE_VALUE)
                throw std::runtime_error("PracticeSuiteEncoder.exe is missing beside the trainer");
            VerifyEncoder(verified);
            SECURITY_ATTRIBUTES inherit{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
            WinHandle read;
            if (streaming) {
                uint64_t nonce = 0;
                if (BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(&nonce), sizeof(nonce),
                                    BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
                    throw std::runtime_error("Cannot create encoder channel");
                auto name = L"\\\\.\\pipe\\ItSteals.Encode." + std::to_wstring(GetCurrentProcessId()) + L"." +
                            std::to_wstring(nonce);
                pipe.Reset(CreateNamedPipeW(
                    name.c_str(), PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                    PIPE_TYPE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, 1048576, 1048576, 0, nullptr));
                if (pipe.value == INVALID_HANDLE_VALUE)
                    throw std::runtime_error("Cannot create video frame channel");
                read.Reset(CreateFileW(name.c_str(), GENERIC_READ, 0, &inherit, OPEN_EXISTING,
                                       FILE_ATTRIBUTE_NORMAL, nullptr));
                if (read.value == INVALID_HANDLE_VALUE)
                    throw std::runtime_error("Cannot open video frame channel");
                event.Reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
                if (!event.value)
                    throw std::runtime_error("Cannot initialize encoder event");
                OVERLAPPED connection{};
                connection.hEvent = event;
                BOOL connected = ConnectNamedPipe(pipe, &connection);
                DWORD connectionError = connected ? ERROR_SUCCESS : GetLastError();
                if (!connected && connectionError != ERROR_PIPE_CONNECTED) {
                    if (connectionError != ERROR_IO_PENDING ||
                        WaitForSingleObject(event, 5000) != WAIT_OBJECT_0)
                        throw std::runtime_error("Cannot connect video frame channel");
                    DWORD ignored = 0;
                    if (!GetOverlappedResult(pipe, &connection, &ignored, FALSE))
                        throw std::runtime_error("Cannot connect video encoder pipe");
                }
            } else {
                read.Reset(CreateFileW(L"NUL", GENERIC_READ, 0, &inherit, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                                       nullptr));
                if (read.value == INVALID_HANDLE_VALUE)
                    throw std::runtime_error("Cannot initialize video encoder input");
            }
            logPath = output;
            logPath += L".encoder.log";
            log.Reset(CreateFileW(logPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, &inherit,
                                  CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
            if (log.value == INVALID_HANDLE_VALUE)
                throw std::runtime_error("Cannot reserve encoder diagnostic log");
            job.Reset(CreateJobObjectW(nullptr, nullptr));
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            if (!job.value ||
                !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
                throw std::runtime_error("Cannot initialize encoder lifecycle");
            auto codec = LosslessArguments(o);
            std::wstring args =
                QuoteArgument(executable.wstring()) + L" " +
                (command.empty() ? std::wstring(codec.begin(), codec.end()) + QuoteArgument(output.wstring())
                                 : command);
            SIZE_T bytes = 0;
            InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
            std::vector<uint8_t> storage(bytes);
            auto attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
            if (!InitializeProcThreadAttributeList(attributes, 1, 0, &bytes))
                throw std::runtime_error("Cannot prepare encoder handles");
            HANDLE handles[] = {read.value, log.value};
            if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles,
                                           sizeof(handles), nullptr, nullptr)) {
                DeleteProcThreadAttributeList(attributes);
                throw std::runtime_error("Cannot restrict encoder handles");
            }
            STARTUPINFOEXW startup{};
            startup.StartupInfo.cb = sizeof(startup);
            startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
            startup.StartupInfo.hStdInput = read;
            startup.StartupInfo.hStdOutput = log;
            startup.StartupInfo.hStdError = log;
            startup.lpAttributeList = attributes;
            PROCESS_INFORMATION info{};
            BOOL ok = CreateProcessW(executable.c_str(), args.data(), nullptr, nullptr, TRUE,
                                     CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT,
                                     nullptr, executable.parent_path().c_str(), &startup.StartupInfo, &info);
            DeleteProcThreadAttributeList(attributes);
            if (!ok)
                throw std::runtime_error("Cannot start the suite video encoder");
            process.Reset(info.hProcess);
            WinHandle mainThread;
            mainThread.Reset(info.hThread);
            if (!AssignProcessToJobObject(job, process)) {
                TerminateProcess(process, 1);
                throw std::runtime_error("Cannot contain the suite video encoder");
            }
            if (ResumeThread(mainThread) == DWORD(-1))
                throw std::runtime_error("Cannot resume video encoder");
        } catch (...) {
            if (process.value && job.value)
                TerminateJobObject(job, 1);
            if (log.value && log.value != INVALID_HANDLE_VALUE) {
                log.Reset();
                std::error_code ec;
                std::filesystem::remove(logPath, ec);
            }
            throw;
        }
    }
    ~LosslessWriter() {
        if (!complete && job.value)
            TerminateJobObject(job, 1);
        pipe.Reset();
        if (process.value)
            WaitForSingleObject(process, 3000);
        log.Reset();
        std::error_code ec;
        if (!logPath.empty())
            std::filesystem::remove(logPath, ec);
    }
    void Write(const std::vector<uint8_t> &bytes) {
        size_t at = 0;
        while (at < bytes.size()) {
            CheckCancelled();
            ResetEvent(event);
            OVERLAPPED operation{};
            operation.hEvent = event;
            DWORD count = 0;
            BOOL ok = WriteFile(pipe, bytes.data() + at, DWORD(std::min(size_t(1048576), bytes.size() - at)),
                                &count, &operation);
            if (!ok) {
                if (GetLastError() != ERROR_IO_PENDING)
                    throw std::runtime_error(Failure());
                ULONGLONG start = GetTickCount64();
                while (WaitForSingleObject(event, 20) == WAIT_TIMEOUT) {
                    if (cancelled || GetTickCount64() - start > 30000 ||
                        WaitForSingleObject(process, 0) == WAIT_OBJECT_0) {
                        CancelIoEx(pipe, &operation);
                        DWORD ignored = 0;
                        GetOverlappedResult(pipe, &operation, &ignored, TRUE);
                        CheckCancelled();
                        throw std::runtime_error(Failure());
                    }
                }
                if (!GetOverlappedResult(pipe, &operation, &count, FALSE))
                    throw std::runtime_error(Failure());
            }
            if (!count)
                throw std::runtime_error(Failure());
            at += count;
        }
    }
    void Finish() {
        pipe.Reset();
        ULONGLONG start = GetTickCount64();
        while (WaitForSingleObject(process, 20) == WAIT_TIMEOUT) {
            CheckCancelled();
            if (GetTickCount64() - start > (streaming ? 30000ull : 3600000ull))
                throw std::runtime_error("Video encoder finalization timed out");
        }
        DWORD code = 1;
        if (!GetExitCodeProcess(process, &code) || code != 0)
            throw std::runtime_error(Failure());
        complete = true;
    }
};
} // namespace suite_clip
#endif
