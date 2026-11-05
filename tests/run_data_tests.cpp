#ifdef _WIN32
#include <windows.h>
#endif
#include "RunDataService.h"
#include <cassert>
#include <iostream>

using namespace suite_runs;
static std::string Checked(std::string body)
{
    char hash[17]; snprintf(hash, sizeof(hash), "%016llx", (unsigned long long)Hash(body));
    return body + "|" + hash;
}
static void Await(const std::function<bool()> &done)
{
    auto until = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!done() && std::chrono::steady_clock::now() < until)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    assert(done());
}
int main(int argc, char **argv)
{
    assert(argc >= 3);
    std::ifstream in(argv[1], std::ios::binary);
    std::string bytes((std::istreambuf_iterator<char>(in)), {}), line;
    std::istringstream input(bytes);
    std::getline(input, line); assert(line == "ELITE_RUN_LEDGER|1");
    std::getline(input, line); auto session = ParseSession(line);
    assert(session == 638000000000000000LL);
    auto store = std::make_shared<Store>();
    auto records = std::make_shared<std::vector<Record>>();
    while (std::getline(input, line)) records->push_back(Parse(line));
    assert(records->size() == 8 && (*records)[0].Ranked() && !(*records)[6].Ranked() && (*records)[7].quick);
    store->records = records; store->session = session;
    Query q; q.display = Display::TopFive;
    auto top = Build(store, q);
    assert(top.rows.size() == 4); // 2 practice, 1 invalid and 1 reset excluded
    q.category = 1;
    assert(Build(store, q).rows.size() == 2);
    q = {}; q.display = Display::Statistics;
    auto stats = Build(store, q);
    assert(stats.rows.empty() && stats.totals[0].attempts == 3 && stats.totals[0].valid == 2);
    assert(stats.totals[0].invalid == 1 && stats.totals[1].quick == 1);
    q = {}; q.display = Display::History; q.session = session + 1;
    assert(Build(store, q).rows.empty());
    q.session = session; q.sort = Sort::Time;
    auto history = Build(store, q); assert(history.rows.size() == 8 && history.rows[0].seconds == 1);
    q.seedFilter = true; q.seed = (*records)[0].seed;
    for (auto &r : Build(store, q).rows) assert(r.seedKnown && (r.category == 1 ? r.selectedSeed : r.seed) == q.seed);
    // Hundreds of tied runs: stable top-five, independent modes and categories.
    records = std::make_shared<std::vector<Record>>();
    for (int i = 0; i < 4000; i++)
    {
        Record r = (*store->records)[0]; r.id += i; r.started += i; r.session = session;
        r.scene = kMapScenes[i % 8]; r.category = i / 8 % 2; r.seconds = 10 + i / 16 % 9;
        records->push_back(r);
    }
    store->records = records; q = {}; q.display = Display::TopFive;
    auto many = Build(store, q); assert(many.rows.size() == 80);
    for (size_t i = 0; i < many.rows.size(); i++) assert(many.ranks[i] >= 1 && many.ranks[i] <= 5);
    auto baseline = std::make_shared<Store>(*store); baseline->legacy.pb[0] = 2;
    q.display = Display::Bests;
    auto bests = Build(baseline, q); assert(bests.rows.size() == 16 && bests.rows[0].id == 0 && bests.rows[0].legacy);
    q.seedFilter = true; q.seed = (*records)[0].seed;
    for (auto &r : Build(baseline, q).rows) assert(r.id > 0); // scalar old PB has no seed evidence
    bool rejected = false;
    try { auto damaged = bytes.substr(bytes.find('\n', bytes.find('\n') + 1) + 1); damaged[10] ^= 1; Parse(damaged.substr(0, damaged.find('\n'))); }
    catch (...) { rejected = true; } assert(rejected);
    // The real worker follows completed lines, holds torn tails, detects corruption,
    // and never publishes a superseded query under a newer category.
    std::filesystem::path dir = argv[2]; std::filesystem::create_directories(dir);
    auto file = dir / "run-ledger-v1.tsv";
    { std::ofstream out(file, std::ios::binary); out << bytes; }
    Service service; service.SetDirectory(dir); q = {}; q.display = Display::History;
    auto request = service.Submit(q);
    Await([&] { auto v = service.Get(); return v->source && v->request == request && v->rows.size() == 8; });
    std::string body = Checked("638000000000001000|7|1|638000000000001000|638000000100001000|10|2|1|42|42|0|1|1|0|0|0|638000000000000000|1|1|0|0");
    { std::ofstream out(file, std::ios::app | std::ios::binary); out << body.substr(0, 31); }
    service.Refresh();
    q.category = 1; request = service.Submit(q);
    Await([&] { return service.Get()->request == request; });
    assert(service.Get()->rows.size() == 3);
    { std::ofstream out(file, std::ios::app | std::ios::binary); out << body.substr(31) << '\n'; }
    service.Refresh();
    Await([&] { return service.Get()->rows.size() == 4; });
    { std::ofstream out(file, std::ios::app | std::ios::binary); out << "old-crash-tail"; }
    service.Refresh(); auto tailRequest = service.Submit(q);
    Await([&] { return service.Get()->request > request; });
    { std::ofstream out(file, std::ios::binary); out << bytes << body << '\n' << Checked("SESSION|" + std::to_string(session + 1)) << '\n'; }
    service.Refresh();
    Await([&] { return service.Get()->source->session == session + 1 && service.Get()->source->error.empty(); });
    assert(service.Get()->rows.size() == 4);
    { std::ofstream out(file, std::ios::app | std::ios::binary); out << "bad complete record\n"; }
    service.Refresh();
    Await([&] { return !service.Get()->source->error.empty(); });
    assert(service.Get()->rows.size() == 4);
    service.Shutdown();
    if (argc > 3)
    {
        std::ifstream counters(argv[3], std::ios::binary);
        std::string wire((std::istreambuf_iterator<char>(counters)), {});
        auto old = ParseLegacy(wire);
        assert(old.available && old.since == session && old.pb[0] == 10 && old.pb[15] == 18 && old.dates[15] == session + 7);
        assert(old.modes[31].attempts == 51 && old.modes[31].finished == 3 && old.modes[31].playSeconds == 131);
        bool refused = false; try { wire[0] ^= 1; ParseLegacy(wire); } catch (...) { refused = true; } assert(refused);
    }
    std::cout << "Run archive shared C#/C++ wire, category validity, top-five ties, legacy PB evidence, query worker, partial append and corruption: PASS\n";
}
