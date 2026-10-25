#pragma once
#include "RunExperienceModel.h"
#include "PracticeMapData.h"
#include "SuiteUpdateModel.h"
#include <limits>
#include <type_traits>
#include <iomanip>

namespace suite_run
{
// Explicit portable fields, never raw IPC memory or executable replay data.
template <class Data, class Visitor> void VerificationFields(Data &d, Visitor field)
{
    field("runUsedFeatures", d.runUsedFeatures);
    field("runAssisted", d.runAssisted);
    field("practiceModeActive", d.practiceModeActive);
    field("seedKnown", d.seedKnown);
    field("seedKind", d.seedKind);
    field("seedSourcesVerified", d.seedSourcesVerified);
    field("rngPatchComplete", d.rngPatchComplete);
    field("observedFromStart", d.observedFromStart);
    field("runWarnings", d.runWarnings);
    field("gameAssemblyHash", d.gameAssemblyHash);
    field("externalScanStatus", d.externalScanStatus);
    field("externalTool", d.externalTool);
    field("validationDetail", d.validationDetail);
    field("runValidity", d.runValidity);
    field("currentRunTimeSeconds", d.currentRunTimeSeconds);
    field("timerState", d.timerState);
    field("runOutcome", d.runOutcome);
    field("lastRunPb", d.lastRunPb);
    field("previousBestSeconds", d.previousBestSeconds);
    field("reviewRunId", d.reviewRunId);
    field("reviewScene", d.reviewScene);
    field("reviewSpawnSeed", d.reviewSpawnSeed);
    field("reviewSamples", d.reviewSamples);
    field("reviewReady", d.reviewReady);
    field("reviewRetained", d.reviewRetained);
    field("runSeed", d.runSeed);
    field("rngReseedCount", d.rngReseedCount);
    field("initialRngSeed", d.initialRngSeed);
    field("runStartedUtc", d.runStartedUtc);
    field("runEndedUtc", d.runEndedUtc);
    field("runSceneIndex", d.runSceneIndex);
    field("currentSceneIndex", d.currentSceneIndex);
    field("sceneEpoch", d.sceneEpoch);
    field("pid", d.pid);
    field("menuInputOwned", d.menuInputOwned);
    field("cursorLock", d.cursorLock);
    field("trainerFeatures", d.trainerFeatures);
    field("freecamActive", d.freecamActive);
    field("hitboxEnabled", d.hitboxEnabled);
    field("brightnessEnabled", d.brightnessEnabled);
    field("seedActive", d.seedActive);
    field("gameVersion", d.gameVersion);
    field("unityVersion", d.unityVersion);
    field("version", d.version);
}

inline std::string VerificationQuote(std::string_view value)
{
    std::string out = "\"";
    for (unsigned char c : value)
    {
        if (c == '"' || c == '\\')
        {
            out += '\\';
            out += char(c);
        }
        else if (c < 32)
        {
            char escaped[7];
            std::snprintf(escaped, sizeof(escaped), "\\u%04x", unsigned(c));
            out += escaped;
        }
        else
            out += char(c);
    }
    return out + "\"";
}

struct Verification
{
    GameData data{};
    uint32_t session = 0;
    bool full = false;
    std::string trainer = suite_brand::Version;
    std::optional<std::array<CheckState, 7>> states;
    EvidenceRows evidence;
    TechnicalRows technical;

    bool Matches(const practice::MapData &map) const
    {
        if (!map.review)
            return false;
        const auto &r = *map.review;
        return data.reviewRunId == r.id && data.reviewScene == map.scene &&
               data.reviewSpawnSeed == map.seed && data.runOutcome == r.outcome &&
               std::abs(data.currentRunTimeSeconds - r.seconds) <= .000001 &&
               uint32_t(data.runUsedFeatures) == r.aids && uint32_t(data.runWarnings) == r.warnings &&
               bool(data.observedFromStart) == r.observedFromStart && data.seedKind == r.seedKind &&
               bool(data.seedKnown) == r.seedKnown && CapturedText(data.gameAssemblyHash) == map.hash &&
               (data.runValidity != 1 || r.validationChecked && r.valid);
    }

