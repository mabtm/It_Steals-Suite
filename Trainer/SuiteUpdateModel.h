#pragma once
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace suite_update
{
inline constexpr char Repository[] = "mabtm/It_Steals-Suite";
inline constexpr char ApiRoot[] = "https://api.github.com/repos/mabtm/It_Steals-Suite";
inline constexpr char RepositoryUrl[] = "https://github.com/mabtm/It_Steals-Suite";
inline constexpr char PackageName[] = "ItStealsSuite-Portable.zip";
inline constexpr uint64_t PackageLimit = 128ull * 1024 * 1024;

// HTTP bearer tokens are opaque. GitHub also issues JWT-based installation
// tokens; validation prevents header injection without assuming an old format.
inline constexpr size_t TokenLimit = 2560;

inline bool ValidToken(std::string_view token)
{
    if (token.empty() || token.size() > TokenLimit)
        return false;
    bool padding = false, base = false;
    for (unsigned char c : token)
    {
        if (c == '=')
        {
            padding = true;
            continue;
        }
        if (padding || !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                         c == '-' || c == '.' || c == '_' || c == '~' || c == '+' || c == '/'))
            return false;
        base = true;
    }
    return base;
}

struct Version
{
    std::array<unsigned, 3> parts{};
    auto operator<=>(const Version &) const = default;

    static std::optional<Version> Parse(std::string_view text)
    {
        if (text.starts_with('v'))
            text.remove_prefix(1);
        Version result;
        for (size_t i = 0; i < 3; i++)
        {
            auto end = text.find('.');
            auto part = text.substr(0, end);
            if (part.empty() || part.size() > 6 || (part.size() > 1 && part[0] == '0'))
                return {};
            auto [at, error] = std::from_chars(part.data(), part.data() + part.size(), result.parts[i]);
            if (error != std::errc{} || at != part.data() + part.size() || result.parts[i] > 999999)
                return {};
            if (i == 2)
                return end == std::string_view::npos ? std::optional(result) : std::nullopt;
            if (end == std::string_view::npos)
                return {};
            text.remove_prefix(end + 1);
        }
        return {};
    }
};

// Bounded JSON for GitHub metadata. Duplicate keys, trailing bytes, invalid
// escapes and unpaired UTF-16 surrogates are rejected before selecting assets.
struct Json
{
    enum class Kind
    {
        Null,
        String,
        Number,
        Boolean,
        Object,
        Array
    } kind = Kind::Null;
    std::string text;
    std::map<std::string, Json, std::less<>> fields;
    std::vector<Json> items;

    const Json &At(std::string_view name) const
    {
        static const Json empty;
        auto i = fields.find(name);
        return i == fields.end() ? empty : i->second;
    }

    std::string String() const
    {
        return kind == Kind::String ? text : "";
    }

    bool IsBool(bool value) const
    {
        return kind == Kind::Boolean && text == (value ? "true" : "false");
    }

    uint64_t UInt() const
    {
        uint64_t n = 0;
        auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), n);
        return kind == Kind::Number && error == std::errc{} && end == text.data() + text.size() ? n : 0;
    }
};

class JsonReader
{
    std::string_view source;
    size_t at = 0, nodes = 0;

    [[noreturn]] static void Bad()
    {
        throw std::runtime_error("GitHub returned invalid release metadata.");
    }

    void Space()
    {
        while (at < source.size() &&
               (source[at] == ' ' || source[at] == '\r' || source[at] == '\n' || source[at] == '\t'))
            at++;
    }

    bool Take(char c)
    {
        Space();
        if (at == source.size() || source[at] != c)
            return false;
        at++;
        return true;
    }

    unsigned Hex()
    {
        unsigned n = 0;
        for (int i = 0; i < 4; i++)
        {
            if (at == source.size())
                Bad();
            char c = source[at++];
            int digit = c >= '0' && c <= '9'   ? c - '0'
                        : c >= 'a' && c <= 'f' ? c - 'a' + 10
                        : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                               : -1;
            if (digit < 0)
                Bad();
            n = n * 16 + unsigned(digit);
        }
        return n;
    }

