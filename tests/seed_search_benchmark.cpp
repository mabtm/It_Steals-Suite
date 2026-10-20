#include "seed_batch_fixture.h"
#include <cassert>
#include <fstream>
#include <iostream>

static std::string MemoryPeak()
{
    std::ifstream in("/proc/self/status");
    std::string line;
    while (std::getline(in, line))
        if (line.rfind("VmHWM:", 0) == 0)
            return line;
    return "Peak memory unavailable on this host";
}

int main()
{
    auto fixture = SeedFixture();
    auto source = StreamingSource(fixture);
    practice::SeedSearch search;
    auto request = StreamingRequest(*source, 1000000, std::min(4, SeedWorkerLimit()), true);
    auto started = std::chrono::steady_clock::now();
    search.Begin(request);
    search.Update(source);
    FeedStreaming(search, request, fixture.samples);
    auto result = AwaitStreaming(search);
    assert(result->phase == practice::SearchPhase::Completed && result->generated == 1000000 &&
           result->evaluated == 1000000 && result->top.size() <= 10 &&
           result->queued <= practice::SeedQueueLimit);
    assert(result->source->data->samples.empty() && result->top.front().index < 20);
    double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    std::cout << "1,000,000 streamed synthetic layouts, " << request.workers << " workers, " << seconds
              << " s, " << 1000000 / seconds << " routes/s, " << MemoryPeak() << "\n";
    std::cout
        << "This exercises the real route pipeline with repeating test geometry; it does not measure Unity generation or live game throughput.\n";
}