    void Present(Presentation &p, double now, bool filesOk = true, std::string status = {}) const
    {
        p.Reset();
        p.completed = data;
        p.completedSession = session;
        p.replay = p.hasResult = true;
        p.filesOk = filesOk;
        p.fileStatus = std::move(status);
        p.recordedChecks = states;
        p.previousBestKnown = full;
        p.ended = now;
        if (full)
        {
            p.evidence = Evidence(data);
            p.technical = TechnicalEvidence(data, session);
            auto helper = CapturedText(data.version);
            p.technical[4][5] =
                "Helper  " + (helper.empty() ? "UNAVAILABLE" : helper) + " / trainer " + trainer;
            p.evidenceBuilt = true;
        }
        else
        {
            p.evidence = evidence;
            p.technical = technical;
            p.evidenceBuilt = true;
        }
    }
};

inline std::string EncodeVerification(const Verification &v)
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "{\"format\":1,\"session\":" << v.session << ",\"trainer\":" << VerificationQuote(v.trainer);
    VerificationFields(v.data, [&](const char *name, const auto &value) {
        out << ',' << VerificationQuote(name) << ':';
        if constexpr (std::is_array_v<std::remove_reference_t<decltype(value)>>)
            out << VerificationQuote(CapturedText(value));
        else if constexpr (std::is_floating_point_v<std::remove_cvref_t<decltype(value)>>)
            out << std::setprecision(17) << value;
        else
            out << +value;
    });
    return out.str() + "}\n";
}

inline Verification DecodeVerification(std::string_view bytes)
{
    if (bytes.size() > 16384)
        throw std::runtime_error("Run verification snapshot is too large");
    auto json = suite_update::JsonReader(bytes).Read();
    if (json.At("format").UInt() != 1 || json.At("session").kind != suite_update::Json::Kind::Number ||
        json.At("session").text != std::to_string(json.At("session").UInt()) ||
        json.At("session").UInt() > UINT32_MAX)
        throw std::runtime_error("Unsupported run verification snapshot");
    Verification v;
    v.session = uint32_t(json.At("session").UInt());
    v.trainer = json.At("trainer").String();
    if (!suite_update::Version::Parse(v.trainer))
        throw std::runtime_error("Invalid verification trainer version");
    VerificationFields(v.data, [&](const char *name, auto &value) {
        const auto &field = json.At(name);
        using T = std::remove_cvref_t<decltype(value)>;
        if constexpr (std::is_array_v<T>)
        {
            if (field.kind != suite_update::Json::Kind::String || field.text.size() >= sizeof(value))
                throw std::runtime_error("Invalid verification text field");
            std::memcpy(value, field.text.data(), field.text.size());
        }
        else
        {
            if (field.kind != suite_update::Json::Kind::Number)
                throw std::runtime_error("Missing verification value");
            T decoded{};
            if constexpr (std::is_integral_v<T>)
            {
                long long n = 0;
                auto [end, error] =
                    std::from_chars(field.text.data(), field.text.data() + field.text.size(), n);
                if (error != std::errc{} || end != field.text.data() + field.text.size() ||
                    n < std::numeric_limits<T>::lowest() || n > std::numeric_limits<T>::max())
                    throw std::runtime_error("Invalid verification integer");
                decoded = T(n);
            }
            else
            {
                size_t end = 0;
                double n = std::stod(field.text, &end);
                if (end != field.text.size() || !std::isfinite(n) || n < 0 || n > 315360000)
                    throw std::runtime_error("Invalid verification time");
                decoded = T(n);
            }
            value = decoded;
        }
    });
    if (v.data.timerState != 2 || (v.data.runOutcome < 1 || v.data.runOutcome > 5) ||
        v.data.runValidity < 1 || v.data.runValidity > 2)
        throw std::runtime_error("Invalid finished run verification");
    if (v.data.seedKnown > 1 || v.data.runAssisted > 1 || v.data.practiceModeActive > 1 ||
        v.data.seedSourcesVerified > 1 || v.data.rngPatchComplete > 1 || v.data.observedFromStart > 1 ||
        v.data.lastRunPb > 1 || v.data.reviewReady != 1 || v.data.reviewRetained != 1 ||
        v.data.menuInputOwned > 1 || v.data.freecamActive > 1 || v.data.hitboxEnabled > 1 ||
        v.data.brightnessEnabled > 1 || v.data.seedActive > 1 || v.data.externalScanStatus > 2 ||
        v.data.seedKind > 2 || v.data.reviewSamples < 0 || v.data.rngReseedCount < 0)
        throw std::runtime_error("Invalid verification flags");
    v.full = true;
    return v;
}

