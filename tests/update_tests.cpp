#include "SuiteUpdater.h"
#include <cassert>
#include <iostream>
#include <random>

using namespace suite_update;

template <class Action> void Reject(Action action)
{
    bool rejected = false;
    try
    {
        action();
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    assert(rejected);
}

int main(int argc, char **argv)
{
    assert(ValidToken("github_pat_example_123"));
    assert(ValidToken("ghs_12345_eyJhbGciOiJIUzI1NiJ9.a-b_c.signature"));
    assert(ValidToken("abc+/~=="));
    assert(ValidToken(std::string(TokenLimit, 'a')));
    for (auto bad : {"", "abc def", "abc\r\nAuthorization: x", "abc\t", "abc=def", "="})
        assert(!ValidToken(bad));
    assert(!ValidToken(std::string(TokenLimit + 1, 'a')));
    assert(!ValidToken(std::string("abc\0def", 7)));
    assert(*Version::Parse("v7.12.10") > *Version::Parse("7.12.9"));
    assert(*Version::Parse("8.0.0") > *Version::Parse("7.999.999"));
    for (auto text : {"", "7.12", "7.12.1.0", "7.12.1-beta", "7.12.1+build", "7.012.1", "-7.12.1",
                      "7.12.1000000", " 7.12.1", "7.12.1\n", "V7.12.1"})
        assert(!Version::Parse(text));
    auto json =
        JsonReader(
            R"({"id":18446744073709551615,"label":"caf\u00e9 \ud83d\ude80","empty":[],"nested":{"a":false},"fraction":1.2e+4})")
            .Read();
    assert(json.At("id").UInt() == UINT64_MAX);
    assert(json.At("label").String() == "caf\xc3\xa9 \xf0\x9f\x9a\x80");
    assert(json.At("nested").At("a").IsBool(false));
    assert(!json.At("nested").IsBool(false));
    assert(json.At("fraction").UInt() == 0);
    assert(JsonReader(Quote("quotes\" and \\ newline\n\x01")).Read().String() ==
           "quotes\" and \\ newline\n\x01");
    for (auto malformed :
         {"[]extra", "[1,]", "{\"a\":1,\"a\":2}", "[01]", "[-]", "[1.]", "[1e]", "[NaN]", "[tru]", "{\"x\":}",
          "\"\\ud800\"", "\"\\udc00\"", "\"\\ud800\\u1234\"", "\"\\q\"", "\"\n\""})
        Reject([&] { JsonReader(malformed).Read(); });
    Reject([] { JsonReader(std::string(100, '[') + std::string(100, ']')).Read(); });
    Reject([] { JsonReader(std::string(4 * 1024 * 1024 + 1, ' ')).Read(); });
    const std::string asset =
        R"({"id":662634595,"name":"ItStealsSuite-Portable.zip","size":16795293,"state":"uploaded","digest":"sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"})";
    auto row = [&](const char *version, const char *draft = "false", const char *preview = "false") {
        return std::string("{\"tag_name\":\"") + version + "\",\"draft\":" + draft +
               ",\"prerelease\":" + preview +
               ",\"published_at\":\"2026-10-21T13:39:00Z\",\"body\":\"Full notes\\n- Fixed a bug.\",\"assets\":[" +
               asset + "]}";
    };
    auto releases =
        Releases("[" + row("v7.12.9") + "," + row("v7.12.10") + "," + row("v8.0.0", "true") + "," +
                 row("v9.0.0", "false", "true") + "," + row("v7.12.10") + "," + row("v7.12.11-beta") + "]");
    assert(releases.size() == 2 && releases[0].version == "7.12.10");
    assert(releases[0].Installable() && releases[0].date == "2026-10-21");
    assert(Newer(releases[0], "7.12.9") && !Newer(releases[1], "7.12.10"));
    auto bad = releases[0];
    bad.sha256.clear();
    assert(!bad.Installable());
    bad = releases[0];
    bad.size = PackageLimit + 1;
    assert(!bad.Installable());
    bad = releases[0];
    bad.asset = 0;
    assert(!bad.Installable());
    Reject([] { Releases("{}"); });
    auto duplicate = row("v7.12.1");
    duplicate.insert(duplicate.size() - 2, "," + asset);
    Reject([&] { Releases("[" + duplicate + "]"); });
    auto history = Changelog(
        "# Changelog\n\n### v7.12.1 Changelog\r\n\n- Added updater.\n\n### v7.12.0 Changelog\n- Prior changes.\n");
    assert(history.size() == 2 && history[0].version == "7.12.1" &&
           history[0].notes.find("Added updater.") != std::string::npos &&
           history[1].notes.find("Prior changes.") != std::string::npos);
    auto page = Changelog(
        "![Suite banner](banner.svg)\n\n[Download Setup](setup.exe)\n\n"
        "### v7.12.2 Changelog\r\n\r\n- Improved the interface.\r\n\r\n"
        "### Install\nClose the game.\n<details>\n| Download | File |\n</details>\n");
    assert(page.size() == 1 && page[0].version == "7.12.2" &&
           page[0].notes == "\n- Improved the interface.\n\n");

    Presentation p;
    assert(!p.Consider("7.12.1", "101:111", true, false, false, true, 0)); // intro
    assert(!p.Consider("7.12.1", "101:111", true, true, true, true, 1));   // run/export
    assert(!p.Consider("7.12.1", "101:111", false, true, false, true, 1)); // hidden
    assert(p.Consider("7.12.1", "101:111", true, true, false, true, 2) && p.changelog && !p.prompt);
    p.seenVersion = "7.12.1";
    p.changelog = false;
    assert(p.Consider("7.12.1", "101:111", true, true, false, true, 3) && p.prompt);
    p.prompt = false;
    for (int scene = 0; scene < 30; scene++)
        assert(!p.Consider("7.12.1", "101:111", true, true, false, true, 4 + scene));
    assert(p.Consider("7.12.1", "101:112", true, true, false, true, 40)); // PID reused, new process
    p.prompt = false;
    p.promptedSession.clear(); // New trainer instance may prompt again.
    assert(p.Consider("7.12.1", "101:112", true, true, false, true, 41));
    p.prompt = false;
    assert(!p.Consider("7.12.1", "", true, true, false, true, 42));
    assert(!p.Consider("7.12.1", "102:113", true, true, false, false, 43));
    assert(p.Consider("7.12.2", "102:113", true, true, false, false, 44) && p.changelog);

    Presentation pending;
    pending.seenVersion = "7.12.1";
    assert(pending.Consider("7.12.1", "200:111", true, true, false, true, 1));
    assert(pending.prompt);
    assert(!pending.Consider("7.12.1", "", false, true, false, true, 2) && !pending.prompt);
    assert(pending.Consider("7.12.1", "201:112", true, true, false, true, 3));
    pending.prompt = false;
    assert(!pending.Consider("7.12.1", "201:112", true, true, false, true, 4));

    if (argc > 1)
    {
        fs::path directory = argv[1];
        fs::create_directories(directory);
        {
            Service first;
            first.Init(directory);
            first.autoCheck = false;
            first.presentation.seenVersion = suite_brand::Version;
            first.presentation.promptedSession = "101:111";
            assert(first.SaveState());
        }
        Service restarted;
        restarted.Init(directory);
        assert(!restarted.autoCheck && restarted.presentation.seenVersion == suite_brand::Version);
        assert(restarted.presentation.promptedSession.empty());
        assert(restarted.presentation.Consider(suite_brand::Version, "101:111", true, true, false, true, 0));
    }
    std::mt19937 random(42);
    for (int i = 0; i < 5000; i++)
    {
        std::string bytes(size_t(random() % 1024), ' ');
        for (auto &c : bytes)
            c = char(random() & 255);
        try
        {
            JsonReader(bytes).Read();
        }
        catch (const std::exception &)
        {
        }
    }
    std::cout
        << "Release metadata, versions, bounded JSON, package identity, complete notes, intro/run deferral and session prompts: PASS\n";
}