    static void Utf8(std::string &out, unsigned n)
    {
        if (n < 0x80)
            out += char(n);
        else if (n < 0x800)
        {
            out += char(0xc0 | (n >> 6));
            out += char(0x80 | (n & 63));
        }
        else if (n < 0x10000)
        {
            out += char(0xe0 | (n >> 12));
            out += char(0x80 | ((n >> 6) & 63));
            out += char(0x80 | (n & 63));
        }
        else
        {
            out += char(0xf0 | (n >> 18));
            out += char(0x80 | ((n >> 12) & 63));
            out += char(0x80 | ((n >> 6) & 63));
            out += char(0x80 | (n & 63));
        }
    }

    std::string String()
    {
        if (!Take('"'))
            Bad();
        std::string out;
        while (at < source.size())
        {
            unsigned char c = source[at++];
            if (c == '"')
                return out;
            if (c < 32)
                Bad();
            if (c != '\\')
            {
                out += char(c);
                continue;
            }
            if (at == source.size())
                Bad();
            switch (source[at++])
            {
            case '"':
                out += '"';
                break;
            case '\\':
                out += '\\';
                break;
            case '/':
                out += '/';
                break;
            case 'b':
                out += '\b';
                break;
            case 'f':
                out += '\f';
                break;
            case 'n':
                out += '\n';
                break;
            case 'r':
                out += '\r';
                break;
            case 't':
                out += '\t';
                break;
            case 'u': {
                unsigned n = Hex();
                if (n >= 0xd800 && n <= 0xdbff)
                {
                    if (at + 2 > source.size() || source.substr(at, 2) != "\\u")
                        Bad();
                    at += 2;
                    unsigned low = Hex();
                    if (low < 0xdc00 || low > 0xdfff)
                        Bad();
                    n = 0x10000 + ((n - 0xd800) << 10) + low - 0xdc00;
                }
                else if (n >= 0xdc00 && n <= 0xdfff)
                    Bad();
                Utf8(out, n);
                break;
            }
            default:
                Bad();
            }
        }
        Bad();
    }

    Json Value(unsigned depth)
    {
        if (depth > 32 || ++nodes > 200000)
            Bad();
        Space();
        if (at == source.size())
            Bad();
        Json out;
        if (source[at] == '"')
        {
            out.kind = Json::Kind::String;
            out.text = String();
            return out;
        }
        if (Take('{'))
        {
            out.kind = Json::Kind::Object;
            if (Take('}'))
                return out;
            do
            {
                auto name = String();
                if (!Take(':'))
                    Bad();
                auto value = Value(depth + 1);
                if (!out.fields.emplace(std::move(name), std::move(value)).second)
                    Bad();
                if (Take('}'))
                    return out;
            } while (Take(','));
            Bad();
        }
        if (Take('['))
        {
            out.kind = Json::Kind::Array;
            if (Take(']'))
                return out;
            do
            {
                out.items.push_back(Value(depth + 1));
                if (Take(']'))
                    return out;
            } while (Take(','));
            Bad();
        }
        for (auto word : {std::string_view("true"), std::string_view("false"), std::string_view("null")})
            if (source.substr(at, word.size()) == word)
            {
                at += word.size();
                out.kind = word == "null" ? Json::Kind::Null : Json::Kind::Boolean;
                out.text = word;
                return out;
            }
        size_t begin = at;
        if (source[at] == '-')
            at++;
        auto digit = [&] { return at < source.size() && source[at] >= '0' && source[at] <= '9'; };
        if (!digit())
            Bad();
        if (source[at] == '0')
            at++;
        else
            while (digit())
                at++;
        if (at < source.size() && source[at] == '.')
        {
            at++;
            if (!digit())
                Bad();
            while (digit())
                at++;
        }
        if (at < source.size() && (source[at] == 'e' || source[at] == 'E'))
        {
            at++;
            if (at < source.size() && (source[at] == '+' || source[at] == '-'))
                at++;
            if (!digit())
                Bad();
            while (digit())
                at++;
        }
        out.kind = Json::Kind::Number;
        out.text = source.substr(begin, at - begin);
        return out;
    }

  public:
    explicit JsonReader(std::string_view text) : source(text) {}

    Json Read()
    {
        if (source.size() > 4 * 1024 * 1024)
            Bad();
        auto result = Value(0);
        Space();
        if (at != source.size())
            Bad();
        return result;
    }
};

