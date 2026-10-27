#pragma once
#include "GpuDevDevice.h"
#include <intrin.h>

namespace gpu_dev
{
using Clock = std::chrono::steady_clock;
inline double Ms(Clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
inline int64_t UtcMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch()).count();
}

inline Report Run(std::shared_ptr<const practice::MapAnalysis> source, Options options,
                  const practice::Cancel &canceled, const std::function<void(Phase, int)> &progress,
                  bool allowSoftware = false)
{
    Report report;
    report.options = options;
    report.startedUtcMs = UtcMs();
    report.logicalCpus = std::thread::hardware_concurrency();
    int brand[13]{};
    for (int i = 0; i < 3; i++) __cpuid(brand + i * 4, 0x80000002 + i);
    report.cpu = reinterpret_cast<const char *>(brand);
    report.source = source ? source->data : nullptr;
    report.liveCapture = source && source->live && !options.fixture;
    auto started = Clock::now();
    auto check = [&] { if (canceled()) throw std::runtime_error("GPU test stopped"); };
    try
    {
        progress(Phase::Preparing, 0);
        if (!source || !source->data || (!options.fixture && !source->live) || !source->data->Verified())
            throw std::runtime_error("Load a verified live map before starting the GPU test");
        const auto &data = *source->data;
        if (options.seeds < 1 || options.seeds > 4096 || options.batch < 1 || options.batch > 32 ||
            options.pairs < 1 || options.pairs > 3 || options.recipe < 1 || options.recipe > 2)
            throw std::runtime_error("Invalid GPU test request");
        int crystals = std::min(data.keep, int(data.points.size()));
        if (crystals < 16 || crystals > 52)
            throw std::runtime_error("This GPU dev path supports 16 to 52 crystals; smaller exact-DP modes stay on the CPU");
        if (data.flags & 128) throw std::runtime_error("Navigation capture is still being prepared");
        practice::PortableSelectionGenerator generator(data);
        practice::RandomSeedOrder seedOrder(options.key);
        std::vector<practice::Selection> selections(options.seeds);
        for (int i = 0; i < options.seeds; i++)
        {
            check();
            generator.Generate(seedOrder.SeedAt(uint32_t(i)), options.recipe, selections[i]);
            report.generated++;
        }
        auto setup = Clock::now();
        Device device(report, allowSoftware);
        report.deviceMs = Ms(setup);
        check();
        setup = Clock::now();
        practice::NavGraph graph(data);
        practice::SearchDistances distances(data, graph, canceled, [](int, int) {});
        report.preparationMs = Ms(setup);
        report.model = distances.ModelName();
        practice::RouteWorkspace work;
        auto cpuBatch = [&](int offset, int count, PairTiming &timing) {
            auto at = Clock::now();
            std::vector<practice::RouteResult> result;
            result.reserve(count);
            for (int i = 0; i < count; i++)
            {
                check();
                result.push_back(distances.Score(selections[offset + i], false, canceled, work));
                if (!result.back().complete) throw std::runtime_error(result.back().message);
            }
            timing.cpuMs += Ms(at);
            return result;
        };
        auto gpuBatch = [&](int offset, int count, PairTiming &timing) {
            auto all = Clock::now(), at = all;
            for (int i = 0; i < count; i++)
            {
                check();
                int side = distances.FillMatrix(selections[offset + i], work);
                device.Pack(i, work.matrix.data(), side - 1);
            }
            timing.packMs += Ms(at);
            check();
            at = Clock::now();
            auto result = device.Execute(count);
            timing.dispatchMs += Ms(at);
            at = Clock::now();
            for (int i = 0; i < count; i++)
            {
                check();
                if (result[i].complete)
                    result[i] = distances.FinishOrder(selections[offset + i], std::move(result[i]), canceled);
            }
            timing.contactsMs += Ms(at);
            timing.gpuMs += Ms(all);
            return result;
        };
        int matrixExamples = 0;
        auto record = [&](int pair, int index, practice::RouteResult cpu, practice::RouteResult gpu) {
            Case row;
            row.pair = pair;
            row.seed = seedOrder.SeedAt(uint32_t(index));
            row.spawn = selections[index].spawn;
            row.crystals = selections[index].crystals;
            row.orderMatch = cpu.order == gpu.order;
            row.scoreMatch = SameScore(cpu, gpu);
            bool mismatch = !row.orderMatch || !row.scoreMatch;
            report.mismatches += mismatch;
            if (matrixExamples < 8 && (mismatch || pair == 0))
            {
                int side = distances.FillMatrix(selections[index], work);
                row.matrix.assign(work.matrix.begin(), work.matrix.begin() + side * side);
                matrixExamples++;
            }
            row.cpu = std::move(cpu);
            row.gpu = std::move(gpu);
            report.cases.push_back(std::move(row));
        };
        // Warm both complete scoring paths before timing the paired batches.
        PairTiming warm;
        auto warmCpu = cpuBatch(0, 1, warm), warmGpu = gpuBatch(0, 1, warm);
        record(0, 0, std::move(warmCpu[0]), std::move(warmGpu[0]));
        report.warmupChecked = 1;
        for (int pair = 1; pair <= options.pairs; pair++)
        {
            report.timings.push_back({});
            auto &timing = report.timings.back();
            timing.pair = pair;
            timing.gpuFirst = pair % 2 == 0;
            for (int offset = 0; offset < options.seeds; offset += options.batch)
            {
                check();
                int count = std::min(options.batch, options.seeds - offset);
                std::vector<practice::RouteResult> cpu, gpu;
                auto reference = [&] { progress(Phase::Cpu, report.checked); cpu = cpuBatch(offset, count, timing); };
                auto accelerated = [&] { progress(Phase::Gpu, report.checked); gpu = gpuBatch(offset, count, timing); };
                if (timing.gpuFirst) { accelerated(); reference(); }
                else { reference(); accelerated(); }
                for (int i = 0; i < count; i++)
                    record(pair, offset + i, std::move(cpu[i]), std::move(gpu[i]));
                timing.checked += count;
                report.checked += count;
                progress(timing.gpuFirst ? Phase::Cpu : Phase::Gpu, report.checked);
            }
        }
        report.phase = Phase::Completed;
    }
    catch (const std::exception &error)
    {
        report.phase = canceled() ? Phase::Stopped : Phase::Failed;
        report.error = error.what();
    }
    report.elapsedMs = Ms(started);
    return report;
}

