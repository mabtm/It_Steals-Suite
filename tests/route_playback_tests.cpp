#include "RoutePlayback.h"
#include <cassert>
#include <iostream>
using namespace practice;

int main()
{
    RunReview run;
    run.seconds = 2;
    for (int i = 0; i <= 100; i++)
        run.trace.push_back({i * .02, {float(i * .1), 0, 0}});
    auto start = PlayedSlice(run, 0);
    assert(start.visible && start.marker.x == 0 && start.trail.size() == 1);
    auto middle = PlayedSlice(run, 1, .2);
    assert(middle.visible && std::abs(middle.marker.x - 5) < 1e-5 &&
           std::abs(middle.trail.front().x - 4) < 1e-5 && middle.trail.size() < 20);
    auto end = PlayedSlice(run, 99, .2);
    assert(end.marker.x == 10 && end.trail.front().x >= 9);
    run.trace = {{0, {0, 0, 0}}, {1, {10, 0, 0}}, {1.02, {11, 0, 0}}};
    assert(PlayedSlice(run, .5).marker.x == 0);
    auto gap = PlayedSlice(run, 1.01);
    assert(gap.trail.front().x >= 10);
    RouteResult route;
    route.complete = true;
    route.line = {{0, 0, 0}, {0, 0, 0}, {10, 0, 0}, {10, 0, 10}};
    route.length = 20;
    auto optimum = SuggestedSlice(route, .75, .1);
    assert(optimum.visible && optimum.marker.x == 10 && std::abs(optimum.marker.z - 5) < 1e-5 &&
           optimum.trail.front().z >= 2.99);
    assert(SuggestedSlice(route, 0).marker.x == 0 && SuggestedSlice(route, 1).marker.z == 10);
    route.estimated = true;
    assert(!SuggestedSlice(route, .5).visible);
    route.estimated = false;
    route.line.clear();
    assert(!SuggestedSlice(route, 1).visible);
    // Cumulative seed drawing retains the spawn and every revealed segment.
    route.line = {{0, 0, 0}, {10, 0, 0}, {10, 0, 10}};
    for (double fraction : {0., .1, .5, .75, 1.})
    {
        auto prefix = SuggestedSlice(route, fraction, 1.);
        assert(prefix.visible && Same(prefix.trail.front(), route.line.front()));
        assert(Same(prefix.marker, Interpolate(fraction <= .5 ? route.line[0] : route.line[1],
                                               fraction <= .5 ? route.line[1] : route.line[2],
                                               fraction <= .5 ? fraction * 2 : (fraction - .5) * 2)));
    }
    SeedRoutePlayback seed;
    seed.Advance(1, true);
    assert(seed.seconds == 0);
    seed.suggested = true;
    for (int i = 0; i < 25; i++)
        seed.Advance(.1, true);
    assert(std::abs(seed.Fraction() - .5) < 1e-6);
    seed.playing = false;
    auto paused = seed.seconds;
    seed.Advance(.1, true);
    assert(seed.seconds == paused);
    seed.ShowAll();
    assert(seed.Complete() && seed.Fraction() == 1);
    seed.playing = true;
    for (int i = 0; i < 1000; i++)
    {
        seed.Advance(.1, true);
        assert(seed.Complete() && seed.Fraction() == 1 && seed.Pulse() >= 0 && seed.Pulse() < 1);
    }
    seed.Restart();
    assert(seed.seconds == 0 && seed.pulseSeconds == 0 && seed.playing && seed.suggested);
    seed.Advance(NAN, true);
    seed.Advance(-1, true);
    assert(seed.seconds == 0);
    seed.Advance(.1, false);
    assert(seed.Complete());
    route.complete = false;
    assert(!SuggestedSlice(route, 1, 1).visible);
    route.complete = true;
    route.estimated = true;
    assert(!SuggestedSlice(route, 1, 1).visible);
    route.estimated = false;
    auto travel = EstimateTravel(81);
    assert(travel.valid && std::abs(travel.fast - 8.1) < 1e-8 && std::abs(travel.slow - 10) < 1e-8);
    assert(!EstimateTravel(81, false).valid && !EstimateTravel(-1).valid && !EstimateTravel(INFINITY).valid &&
           EstimateTravel(0).fast == 0);
    assert(TravelDuration(90, 9) == 10 && RouteSpeed(0) == 8.1f && RouteSpeed(99) == 10 &&
           RouteSpeed(NAN) == 9);
    ReviewPlayback clock;
    clock.Bind(1);
    clock.Advance(.05, 1);
    assert(clock.seconds == .1 && clock.playing);
    clock.playing = false;
    clock.Advance(.05, 1);
    assert(clock.seconds == .1);
    clock.played = false;
    clock.suggested = true;
    clock.Restart();
    assert(!clock.played && clock.suggested && clock.playing && clock.seconds == 0);
    clock.Advance(99, .1);
    assert(!clock.playing && clock.seconds == .1);
    clock.Bind(2);
    assert(clock.seconds == 0 && clock.playing);
    clock.Advance(NAN, 1);
    assert(clock.seconds == 0);
    clock.played = clock.suggested = true;
    assert(clock.Duration(10, 20, true) == 20);
    clock.suggested = false;
    assert(clock.Duration(10, 20, true) == 10);
    clock.played = false;
    clock.suggested = true;
    assert(clock.Duration(10, 5, true) == 5);
    assert(clock.Duration(10, 5, false) == 10);
    std::cout
        << "8.1–10 u/s travel range and 9 u/s playback, recorded-time and distance interpolation, cumulative seed drawing, immediate full path, persistent completed route / pulse / pause / reduced motion, replay short trails, gaps, start/end, repeated points, incomplete geometry, pause/restart/scrub, independent toggles and visible-route duration: PASS\n";
}
