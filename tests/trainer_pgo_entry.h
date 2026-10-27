#pragma once
// Only compiled when SuitePgoResearch=true. Reuse the identical benchmark
// inside the actual trainer image; ordinary builds have no additional entry.
#include <filesystem>
#include <shellapi.h>
#define main SuitePgoBenchmarkMain
#define RunBenchmark SuitePgoRunBenchmark
#include "seed_route_benchmark.cpp"
#undef RunBenchmark
#undef main

inline int TrainerPgoEntry()
{
    int count = 0;
    LPWSTR *arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!arguments)
        return -1;
    if (count < 2 || std::wstring_view(arguments[1]) != L"--suite-pgo-benchmark")
    {
        LocalFree(arguments);
        return -1;
    }
    if (count < 9)
    {
        LocalFree(arguments);
        return 2;
    }
    std::ofstream output(std::filesystem::path(arguments[2]), std::ios::trunc);
    if (!output)
    {
        LocalFree(arguments);
        return 2;
    }
    std::vector<std::string> text{"benchmark"};
    for (int i = 3; i < count; i++)
    {
        int length = WideCharToMultiByte(CP_UTF8, 0, arguments[i], -1, nullptr, 0, nullptr, nullptr);
        std::string value(size_t(length), '\0');
        WideCharToMultiByte(CP_UTF8, 0, arguments[i], -1, value.data(), length, nullptr, nullptr);
        if (!value.empty())
            value.pop_back();
        text.push_back(std::move(value));
    }
    LocalFree(arguments);
    std::vector<char *> argv;
    for (auto &value : text)
        argv.push_back(value.data());
    auto *oldOut = std::cout.rdbuf(output.rdbuf());
    auto *oldError = std::cerr.rdbuf(output.rdbuf());
    int result = SuitePgoBenchmarkMain(int(argv.size()), argv.data());
    std::cout.rdbuf(oldOut);
    std::cerr.rdbuf(oldError);
    return result;
}
