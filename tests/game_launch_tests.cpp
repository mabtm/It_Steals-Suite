#include "../Trainer/GameLaunch.h"
#include "../Trainer/TrainerPreferences.h"
#include <cassert>
#include <iostream>

namespace fs = std::filesystem;
using namespace game_launch;

static void Write(const fs::path &path, const std::string &text)
{
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file.write(text.data(), text.size());
}

static void Game(const fs::path &folder)
{
    std::string image(128, '\0');
    image[0] = 'M';
    image[1] = 'Z';
    image[60] = 64;
    image[64] = 'P';
    image[65] = 'E';
    image[68] = 0x64;
    image[69] = static_cast<char>(0x86);
    Write(folder / "It Steals.exe", image);
    Write(folder / "It Steals_Data/Managed/Assembly-CSharp.dll", "test marker");
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    const auto root = fs::absolute(argv[1]) / "launcher";
    fs::create_directories(root);
    StartupOnce once;
    assert(once.Plan(true, false, false));
    assert(!once.Plan(true, false, false));
    StartupOnce disabled, running, suppressed;
    assert(!disabled.Plan(false, false, false));
    assert(!running.Plan(true, true, false));
    assert(!suppressed.Plan(true, false, true));
    assert(TrainerPreferences::Decode("version=18\n").autoStartGame);
    assert(!TrainerPreferences::Decode("version=18\nautoStartGame=0\n").autoStartGame);
    assert(TrainerPreferences::Decode("autoStartGame=2\n").autoStartGame);
    TrainerPreferences preferences;
    preferences.autoStartGame = false;
    assert(!TrainerPreferences::Decode(preferences.Encode()).autoStartGame);

    const Location direct{"C:/Games/It Steals.exe", {}}, steamStart{direct.game, "C:/Steam/Steam.exe"};
    const std::wstring skip = L" --practice-suite-skip-intro --melonloader.disablestartscreen";
    assert(CommandLine(direct, false) == L"\"C:/Games/It Steals.exe\"");
    assert(CommandLine(direct, true) == CommandLine(direct, false) + skip);
    assert(CommandLine(steamStart, false) == L"\"C:/Steam/Steam.exe\" -applaunch 1349060");
    assert(CommandLine(steamStart, true) == CommandLine(steamStart, false) + skip);

    assert(KeyValues("\"root\" { \"a\" \"b\" } // comment\n").Parse());
    assert(!KeyValues("\"root\" { \"a\"").Parse());
    assert(!KeyValues(std::string(MetadataLimit + 1, ' ')).Parse());
    assert(!KeyValues(
                "\"a\" { \"b\" { \"c\" { \"d\" { \"e\" { \"f\" { \"g\" { \"h\" { \"i\" { } } } } } } } } }")
                .Parse());
    uint32_t random = 0x7120;
    for (int i = 0; i < 2000; ++i)
    {
        std::string fuzz;
        for (int j = 0; j < i % 137; ++j)
        {
            random = random * 1664525 + 1013904223;
            fuzz += "\"{}\\ ab/\n"[random % 10];
        }
        KeyValues(fuzz).Parse();
    }

    const auto app = root / "Suite", steam = root / "Steam", library = root / "Second library";
    const auto game = library / "steamapps/common/It Steals";
    fs::create_directories(app);
    Game(game);
    Write(steam / "Steam.exe", "test Steam marker");
    Write(steam / "steamapps/libraryfolders.vdf",
          "\"libraryfolders\" { \"1\" { \"path\" \"" + library.generic_string() + "\" } }");
    Write(library / "steamapps/appmanifest_1349060.acf",
          "\"AppState\" { \"appid\" \"1349060\" \"installdir\" \"It Steals\" }");
    auto resolved = Resolve(app, {steam});
    assert(resolved.game == game / "It Steals.exe" && resolved.steam == steam / "Steam.exe");
    assert(Libraries(steam).size() == 2);
    Write(steam / "steamapps/libraryfolders.vdf",
          "\"LibraryFolders\" { \"1\" \"" + library.generic_string() + "\" }");
    assert(Resolve(app, {steam}).game == resolved.game);
    Write(library / "steamapps/appmanifest_1349060.acf",
          "\"AppState\" { \"appid\" \"12\" \"installdir\" \"It Steals\" }");
    assert(Resolve(app, {steam}).game.empty());
    Write(library / "steamapps/appmanifest_1349060.acf",
          "\"AppState\" { \"appid\" \"1349060\" \"installdir\" \"../It Steals\" }");
    assert(GameInLibrary(library).empty());

    const auto custom = root / fs::u8path("Custom & caf\xc3\xa9");
    Game(custom);
    auto path = (custom / "It Steals.exe").generic_u8string();
    Write(app / "installation.ini",
          "\xef\xbb\xbfgameExecutable=" + std::string(path.begin(), path.end()) + "\r\n");
    assert(Resolve(app, {steam}).game == custom / "It Steals.exe");
    assert(Resolve(app, {steam}).steam.empty());
    auto image = Read(custom / "It Steals.exe");
    image[68] = 0x4c;
    image[69] = 0x01;
    Write(custom / "It Steals.exe", image);
    assert(!IsGame(custom / "It Steals.exe"));
    Game(custom);
    fs::remove(custom / "It Steals_Data/Managed/Assembly-CSharp.dll");
    assert(!IsGame(custom / "It Steals.exe"));
    Game(root);
    assert(Resolve(app, {}).game == root / "It Steals.exe");
    std::cout << "Game launcher contracts passed\n";
}