inline Verification HistoricalVerification(const practice::MapData &map,
                                           const suite_update::Json *report = nullptr)
{
    const auto &r = *map.review;
    Verification v;
    auto &d = v.data;
    d.reviewRunId = r.id;
    d.reviewScene = d.runSceneIndex = d.currentSceneIndex = map.scene;
    d.reviewSpawnSeed = map.seed;
    d.reviewReady = d.reviewRetained = 1;
    d.reviewSamples = int(r.trace.size());
    d.currentRunTimeSeconds = r.seconds;
    d.timerState = 2;
    d.runOutcome = byte(r.outcome);
    d.runValidity = r.valid ? 1 : 2;
    d.runUsedFeatures = int(r.aids);
    d.runAssisted = r.aids != 0;
    d.practiceModeActive = (r.aids & 1u) != 0;
    d.runWarnings = int(r.warnings);
    d.observedFromStart = r.observedFromStart;
    d.seedKnown = r.seedKnown;
    d.seedKind = byte(r.seedKind);
    // This flag records that the original verifier completed its build, RNG
    // and integrity checks. It never describes the current game's process.
    d.seedSourcesVerified = d.rngPatchComplete = r.validationChecked;
    d.externalScanStatus = r.validationChecked ? 1 : 0;
    auto copy = [](auto &target, std::string_view text) {
        std::memcpy(target, text.data(), std::min(text.size(), sizeof(target) - 1));
    };
    copy(d.gameAssemblyHash, map.hash);
    copy(d.validationDetail, r.validityReason);
    if (report)
    {
        copy(d.gameVersion, report->At("gameReportedVersion").String());
        copy(d.unityVersion, report->At("unityVersion").String());
        copy(d.version, report->At("toolVersion").String());
        const auto &seed = report->At("seed");
        if (seed.kind == suite_update::Json::Kind::Number)
            d.runSeed = int(std::stoll(seed.text));
        d.rngReseedCount = int(report->At("reseedsObserved").UInt());
        d.lastRunPb = report->At("localPersonalBest").IsBool(true);
    }
    v.evidence = Evidence(d);
    v.technical = TechnicalEvidence(d, 0);
    v.evidence[5][1] = v.technical[5][1] = "Reported external tool  Not recorded";
    auto checks = Checks(d);
    std::array<CheckState, 7> states;
    for (size_t i = 0; i < 7; i++)
        states[i] = checks[i].state;
    if (!r.validationChecked)
        for (auto &state : states)
            if (state == CheckState::Pass)
                state = CheckState::Unknown;
    v.states = states;
    auto missing = [&](int row, int column, const char *label) {
        v.technical[row][column] = std::string(label) + "  Not recorded";
    };
    missing(0, 3, "Finish feature mask");
    missing(0, 4, "Finish freecam / hitboxes");
    missing(0, 5, "Finish fullbright / SET");
    missing(1, 3, "Initial RNG seed");
    missing(2, 3, "Start UTC ticks");
    missing(2, 4, "End UTC ticks");
    missing(2, 5, "Scene epoch");
    missing(3, 3, "UTC span");
    missing(5, 3, "Finish monitor status code");
    missing(6, 3, "PID / session");
    missing(6, 4, "Input owner / cursor lock");
    missing(6, 5, "IPC snapshot");
    if (!report)
    {
        v.evidence[1][0] = v.technical[1][0] = "Run RNG seed  Not recorded";
        v.evidence[1][2] = v.technical[1][2] = "RNG reseeds  Not recorded";
        missing(4, 5, "Helper / trainer version");
    }
    else
        v.technical[4][5] = "Recorded helper  " + CapturedText(d.version);
    return v;
}
} // namespace suite_run
