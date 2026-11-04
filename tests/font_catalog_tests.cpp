#include "FontCatalog.h"
#include "TrainerPreferences.h"
#include "FontRasterizer.h"
#include <cassert>
#include <iostream>

static void Put16(std::vector<unsigned char> &b, size_t at, uint16_t value)
{
    b[at] = value >> 8;
    b[at + 1] = value & 255;
}
static void Put32(std::vector<unsigned char> &b, size_t at, uint32_t value)
{
    Put16(b, at, value >> 16);
    Put16(b, at + 2, value & 65535);
}

// A bounded SFNT metadata fixture with a Windows UTF-16 full-name record.
// The rasterizer is deliberately not involved in catalog discovery.
static std::vector<unsigned char> Font(const std::string &name, size_t base = 0)
{
    std::vector<unsigned char> b(base + 1024);
    Put32(b, base, 0x00010000);
    Put16(b, base + 4, 7);
    const uint32_t tags[] = {0x636d6170, 0x68656164, 0x68686561, 0x686d7478,
                             0x6d617870, 0x676c7966, 0x6e616d65};
    for (size_t i = 0; i < 7; ++i)
    {
        Put32(b, base + 12 + i * 16, tags[i]);
        Put32(b, base + 20 + i * 16, uint32_t(base + 128 + i * 96));
        Put32(b, base + 24 + i * 16, 96);
    }
    size_t names = base + 128 + 6 * 96;
    Put16(b, names + 2, 1);
    Put16(b, names + 4, 18);
    Put16(b, names + 6, 3); // Windows platform
    Put16(b, names + 8, 1); // Unicode BMP
    Put16(b, names + 10, 0x409);
    Put16(b, names + 12, 4); // full name
    Put16(b, names + 14, uint16_t(name.size() * 2));
    for (size_t i = 0; i < name.size(); ++i)
        Put16(b, names + 18 + i * 2, static_cast<unsigned char>(name[i]));
    return b;
}

int main()
{
    auto bytes = Font("Test Display Bold");
    assert(suite_fonts::Offsets(bytes) == std::vector<size_t>{0});
    assert(suite_fonts::Name(bytes, 0) == "Test Display Bold");
    auto bad = bytes;
    Put32(bad, 20, uint32_t(bad.size() + 1));
    assert(suite_fonts::Offsets(bad).empty());
    bad = bytes;
    Put32(bad, 24, 0xffffffff);
    assert(suite_fonts::Offsets(bad).empty());
    assert(suite_fonts::Offsets(std::vector<unsigned char>(11)).empty());

    auto collection = Font("Collection Face", 32);
    Put32(collection, 0, 0x74746366);
    Put32(collection, 4, 0x00010000);
    Put32(collection, 8, 1);
    Put32(collection, 12, 32);
    assert(suite_fonts::Offsets(collection) == std::vector<size_t>{32});
    assert(suite_fonts::Name(collection, 32) == "Collection Face");
    Put32(collection, 8, 257);
    assert(suite_fonts::Offsets(collection).empty());

    auto folder = std::filesystem::temp_directory_path() / "it-steals-font-catalog-test";
    std::filesystem::create_directories(folder);
    auto write = [&](const std::filesystem::path &path, const std::vector<unsigned char> &data) {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char *>(data.data()), std::streamsize(data.size()));
    };
    write(folder / "face.TTF", bytes);
    write(folder / "ignored.fon", bytes);
    write(folder / "broken.otf", bad);
    suite_fonts::Catalog catalog;
    catalog.AddDirectory(folder, "Suite");
    catalog.AddDirectory(folder / "missing", "Missing");
    catalog.Sort();
    assert(catalog.faces.size() == 1);
    auto id = catalog.faces.front().id;
    assert(id == "Suite:face.TTF#0");
    assert(catalog.Find(id) && suite_fonts::Matches(*catalog.Find(id), "display bold"));
    assert(suite_fonts::Matches(*catalog.Find(id), "SUITE"));
    assert(!suite_fonts::Matches(*catalog.Find(id), "unrelated"));
    auto second = Font("Alphabetical first");
    write(folder / "added.ttf", second);
    catalog.faces.clear();
    catalog.AddDirectory(folder, "Suite");
    catalog.Sort();
    assert(catalog.faces.front().name == "Alphabetical first");
    assert(catalog.Find(id) && catalog.Find(id)->name == "Test Display Bold");
    std::filesystem::remove(folder / "face.TTF");
    catalog.faces.clear();
    catalog.AddDirectory(folder, "Suite");
    catalog.Sort();
    assert(!catalog.Find(id));
    TrainerPreferences preferences;
    preferences.appearance.fontIds[0] = id;
    assert(TrainerPreferences::Decode(preferences.Encode()).appearance.fontIds[0] == id);
    assert(!suite_fonts::ValidId("invalid\nfont"));
#ifdef _WIN32
    suite_fonts::RefreshInstalled();
    assert(suite_fonts::catalog.scanned && !suite_fonts::catalog.faces.empty());
    size_t usable = 0;
    for (const auto &face : suite_fonts::catalog.faces)
    {
        assert(suite_fonts::catalog.Find(face.id));
        auto file = suite_fonts::Read(face.path);
        auto offsets = suite_fonts::Offsets(file);
        assert(size_t(face.index) < offsets.size());
        stbtt_fontinfo info{};
        if (stbtt_InitFont(&info, file.data(), int(offsets[face.index])) && stbtt_FindGlyphIndex(&info, 'A'))
            ++usable;
    }
    assert(usable > 0);
    std::cout << "Actual Windows system/user font discovery and rasterizer initialization: " << usable << " usable faces\n";
#endif
    std::filesystem::remove_all(folder);
    std::cout << "Font metadata, collection bounds, search, stable selection and missing-font persistence: PASS\n";
}