inline std::string Quote(std::string_view text)
{
    std::string out = "\"";
    constexpr char digits[] = "0123456789abcdef";
    for (unsigned char c : text)
        if (c == '"' || c == '\\')
        {
            out += '\\';
            out += char(c);
        }
        else if (c < 32)
        {
            out += "\\u00";
            out += digits[c >> 4];
            out += digits[c & 15];
        }
        else
            out += char(c);
    return out + '"';
}

inline bool ValidHash(std::string_view value)
{
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}

struct Release
{
    std::string version, notes, date, sha256;
    uint64_t asset = 0, size = 0;

    bool Installable() const
    {
        return asset && size > 0 && size <= PackageLimit && ValidHash(sha256);
    }
};

inline std::vector<Release> Releases(std::string_view bytes)
{
    auto json = JsonReader(bytes).Read();
    if (json.kind != Json::Kind::Array || json.items.size() > 100)
        throw std::runtime_error("GitHub returned an unexpected release list.");
    std::vector<Release> result;
    for (const auto &row : json.items)
    {
        auto version = row.At("tag_name").String();
        if (!Version::Parse(version) || !row.At("draft").IsBool(false) || !row.At("prerelease").IsBool(false))
            continue;
        if (version[0] == 'v')
            version.erase(0, 1);
        Release release;
        release.version = version;
        release.notes = row.At("body").String();
        if (release.notes.size() > 131072)
            throw std::runtime_error("Release notes exceed the supported size.");
        release.date = row.At("published_at").String().substr(0, 10);
        for (const auto &asset : row.At("assets").items)
            if (asset.At("name").String() == PackageName && asset.At("state").String() == "uploaded")
            {
                if (release.asset)
                    throw std::runtime_error("Release contains duplicate update packages.");
                release.asset = asset.At("id").UInt();
                release.size = asset.At("size").UInt();
                auto digest = asset.At("digest").String();
                if (digest.starts_with("sha256:"))
                    release.sha256 = digest.substr(7);
            }
        if (std::none_of(result.begin(), result.end(), [&](const auto &r) { return r.version == version; }))
            result.push_back(std::move(release));
    }
    std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
        return Version::Parse(a.version) > Version::Parse(b.version);
    });
    return result;
}

inline std::vector<Release> Changelog(std::string_view text)
{
    std::vector<Release> result;
    bool collecting = false;
    for (size_t at = 0; at < text.size();)
    {
        auto next = text.find('\n', at);
        auto line = text.substr(at, next == std::string_view::npos ? next : next - at);
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
        if (line.starts_with("### v") && line.ends_with(" Changelog"))
        {
            collecting = false;
            auto v = line.substr(5, line.size() - 15);
            if (Version::Parse(v))
            {
                Release r;
                r.version = v;
                result.push_back(std::move(r));
                collecting = true;
            }
        }
        else if (line.starts_with("#"))
            collecting = false;
        else if (collecting)
        {
            result.back().notes += line;
            result.back().notes += '\n';
        }
        if (next == std::string_view::npos)
            break;
        at = next + 1;
    }
    return result;
}

enum class Phase
{
    Idle,
    Checking,
    Current,
    Available,
    Downloading,
    Ready,
    Installing,
    Error
};

inline bool Busy(Phase phase)
{
    return phase == Phase::Checking || phase == Phase::Downloading || phase == Phase::Installing;
}

inline bool Newer(const Release &r, std::string_view current)
{
    auto installed = Version::Parse(current), remote = Version::Parse(r.version);
    return installed && remote && *remote > *installed;
}

struct Presentation
{
    std::string seenVersion, promptedSession, observedSession;
    bool changelog = false, prompt = false, installRequested = false;
    double appeared = 0;

    bool Consider(std::string_view current, std::string_view session, bool ready, bool introDone,
                  bool blocked, bool available, double now)
    {
        if (observedSession != session)
        {
            observedSession = session;
            prompt = false; // A queued prompt belongs only to its game process.
        }
        if (!ready || !introDone || blocked || changelog || prompt)
            return false;
        if (seenVersion != current)
        {
            changelog = true;
            appeared = now;
            return true;
        }
        if (available && !session.empty() && promptedSession != session)
        {
            promptedSession = session;
            prompt = true;
            appeared = now;
            return true;
        }
        return false;
    }
};
} // namespace suite_update
