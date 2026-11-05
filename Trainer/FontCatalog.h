#pragma once
#include <algorithm>
#include <atomic>
#include <cctype>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace suite_fonts
{
inline constexpr size_t MaximumBytes = 64 * 1024 * 1024;
struct ScanLimits
{
    size_t entries = 65536, files = 16384, faces = 65536;
    uint64_t bytes = uint64_t{2} * 1024 * 1024 * 1024;
};

struct ScanBudget
{
    ScanLimits limits;
    size_t entries = 0, files = 0;
    uint64_t bytes = 0;
    std::function<bool()> canceled;

    void Check() const
    {
        if (canceled && canceled())
            throw std::runtime_error("Font refresh canceled.");
    }
};
struct Face
{
    std::string id, name, source;
    std::filesystem::path path;
    int index = 0;
};

inline std::string Utf8(const std::filesystem::path &path)
{
    auto text = path.u8string();
    return {reinterpret_cast<const char *>(text.data()), text.size()};
}

inline std::string Fold(std::string text)
{
    for (char &c : text)
        if (static_cast<unsigned char>(c) < 128)
            c = char(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

inline bool Matches(const Face &face, const std::string &query)
{
    return Fold(face.name + " " + face.source + " " + Utf8(face.path.filename())).find(Fold(query)) !=
           std::string::npos;
}

inline bool ValidId(const std::string &id)
{
    if (id.size() > 2048)
        return false;
    for (unsigned char c : id)
        if (c < 32 || c == 127)
            return false;
    return true;
}

inline std::vector<unsigned char> Read(const std::filesystem::path &path)
{
    std::error_code error;
    auto size = std::filesystem::file_size(path, error);
    if (error || size < 12 || size > MaximumBytes)
        return {};
    std::ifstream input(path, std::ios::binary);
    std::vector<unsigned char> bytes(static_cast<size_t>(size));
    if (!input.read(reinterpret_cast<char *>(bytes.data()), std::streamsize(bytes.size())))
        return {};
    return bytes;
}

inline uint32_t U32(const std::vector<unsigned char> &b, size_t at)
{
    return uint32_t(b[at]) << 24 | uint32_t(b[at + 1]) << 16 | uint32_t(b[at + 2]) << 8 | b[at + 3];
}
inline uint16_t U16(const std::vector<unsigned char> &b, size_t at)
{
    return uint16_t(b[at]) << 8 | b[at + 1];
}

// Validate table bounds before a file is offered to the font renderer. Bitmap
// fonts and unsupported containers are omitted; TrueType/OpenType collections
// expose each face separately instead of silently using the first face.
inline std::vector<size_t> Offsets(const std::vector<unsigned char> &b)
{
    std::vector<size_t> offsets;
    if (b.size() < 12)
        return offsets;
    if (U32(b, 0) == 0x74746366) // ttcf
    {
        uint32_t count = U32(b, 8);
        if (!count || count > 256 || count > (b.size() - 12) / 4)
            return {};
        for (uint32_t i = 0; i < count; ++i)
            offsets.push_back(U32(b, 12 + i * 4));
    }
    else
        offsets.push_back(0);
    for (size_t at : offsets)
    {
        if (at > b.size() - 12)
            return {};
        auto version = U32(b, at);
        if (version != 0x00010000 && version != 0x4f54544f && version != 0x74727565)
            return {};
        size_t count = U16(b, at + 4);
        if (!count || count > (b.size() - at - 12) / 16)
            return {};
        bool cmap = false, head = false, hhea = false, hmtx = false, maxp = false, outline = false;
        for (size_t i = 0; i < count; ++i)
        {
            size_t table = at + 12 + i * 16;
            uint32_t offset = U32(b, table + 8), length = U32(b, table + 12);
            if (offset > b.size() || length > b.size() - offset)
                return {};
            auto tag = U32(b, table);
            cmap |= tag == 0x636d6170 && length >= 4;
            head |= tag == 0x68656164 && length >= 54;
            hhea |= tag == 0x68686561 && length >= 36;
            hmtx |= tag == 0x686d7478 && length >= 4;
            maxp |= tag == 0x6d617870 && length >= 6;
            outline |= tag == 0x676c7966 || tag == 0x43464620;
        }
        if (!(cmap && head && hhea && hmtx && maxp && outline))
            return {};
    }
    return offsets;
}

inline void AppendUtf8(std::string &text, uint32_t cp)
{
    if (cp < 0x80)
        text += char(cp);
    else if (cp < 0x800)
    {
        text += char(0xc0 | (cp >> 6));
        text += char(0x80 | (cp & 63));
    }
    else if (cp < 0x10000)
    {
        text += char(0xe0 | (cp >> 12));
        text += char(0x80 | ((cp >> 6) & 63));
        text += char(0x80 | (cp & 63));
    }
    else
    {
        text += char(0xf0 | (cp >> 18));
        text += char(0x80 | ((cp >> 12) & 63));
        text += char(0x80 | ((cp >> 6) & 63));
        text += char(0x80 | (cp & 63));
    }
}

inline std::string Name(const std::vector<unsigned char> &b, size_t face)
{
    for (size_t i = 0; i < U16(b, face + 4); ++i)
    {
        size_t table = face + 12 + i * 16;
        if (U32(b, table) != 0x6e616d65)
            continue;
        size_t at = U32(b, table + 8), length = U32(b, table + 12);
        if (length < 6)
            return {};
        size_t count = U16(b, at + 2), storage = U16(b, at + 4);
        if (count > (length - 6) / 12 || storage > length)
            return {};
        std::string result;
        int best = -1;
        for (size_t n = 0; n < count; ++n)
        {
            size_t record = at + 6 + n * 12;
            int platform = U16(b, record), language = U16(b, record + 4), name = U16(b, record + 6);
            size_t size = U16(b, record + 8), start = U16(b, record + 10);
            if (name != 4 || (platform != 0 && platform != 3) || size % 2 ||
                start > length - storage || size > length - storage - start)
                continue;
            int score = (platform == 3 ? 2 : 0) + (language == 0x409 ? 4 : 0);
            if (score <= best)
                continue;
            std::string text;
            for (size_t j = 0; j < size; j += 2)
            {
                uint32_t cp = U16(b, at + storage + start + j);
                if (cp >= 0xd800 && cp <= 0xdbff && j + 2 < size)
                {
                    auto low = U16(b, at + storage + start + j + 2);
                    if (low >= 0xdc00 && low <= 0xdfff)
                    {
                        cp = 0x10000 + ((cp - 0xd800) << 10) + low - 0xdc00;
                        j += 2;
                    }
                }
                if (cp >= 32 && cp != 127 && !(cp >= 0xd800 && cp <= 0xdfff))
                    AppendUtf8(text, cp);
            }
            if (!text.empty())
            {
                result = std::move(text);
                best = score;
            }
        }
        return result;
    }
    return {};
}

struct Catalog
{
    std::vector<Face> faces;
    bool scanned = false;
    uint64_t revision = 0;
    void AddDirectory(const std::filesystem::path &directory, const std::string &source,
                      ScanBudget *budget = nullptr)
    {
        std::error_code error;
        for (std::filesystem::directory_iterator it(directory, error), end; it != end && !error; it.increment(error))
        {
            if (budget)
            {
                budget->Check();
                if (++budget->entries > budget->limits.entries)
                    throw std::runtime_error("The font folders contain too many files to refresh.");
            }
            auto extension = Fold(Utf8(it->path().extension()));
            if (extension != ".ttf" && extension != ".otf" && extension != ".ttc")
                continue;
            if (budget)
            {
                if (++budget->files > budget->limits.files)
                    throw std::runtime_error("The installed font collection is too large to refresh.");
                std::error_code sizeError;
                auto size = std::filesystem::file_size(it->path(), sizeError);
                if (sizeError || size < 12 || size > MaximumBytes)
                    continue;
                if (budget->bytes > budget->limits.bytes || size > budget->limits.bytes - budget->bytes)
                    throw std::runtime_error("The installed font collection is too large to refresh.");
                budget->bytes += size;
            }
            auto bytes = Read(it->path());
            auto offsets = Offsets(bytes);
            for (size_t face = 0; face < offsets.size(); ++face)
            {
                if (budget)
                {
                    budget->Check();
                    if (faces.size() >= budget->limits.faces)
                        throw std::runtime_error("The installed font collection has too many font faces to refresh.");
                }
                auto name = Name(bytes, offsets[face]);
                if (name.empty())
                    name = Utf8(it->path().stem()) + (offsets.size() > 1 ? " #" + std::to_string(face + 1) : "");
                faces.push_back({source + ":" + Utf8(it->path().filename()) + "#" + std::to_string(face),
                                 name, source, it->path(), int(face)});
            }
        }
    }
    void Sort()
    {
        std::sort(faces.begin(), faces.end(), [](const Face &a, const Face &b) {
            auto first = Fold(a.name), second = Fold(b.name);
            return first != second ? first < second : a.id < b.id;
        });
        scanned = true;
        ++revision;
    }
    const Face *Find(const std::string &id) const
    {
        for (const auto &face : faces)
            if (face.id == id)
                return &face;
        return nullptr;
    }
};
inline Catalog catalog;

struct FontDirectory
{
    std::filesystem::path path;
    std::string source;
};

inline std::vector<FontDirectory> InstalledDirectories()
{
    std::vector<FontDirectory> result;
#ifdef _WIN32
    wchar_t directory[32768]{};
    auto count = GetWindowsDirectoryW(directory, 32768);
    if (count && count < 32768)
        result.push_back({std::filesystem::path(directory) / L"Fonts", "Windows"});
    count = GetEnvironmentVariableW(L"LOCALAPPDATA", directory, 32768);
    if (count && count < 32768)
        result.push_back({std::filesystem::path(directory) / L"Microsoft/Windows/Fonts", "User"});
    count = GetModuleFileNameW(nullptr, directory, 32768);
    if (count && count < 32768)
        result.push_back({std::filesystem::path(directory).parent_path() / L"Fonts", "Suite"});
#endif
    return result;
}

inline Catalog ScanDirectories(const std::vector<FontDirectory> &directories, ScanBudget &budget)
{
    Catalog result;
    // Production has three locations. Keep custom scan requests bounded too.
    if (directories.size() > 16)
        throw std::runtime_error("Too many font folders were requested.");
    for (const auto &directory : directories)
    {
        budget.Check();
        result.AddDirectory(directory.path, directory.source, &budget);
    }
    budget.Check();
    result.Sort();
    return result;
}

// Synchronous discovery is retained for command-line validation. Production
// menu/startup code uses ScanService and publishes only from its UI thread.
inline void RefreshInstalled()
{
    ScanBudget budget;
    auto next = ScanDirectories(InstalledDirectories(), budget);
    next.revision = catalog.revision + 1;
    catalog = std::move(next);
}

enum class ScanPhase { Idle, Pending, Ready, Failed };
struct ScanStatus
{
    ScanPhase phase = ScanPhase::Idle;
    std::string error;
};

class ScanService
{
    struct Job
    {
        bool installed = true;
        std::vector<FontDirectory> directories;
        ScanLimits limits;
    };
    struct Result
    {
        Catalog catalog;
        std::string error;
    };

    std::mutex gate;
    std::condition_variable wake;
    std::thread worker;
    std::atomic_bool stopping{false};
    std::optional<Job> job;
    std::optional<Result> completed;
    ScanStatus state;

    void Work()
    {
        for (;;)
        {
            Job request;
            {
                std::unique_lock lock(gate);
                wake.wait(lock, [&] { return stopping.load() || job.has_value(); });
                if (stopping.load())
                    return;
                request = std::move(*job);
                job.reset();
            }
            Result result;
            try
            {
                ScanBudget budget{request.limits};
                budget.canceled = [&] { return stopping.load(); };
                result.catalog = ScanDirectories(request.installed ? InstalledDirectories()
                                                                   : request.directories, budget);
            }
            catch (const std::exception &error)
            {
                result.error = error.what();
            }
            std::lock_guard lock(gate);
            completed = std::move(result);
        }
    }

    bool Begin(Job request)
    {
        std::lock_guard lock(gate);
        // Repeated requests coalesce into the current scan, including the
        // completed result awaiting publication. No file work happens here.
        if (stopping.load() || state.phase == ScanPhase::Pending)
            return false;
        state = {ScanPhase::Pending, {}};
        completed.reset();
        try
        {
            if (!worker.joinable())
                worker = std::thread(&ScanService::Work, this);
            job = std::move(request);
            wake.notify_one();
            return true;
        }
        catch (const std::exception &error)
        {
            state = {ScanPhase::Failed, error.what()};
            return false;
        }
    }

  public:
    ScanService() = default;
    ScanService(const ScanService &) = delete;
    ~ScanService() { Shutdown(); }

    bool Refresh() { return Begin({}); }

    bool RefreshDirectories(std::vector<FontDirectory> directories, ScanLimits limits = {})
    {
        return Begin({false, std::move(directories), limits});
    }

    ScanStatus Status()
    {
        std::lock_guard lock(gate);
        return state;
    }

    // Call only on the UI thread. The worker owns a separate local catalog;
    // readers of the published catalog never race a vector mutation.
    bool Poll(Catalog &target = catalog)
    {
        std::lock_guard lock(gate);
        if (!completed)
            return false;
        auto result = std::move(*completed);
        completed.reset();
        if (!result.error.empty())
        {
            state = {ScanPhase::Failed, std::move(result.error)};
            return false;
        }
        result.catalog.revision = target.revision + 1;
        target = std::move(result.catalog);
        state = {ScanPhase::Ready, {}};
        return true;
    }

    void Shutdown()
    {
        {
            std::lock_guard lock(gate);
            stopping = true;
        }
        wake.notify_one();
        if (worker.joinable())
            worker.join();
    }
};

inline ScanService scan;
} // namespace suite_fonts
