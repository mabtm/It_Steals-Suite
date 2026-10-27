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

inline double ThreadMs()
{
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user)) return 0;
    auto ticks = [](FILETIME t) { return (uint64_t(t.dwHighDateTime) << 32) | t.dwLowDateTime; };
    return double(ticks(kernel) + ticks(user)) / 10000.;
}

struct ScopeTiming
{
    double &wall, &cpu;
    Clock::time_point started = Clock::now();
    double thread = ThreadMs();
    ~ScopeTiming() { wall += Ms(started); cpu += ThreadMs() - thread; }
};

using Progress = std::function<void(Phase, int, int, int)>;
inline Report Run(std::shared_ptr<const practice::MapAnalysis> source, Options options,
                  const practice::Cancel &canceled, const Progress &progress,
                  bool allowSoftware = false, const std::function<void(const Report &)> &checkpoint = {})
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
    int processed = 0, matrixExamples = 0;
    auto check = [&] { if (canceled()) throw std::runtime_error("GPU test stopped"); };
    try
    {
        progress(Phase::Preparing, 0, 0, 0);
        if (!source || !source->data || (!options.fixture && !source->live) || !source->data->Verified())
            throw std::runtime_error("Load a verified live map before starting the GPU test");
        const auto &data = *source->data;
        if (!ValidOptions(options)) throw std::runtime_error("Invalid GPU test request");
        int crystals = std::min(data.keep, int(data.points.size()));
        if (crystals < 16 || crystals > 52)
            throw std::runtime_error("This GPU dev path supports 16 to 52 crystals; smaller exact-DP modes stay on the CPU");
        if (data.flags & 128) throw std::runtime_error("Navigation capture is still being prepared");
        practice::PortableSelectionGenerator generator(data);
        practice::RandomSeedOrder seedOrder(options.key);
        auto generation = Clock::now();
        std::vector<practice::Selection> selections(options.seeds);
        for (int i = 0; i < options.seeds; i++)
        {
            check();
            generator.Generate(seedOrder.SeedAt(uint32_t(i)), options.recipe, selections[i]);
            report.generated++;
        }
        report.generationMs = Ms(generation);
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
        auto configurations = Batches(options);
        std::vector<practice::RouteResult> cpu(options.seeds), gpu(options.seeds);
        std::vector<double> centerLengths(options.seeds);
        auto cpuPass = [&](int count, PairTiming &timing, bool warmup) {
            if (!warmup) progress(Phase::Cpu, processed, timing.batch, timing.pair);
            ScopeTiming measured{timing.cpuMs, timing.cpuThreadMs};
            for (int i = 0; i < count; i++)
            {
                check();
                cpu[i] = distances.Score(selections[i], false, canceled, work);
                if (!cpu[i].complete) throw std::runtime_error(cpu[i].message);
                timing.cpuScored++;
                if (!warmup)
                {
                    processed++;
                    if (i % 16 == 15 || i + 1 == count) progress(Phase::Cpu, processed, timing.batch, timing.pair);
                }
            }
        };
        auto gpuPass = [&](int count, PairTiming &timing, bool warmup) {
            if (!warmup) progress(Phase::Gpu, processed, timing.batch, timing.pair);
            ScopeTiming measured{timing.gpuMs, timing.gpuThreadMs};
            if (!warmup) device.BeginPass();
            std::exception_ptr failure;
            try
            {
                for (int offset = 0; offset < count; offset += timing.batch)
                {
                    check();
                    int size = std::min(timing.batch, count - offset);
                    auto at = Clock::now();
                    for (int i = 0; i < size; i++)
                    {
                        check();
                        int side = distances.FillMatrix(selections[offset + i], work);
                        device.Pack(i, work.matrix.data(), side - 1);
                    }
                    timing.packMs += Ms(at);
                    check();
                    at = Clock::now();
                    auto result = device.Execute(size, &timing);
                    timing.dispatchMs += Ms(at);
                    at = Clock::now();
                    for (int i = 0; i < size; i++)
                    {
                        check();
                        centerLengths[offset + i] = result[i].length;
                        gpu[offset + i] = result[i].complete
                            ? distances.FinishOrder(selections[offset + i], std::move(result[i]), canceled)
                            : std::move(result[i]);
                        timing.gpuScored++;
                        if (!warmup) processed++;
                    }
                    timing.contactsMs += Ms(at);
                    if (!warmup) progress(Phase::Gpu, processed, timing.batch, timing.pair);
                }
            }
            catch (...) { failure = std::current_exception(); }
            if (!warmup)
            {
                try { device.EndPass(timing, canceled); }
                catch (...) { if (!failure) failure = std::current_exception(); }
            }
            if (failure) std::rethrow_exception(failure);
        };
        auto compare = [&](PairTiming &timing, bool warmup) {
            int count = std::min(timing.cpuScored, timing.gpuScored);
            for (int index = 0; index < count; index++)
            {
                int side = distances.FillMatrix(selections[index], work), previous = 0;
                double cpuCenter = 0;
                for (int id : cpu[index].order)
                {
                    cpuCenter += work.matrix[previous * side + id + 1];
                    previous = id + 1;
                }
                bool order = cpu[index].order == gpu[index].order;
                bool score = SameScore(cpu[index], gpu[index]);
                bool center = std::isfinite(centerLengths[index]) && cpuCenter == centerLengths[index];
                bool mismatch = !order || !score || !center;
                report.mismatches += mismatch;
                timing.mismatches += mismatch;
                DigestRoute(timing.cpuDigest, cpu[index]); DigestRoute(timing.gpuDigest, gpu[index]);
                if (warmup) report.warmupChecked++;
                else { timing.checked++; report.checked++; }
                bool example = warmup ? index == 0 : index == 0 || index + 1 == count;
                if ((!mismatch && !example) || !KeepCase(report, mismatch, warmup)) continue;
                Case row;
                row.pair = timing.pair; row.batch = timing.batch;
                row.seed = seedOrder.SeedAt(uint32_t(index)); row.spawn = selections[index].spawn;
                row.crystals = selections[index].crystals;
                row.orderMatch = order; row.scoreMatch = score; row.centerLengthMatch = center;
                row.cpuCenterLength = cpuCenter; row.gpuCenterLength = centerLengths[index];
                if (matrixExamples < 8 && (mismatch || (warmup && matrixExamples == 0)))
                {
                    row.matrix.assign(work.matrix.begin(), work.matrix.begin() + side * side);
                    matrixExamples++;
                }
                row.cpu = std::move(cpu[index]); row.gpu = std::move(gpu[index]);
                report.cases.push_back(std::move(row));
            }
        };
        // Warm the full batch for each configuration, including CPU pickup scoring.
        auto warming = Clock::now();
        for (int batch : configurations)
        {
            check();
            progress(Phase::Warming, 0, batch, 0);
            device.Resize(batch, report);
            PairTiming warm; warm.batch = batch;
            int count = std::min(batch, options.seeds);
            cpuPass(count, warm, true); gpuPass(count, warm, true); compare(warm, true);
        }
        report.warmupMs = Ms(warming);
        progress(Phase::Baseline, 0, 0, 0);
        auto baseline = Clock::now();
        while (Ms(baseline) < options.baselineSeconds * 1000.)
        {
            check();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        report.baselineMs = Ms(baseline);
        for (int pair = 1; pair <= options.pairs; pair++)
            for (size_t slot = 0; slot < configurations.size(); slot++)
            {
                check();
                int batch = configurations[(slot + size_t(pair - 1)) % configurations.size()];
                progress(Phase::Preparing, processed, batch, pair);
                setup = Clock::now();
                device.Resize(batch, report);
                report.timings.push_back({});
                auto &timing = report.timings.back();
                timing.pair = pair; timing.batch = batch; timing.sequence = int(report.timings.size());
                timing.bufferSetupMs = Ms(setup);
                timing.gpuFirst = pair % 2 == 0;
                std::exception_ptr failure;
                try
                {
                    if (timing.gpuFirst) { gpuPass(options.seeds, timing, false); cpuPass(options.seeds, timing, false); }
                    else { cpuPass(options.seeds, timing, false); gpuPass(options.seeds, timing, false); }
                }
                catch (...) { failure = std::current_exception(); }
                // Keep verified partial progress on cancellation or device failure.
                progress(Phase::Verifying, processed, batch, pair);
                auto checking = Clock::now();
                compare(timing, false);
                timing.verificationMs = Ms(checking);
                timing.complete = timing.checked == options.seeds && !failure;
                report.phase = Phase::Verifying;
                report.elapsedMs = Ms(started);
                if (checkpoint) checkpoint(report);
                if (failure) std::rethrow_exception(failure);
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
    std::atomic<int> activeBatch{0}, activePair{0};
    std::mutex frameGate;
    Observations frames;

  public:
    std::filesystem::path directory;
    ~Session() { Stop(); if (worker.joinable()) worker.join(); }
    bool Busy() const { return busy.load(std::memory_order_acquire); }
    Phase Status() const { return phase.load(std::memory_order_relaxed); }
    int Checked() const { return checked.load(std::memory_order_relaxed); }
    int Batch() const { return activeBatch.load(std::memory_order_relaxed); }
    int Pair() const { return activePair.load(std::memory_order_relaxed); }
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
        activeBatch.store(0); activePair.store(0);
        std::atomic_store(&current, std::shared_ptr<const Report>{});
        phase.store(Phase::Preparing);
        busy.store(true, std::memory_order_release);
        auto folder = directory;
        worker = std::jthread([this, source, options, folder](std::stop_token stop) {
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
            auto publish = [&](const Report &result) {
                auto snapshot = std::make_shared<Report>(result);
                { std::lock_guard lock(frameGate); snapshot->observations = frames; }
                if (!folder.empty())
                {
                    auto path = folder / ("gpu-dev-" + std::to_string(snapshot->startedUtcMs) + ".json");
                    try { Save(path, *snapshot); snapshot->savedPath = path; }
                    catch (const std::exception &e) { snapshot->exportError = e.what(); }
                }
                std::atomic_store(&current, std::shared_ptr<const Report>(snapshot));
            };
            auto report = Run(source, options, [&] { return stop.stop_requested(); },
                [&](Phase p, int done, int batch, int pair) {
                    activeBatch.store(batch); activePair.store(pair); checked.store(done); phase.store(p);
                }, false, publish);
            publish(report);
            phase.store(report.phase);
            busy.store(false, std::memory_order_release);
        });
    }

    void Observe(double cpuMs, double intervalMs, bool menu, double gameFps)
    {
        if (!Busy() || Status() == Phase::Detecting) return;
        std::lock_guard lock(frameGate);
        frames.Record(cpuMs, intervalMs, menu, gameFps, Status(), Batch());
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