struct AdapterList
{
    std::vector<Adapter> entries;
    std::string error;
};

class Session
{
    std::jthread worker;
    std::shared_ptr<const Report> current;
    std::shared_ptr<const AdapterList> adapters = std::make_shared<AdapterList>();
    std::shared_ptr<const practice::MapAnalysis> context;
    std::atomic<bool> busy{false};
    std::atomic<Phase> phase{Phase::Idle};
    std::atomic<int> checked{0};
    std::mutex frameGate;
    Observations frames;

  public:
    std::filesystem::path directory;
    ~Session() { Stop(); if (worker.joinable()) worker.join(); }
    bool Busy() const { return busy.load(std::memory_order_acquire); }
    Phase Status() const { return phase.load(std::memory_order_relaxed); }
    int Checked() const { return checked.load(std::memory_order_relaxed); }
    std::shared_ptr<const Report> Get() const { return std::atomic_load(&current); }
    std::shared_ptr<const AdapterList> GetAdapters() const { return std::atomic_load(&adapters); }
    void Stop() { if (worker.joinable()) worker.request_stop(); }

    void RefreshAdapters()
    {
        if (Busy()) return;
        if (worker.joinable()) worker.join();
        context.reset();
        busy.store(true, std::memory_order_release);
        phase.store(Phase::Detecting);
        worker = std::jthread([this](std::stop_token) {
            auto list = std::make_shared<AdapterList>();
            try { list->entries = Adapters(); }
            catch (const std::exception &e) { list->error = e.what(); }
            std::atomic_store(&adapters, std::shared_ptr<const AdapterList>(list));
            phase.store(Phase::Idle);
            busy.store(false, std::memory_order_release);
        });
    }

    void Start(std::shared_ptr<const practice::MapAnalysis> source, Options options, Observations baseline)
    {
        if (Busy()) return;
        if (worker.joinable()) worker.join();
        context = source;
        { std::lock_guard lock(frameGate); frames = baseline; }
        checked.store(0);
        phase.store(Phase::Preparing);
        busy.store(true, std::memory_order_release);
        auto folder = directory;
        worker = std::jthread([this, source, options, folder](std::stop_token stop) {
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
            auto report = std::make_shared<Report>(Run(source, options, [&] { return stop.stop_requested(); },
                [&](Phase p, int done) { phase.store(p); checked.store(done); }));
            { std::lock_guard lock(frameGate); report->observations = frames; }
            if (!folder.empty())
            {
                auto path = folder / ("gpu-dev-" + std::to_string(report->startedUtcMs) + ".json");
                try { Save(path, *report); report->savedPath = path; }
                catch (const std::exception &e) { report->exportError = e.what(); }
            }
            std::atomic_store(&current, std::shared_ptr<const Report>(report));
            phase.store(report->phase);
            busy.store(false, std::memory_order_release);
        });
    }

    void Observe(double cpuMs, double intervalMs, bool menu, double gameFps)
    {
        if (!Busy() || Status() == Phase::Detecting) return;
        std::lock_guard lock(frameGate);
        frames.Record(cpuMs, intervalMs, menu, gameFps);
    }

    void CheckContext(int pid, int scene, int epoch, uint32_t session, bool restricted)
    {
        if (!Busy() || !context || !context->data) return;
        const auto &d = *context->data;
        if (restricted || d.pid != pid || d.scene != scene || d.epoch != epoch || context->session != session)
            Stop();
    }
};
} // namespace gpu_dev
