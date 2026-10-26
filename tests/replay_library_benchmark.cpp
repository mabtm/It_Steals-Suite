#include "ReplayLibraryModel.h"
#include <chrono>
#include <iomanip>
#include <iostream>

using namespace practice;
using Clock = std::chrono::steady_clock;

// Frozen 7.17 list-filter path, before ImGui's existing visible-row clipping.
static std::vector<size_t> Legacy(const ReplayArchiveState &state, std::string query)
{
    std::transform(query.begin(), query.end(), query.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    std::vector<size_t> shown;
    for (size_t i = 0; i < state.runs.size(); i++)
    {
        const auto &row = state.runs[i];
        std::string text = row.name + " " + row.category + " " + std::to_string(row.seed) + " " +
                           kMapLevelNames[MapLevelIndex(row.scene)];
        std::transform(text.begin(), text.end(), text.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        if (text.find(query) != std::string::npos)
            shown.push_back(i);
    }
    return shown;
}

int main()
{
    std::cout << "runs,frames,repeat,variant,ms_per_frame,matches,checksum,cold_index_ms\n"
              << std::setprecision(10);
    for (int count : {1000, 10000})
    {
        auto state = std::make_shared<ReplayArchiveState>();
        for (int i = 0; i < count; i++)
        {
            ReplayInfo row;
            row.id = 638000000000000000ll + count - i;
            row.scene = i % 2 ? 3 : 13;
            row.seed = i * 17;
            row.name = "Recorded run " + std::to_string(i);
            row.category = i % 3 ? "Training" : "PB";
            row.outcome = 1;
            row.seconds = 60 + i % 20;
            state->runs.push_back(std::move(row));
        }
        ReplayLibraryFilter filter;
        filter.query = "Training";
        ReplayLibraryView view;
        auto start = Clock::now();
        view.Update(state, filter);
        double cold = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        if (Legacy(*state, filter.query) != view.shown)
            throw std::runtime_error("Filter parity failed");
        for (int repeat = 0; repeat < 5; repeat++)
            for (int pass = 0; pass < 2; pass++)
            {
                bool cached = (pass ^ (repeat & 1)) != 0;
                uint64_t checksum = 0;
                start = Clock::now();
                for (int frame = 0; frame < 120; frame++)
                    if (cached)
                    {
                        view.Update(state, filter);
                        checksum += view.shown.size() + view.shown.front() + view.shown.back();
                    }
                    else
                    {
                        auto shown = Legacy(*state, filter.query);
                        checksum += shown.size() + shown.front() + shown.back();
                    }
                double ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count() / 120;
                std::cout << count << ",120," << repeat + 1 << ',' << (cached ? "7.18.0" : "7.17.0") << ','
                          << ms << ',' << view.shown.size() << ',' << checksum << ',' << cold << '\n';
            }
    }
}
