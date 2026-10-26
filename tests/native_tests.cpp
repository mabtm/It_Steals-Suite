#include "SharedProtocol.h"
#include "MinimapGeometry.h"
#include "OverlayPolicy.h"
#include "TrainerPreferences.h"
#include "TelemetryPresentation.h"
#include <cassert>
#include <cstring>
#include <limits>
#include <random>
#include <iostream>
#include <set>

int main()
{
    GameData replayOptions{};
    replayOptions.replay3dOptions = -1;
    replayOptions.replay3dFollowDistance = 99;
    replayOptions.replay3dFollowHeight = 99;
    SanitizeSnapshot(replayOptions);
    assert(kProtocolVersion == 30 && replayOptions.replay3dOptions == 0x7F03DBFF &&
           !(replayOptions.replay3dOptions & 1024) && replayOptions.replay3dFollowDistance == 20 &&
           replayOptions.replay3dFollowHeight == 0);
    replayOptions.replay3dFollowDistance = -10;
    SanitizeSnapshot(replayOptions);
    assert(replayOptions.replay3dFollowDistance == .75f);
    GameData inputs{};
    inputs.inputSequence = 19;
    inputs.inputSeconds = std::numeric_limits<double>::quiet_NaN();
    inputs.inputState = inputs.inputFlags = inputs.inputMouse = inputs.inputEventCount = -1;
    inputs.inputPadAxes[0] = 9;
    inputs.inputPadAxes[2] = -9;
    inputs.inputGameAxes[0] = std::numeric_limits<float>::infinity();
    SanitizeSnapshot(inputs);
    assert(sizeof(GameData) == 17860 && inputs.inputSequence == 0 && inputs.inputSeconds == 0 &&
           inputs.inputState == 2047 && inputs.inputFlags == 0x00ff070f && inputs.inputMouse == 127 &&
           inputs.inputEventCount == 0 && inputs.inputPadAxes[0] == 1 && inputs.inputPadAxes[2] == -1 &&
           inputs.inputGameAxes[0] == 0);
    // Actual MouseLook movement: Vertical * playerBody.right and
    // -Horizontal * playerBody.forward. Test body yaw, not generic Unity forward.
    MinimapOptions options;
    options.range = 40;
    for (float bodyYaw : {-360.f, -90.f, 0.f, 37.f, 90.f, 180.f, 270.f, 359.f, 360.f, 450.f})
    {
        MapTransform map({100, 100}, 80, options, 5, 6, bodyYaw, -50, 50, -50, 50);
        float angle = bodyYaw * 3.14159265358979323846f / 180;
        float forwardX = std::cos(angle), forwardZ = -std::sin(angle);
        auto forward = map.Project(5 + 10 * forwardX, 6 + 10 * forwardZ);
        assert(std::fabs(forward.x - 100) < .001f && std::fabs(forward.y - 80) < .001f);
        auto right = map.Project(5 - 10 * std::sin(angle), 6 - 10 * std::cos(angle));
        assert(std::fabs(right.x - 120) < .001f && std::fabs(right.y - 100) < .001f);
        // The North-up player arrow must match actual forward movement too.
        float arrowAngle = PlayerMapHeading(bodyYaw) * 3.14159265358979323846f / 180;
        assert(std::fabs(std::sin(arrowAngle) - forwardX) < .001f &&
               std::fabs(std::cos(arrowAngle) - forwardZ) < .001f);
    }
    options.playerUp = false;
    MapTransform north({100, 100}, 80, options, 0, 0, 123, -50, 50, -50, 50);
    assert(north.Project(0, 10).y == 80 && north.Project(10, 0).x == 120);
    options.follow = false;
    MapTransform overview({100, 100}, 80, options, 999, 999, 0, -40, 40, -30, 30);
    assert(overview.Project(0, 0).x == 100 && overview.Project(0, 0).y == 100);
    for (auto point : {ImVec2(-40, -30), ImVec2(40, 30)})
    {
        auto p = overview.Project(point.x, point.y);
        assert(std::hypot(p.x - 100, p.y - 100) <= 80.001);
    }
    MapTrail trail;
    GameData movement{};
    movement.playerReady = 1;
    movement.health = 2;
    movement.sceneEpoch = 1;
    for (int i = 0; i < 1000; i++)
    {
        movement.playerX = float(i);
        trail.Push(movement, 1, i * .11);
    }
    assert(trail.Size() == 128 && trail.At(127).x == 999);
    movement.lastTeleportSerial = 3;
    trail.Push(movement, 1, 110);
    assert(trail.Size() == 1);
    movement.sceneEpoch = 2;
    trail.Push(movement, 1, 110.11);
    assert(trail.Size() == 1);
    movement.playerReady = 0;
    trail.Push(movement, 1, 110.22);
    assert(trail.Size() == 0);
    TrainerPreferences extended;
    extended.map.zoom = 2;
    extended.map.playerUp = false;
    extended.reports = true;
    extended.input.enabled = extended.input.fullKeyboard = true;
    extended.input.x = .37f;
    extended.input.y = .61f;
    extended.input.opacity = .42f;
    extended.input.controllerIndex = 3;
    extended.input.clock = 2;
    extended.planner.seed = -123;
    extended.planner.recipe = 1;
    extended.planner.searchCount = 1000000;
    extended.planner.minimapRoute = false;
    extended.planner.searchCount = 1000000;
    // Two-core hosts reserve one core, so round-trip a supported worker count.
    extended.planner.searchWorkers = std::min(2, SeedWorkerLimit());
    extended.planner.generationMs = 3.25f;
    extended.planner.searchFast = true;
    extended.planner.searchObjective = 1;
    extended.planner.automaticLoad = false;
    extended.planner.calculateRoutes = true;
    extended.planner.level = 6;
    extended.planner.filters[0].catalog = 0xffff123489abcdefULL;
    extended.planner.filters[0].spawn = 1;
    extended.planner.filters[5].catalog = 0xffffffff01234567ULL;
    extended.planner.filters[5].spawn = 3;
    assert(TrainerPreferences::Decode(extended.Encode()).Encode() == extended.Encode());
    extended.frameLimitMode = 1;
    extended.vSyncMode = 1;
    extended.gameFrameLimit = 333;
    extended.hud.visible[25] = extended.speedrun.visible[25] = true;
    assert(TrainerPreferences::Decode(extended.Encode()).Encode() == extended.Encode());
    auto legacyCoordinate = TrainerPreferences::Decode("version=11\nhud.show25=1\nspeedrun.show25=1\n");
    assert(!legacyCoordinate.hud.visible[25] && !legacyCoordinate.speedrun.visible[25]);
    auto oldSearch = TrainerPreferences::Decode(
        "version=9\nplanner.searchFast=1\nplanner.searchCount=1234567\nplanner.searchWorkers=2\nplanner.generationMs=1.5\n");
    assert(oldSearch.planner.searchCount == 1234567 && oldSearch.planner.searchFast &&
           oldSearch.planner.searchObjective == 0 && oldSearch.planner.automaticLoad &&
           !oldSearch.planner.filters[0].HasRules());
    auto badFilters = TrainerPreferences::Decode(
        "planner.searchObjective=99\nplanner.maxRoute=-1\nplanner.maxOpening=999999999\nplanner.filter0.low=4294967296\nplanner.filter0.point1024=1\nplanner.filter0.point2=1.5\nplanner.filter0.point2junk=1\nplanner.filter6.point2=1\nplanner.filter0.spawn=999\n");
    assert(badFilters.planner.searchObjective == 0 && badFilters.planner.filters[0].catalog == 0 &&
           !badFilters.planner.filters[0].HasRules());
    auto fullFilters = extended;
    for (auto &filter : fullFilters.planner.filters)
        filter.spawn = 127;
    assert(fullFilters.Encode().size() < 262144 &&
           TrainerPreferences::Decode(fullFilters.Encode()).Encode() == fullFilters.Encode());
    auto largeCount = TrainerPreferences::Decode(
        "version=8\nplanner.searchCount=4294967296\nplanner.samples=2000\nplanner.heatmap=1\nplanner.searchWorkers=99999\nplanner.generationMs=999\n");
    assert(largeCount.planner.searchCount == (uint64_t{1} << 32) &&
           largeCount.planner.searchWorkers == SeedWorkerLimit() && largeCount.planner.generationMs == 8);
    assert(largeCount.Encode().find("planner.samples") == std::string::npos &&
           largeCount.Encode().find("planner.heatmap") == std::string::npos);
    assert(TrainerPreferences::Decode(largeCount.Encode()).Encode() == largeCount.Encode());
    auto invalidSearch = TrainerPreferences::Decode(
        "planner.searchCount=-5\nplanner.searchWorkers=-1\nplanner.generationMs=-1\n");
    assert(invalidSearch.planner.searchCount == 1 && invalidSearch.planner.searchWorkers == 1 &&
           invalidSearch.planner.generationMs == .25f);
    auto legacyGoals = TrainerPreferences::Decode(
        "version=10\nplanner.searchObjective=2\nplanner.filter0.point2=1\nplanner.filter0.spawn=1\n");
    assert(legacyGoals.planner.searchObjective == 1 && legacyGoals.planner.filters[0].spawn == 1 &&
           legacyGoals.Encode().find("point2") == std::string::npos);
    GameData timerActors{};
    timerActors.timerMonsterCount = 999;
    timerActors.timerMonsterOverflow = -1;
    timerActors.timerMonsterPositions[0] = NAN;
    timerActors.timerMonsterStates[1] = 99;
    timerActors.plannerGenerationMs = INFINITY;
    SanitizeSnapshot(timerActors);
    assert(timerActors.timerMonsterCount == 127 && timerActors.timerMonsterStates[0] == 0 &&
           timerActors.timerMonsterOverflow == 0 && timerActors.plannerGenerationMs == 0);
    // Migration drops retired metric IDs and old route keys, preserving all
    // surviving metrics in the user's order and their visibility.
    std::string old = "version=6\nroute0.enabled=1\nhud.show25=1\nhud.show0=0\n";
    for (int i = 0; i < 29; i++)
        old += "hud.order" + std::to_string(i) + "=" + std::to_string(28 - i) + "\n";
    auto migrated = TrainerPreferences::Decode(old);
    for (int i = 0; i < 25; i++)
        assert(migrated.hud.order[i] == 24 - i);
    assert(migrated.hud.order[25] == 25 && !migrated.hud.visible[25]);
    assert(!migrated.hud.visible[0] && migrated.Encode().find("route0.") == std::string::npos);
    assert(CompactHudAllowed(false, false) && !SpeedrunHudAllowed(false, false));
    assert(CompactHudAllowed(true, true) && SpeedrunHudAllowed(true, true));
    assert(!CompactHudAllowed(true, false) && SpeedrunHudAllowed(true, false));
    // A leftover practice preference while Speedrun is off stays Normal mode.
    assert(CompactHudAllowed(false, true) && !SpeedrunHudAllowed(false, true));
    OverlayState window;
    window.gameExists = window.gameVisible = window.clientValid = window.gameForeground = true;
    assert(window.ShouldShow());
    window.gameForeground = false;
    assert(!window.ShouldShow()); // Alt+Tab, game still visible.
    window.overlayForeground = true;
    assert(window.ShouldShow()); // Click the trainer menu.
    window.overlayForeground = false;
    assert(!window.ShouldShow()); // Another app from the menu.
    window.gameForeground = true;
    assert(window.ShouldShow()); // Return to the game.
    window.minimized = true;
    assert(!window.ShouldShow());
    window.minimized = false;
    window.cloaked = true;
    assert(!window.ShouldShow());
    window.cloaked = false;
    window.clientValid = false;
    assert(!window.ShouldShow());
    window.clientValid = true;
    window.gameVisible = false;
    assert(!window.ShouldShow());
    window.gameVisible = true;
    window.gameExists = false;
    assert(!window.ShouldShow());
    window.gameExists = true;
    window.enabled = false;
    assert(!window.ShouldShow());
    window.gameForeground = false;
    window.gameForeground = true;
    assert(!window.ShouldShow()); // Manual hiding survives focus changes.
    window.enabled = true;
    assert(window.ShouldShow());
    KeyEdge key;
    assert(!key.Update(true, false));
    assert(!key.Update(true, true)); // No replay of an outside-game press.
    assert(!key.Update(false, true));
    assert(key.Update(true, true));
    assert(!key.Update(true, true));
    assert(ClampRenderRate(144) == 60 && ClampRenderRate(30) == 30);
    TrainerPreferences preferences;
    preferences.seed = -2147483647;
    preferences.level = 5;
    preferences.fps = 120;
    preferences.opacity = .35f;
    preferences.width = 340;
    preferences.height = 340;
    auto restored = TrainerPreferences::Decode(preferences.Encode());
    assert(restored.Encode() == preferences.Encode());
    restored = TrainerPreferences::Decode(
        "width=999999999999999999\nheight=-500\nfps=999\nopacity=nan\nlevel=999\nseed=123garbage\nbackground0=1e300\n");
    assert(restored.width == 500 && restored.height == 500 && restored.fps == 60 && restored.level == 7 &&
           restored.seed == 0);
    assert(std::isfinite(restored.background[0]) && restored.background[0] == 1);
    preferences.hud.visible[static_cast<int>(OverlayField::Seed)] = true;
    preferences.hud.visible[static_cast<int>(OverlayField::Fps)] = false;
    preferences.speedrun.anchor = 4;
    preferences.speedrun.x = .7f;
    std::swap(preferences.hud.order[0], preferences.hud.order[4]);
    assert(TrainerPreferences::Decode(preferences.Encode()).Encode() == preferences.Encode());
    restored = TrainerPreferences::Decode(
        "hud.order0=-1\nhud.order1=0\nhud.order2=0\nhud.scale=1e300\nspeedrun.opacity=-9\nspeedrun.x=nan\n");
    std::set<int> ordered(restored.hud.order.begin(), restored.hud.order.end());
    assert(ordered.size() == kOverlayFields && restored.hud.scale == 1.75f &&
           restored.speedrun.opacity == .25f);
    TelemetryPresentation smooth;
    GameData pose{};
    pose.currentSceneIndex = 3;
    pose.playerRotY = 359;
    smooth.Push(pose, 1, 10);
    pose.playerX = 10;
    pose.playerRotY = 1;
    smooth.Push(pose, 1.02, 10.02);
    auto half = smooth.Sample(10.03);
    assert(std::fabs(half.x - 5) < .001f && std::fabs(half.heading - 360) < .001f);
    assert(smooth.Sample(12).x == 10); // No extrapolation of stale positions.
    pose.playerX = 1000;
    smooth.Push(pose, 1.04, 10.04);
    assert(smooth.Sample(10.04).x == 1000);
    pose.currentSceneIndex = 8;
    pose.playerX = 0;
    smooth.Push(pose, 1.06, 10.06);
    assert(smooth.Sample(10.06).x == 0);
    pose.timerState = 1;
    pose.currentRunTimeSeconds = 10;
    assert(DisplayRunTime(pose, 2) == 10.15);
    pose.timerState = 2;
    assert(DisplayRunTime(pose, 2) == 10);
    std::vector<std::pair<ImVec2, ImVec2>> edges;
    ExtractUniqueEdges({}, 200, edges);
    assert(edges.empty());
    ExtractUniqueEdges(std::vector<float>(800, 0), 200, edges);
    assert(edges.empty());
    ExtractUniqueEdges({0, 0, 2, 2}, -1, edges);
    assert(edges.empty());
    ExtractUniqueEdges({0, 0, 2, 2}, 200, edges);
    assert(edges.size() == 4);
    ExtractUniqueEdges({0, 0, 2, 2, 2, 0, 4, 2}, 2, edges);
    for (auto edge : edges)
        assert(!(edge.first.x == 2 && edge.second.x == 2));
    ImVec2 a{2, 2}, b{2, 2};
    assert(!ClipLineToCircle(a, b, {0, 0}, 1));
    a = {-2, 0};
    b = {2, 0};
    assert(ClipLineToCircle(a, b, {0, 0}, 1));
    assert(std::fabs(a.x + 1) < .0001f && std::fabs(b.x - 1) < .0001f);
    auto polygon = ClipPolygonToCircle({{-2, -2}, {2, -2}, {2, 2}, {-2, 2}}, {0, 0}, 1);
    assert(polygon.size() == 32);
    for (ImVec2 point : polygon)
        assert(point.x * point.x + point.y * point.y <= 1.0001f);
    assert(ClipPolygonToCircle({{3, 3}, {4, 3}, {4, 4}, {3, 4}}, {0, 0}, 1).empty());
    std::vector<ImVec2> reusable, scratch;
    for (int i = 0; i < 100; i++)
        ClipPolygonToCircleReusable({{-2, -2}, {2, -2}, {2, 2}, {-2, 2}}, {0, 0}, 1, reusable, scratch);
    assert(reusable.size() == 32 && reusable.capacity() <= 80 && scratch.capacity() <= 80);
    assert(ClipPolygonToCircle({{0, 0}}, {0, 0}, -1).empty());
    GameData data{};
    data.pathCornerCount = std::numeric_limits<int>::max();
    data.wallCount = -7;
    data.crystalCount = 999;
    data.currentRunTimeSeconds = std::numeric_limits<double>::quiet_NaN();
    SanitizeSnapshot(data);
    assert(data.pathCornerCount == 20 && data.wallCount == 0 && data.crystalCount == 100 &&
           data.currentRunTimeSeconds == 0);
    std::mt19937 random(12345);
    for (int iteration = 0; iteration < 10000; iteration++)
    {
        byte *bytes = reinterpret_cast<byte *>(&data);
        for (size_t i = 0; i < sizeof(data); i++)
            bytes[i] = static_cast<byte>(random());
        SanitizeSnapshot(data);
        assert(data.crystalCount >= 0 && data.crystalCount <= 100);
        assert(data.wallCount >= 0 && data.wallCount <= 200);
        assert(data.pathCornerCount >= 0 && data.pathCornerCount <= 20);
        assert(std::isfinite(data.playerRotY) && std::isfinite(data.smoothedSpeed));
        assert(data.objectives >= 0 && data.objectives <= data.totalObjectives);
        assert(data.historyCount >= 0 && data.historyCount <= 16 && data.seedKind <= 2);
        assert(data.runValidity >= 1 && data.runValidity <= 2 && data.externalScanStatus <= 3 &&
               data.reviewSamples >= 0 && data.reviewSamples <= 32768 && data.wallOutlineCount >= 0 &&
               data.wallOutlineCount <= 200);
        assert(data.viewCamera[63] == 0 && data.externalTool[95] == 0 && data.validationDetail[191] == 0 &&
               std::isfinite(data.gameRefreshHz));
        for (int i = 0; i < data.wallOutlineCount * 8; i++)
            assert(ValidCoordinate(data.wallOutlines[i]));
        if (data.historyCount)
        {
            auto record = ReadHistory(data, 0);
            assert(std::isfinite(record.seconds) && std::isfinite(record.progress));
        }
        std::vector<float> boxes(data.wallBoxes, data.wallBoxes + data.wallCount * 4);
        ExtractUniqueEdges(boxes, data.wallCount, edges);
    }
    std::cout
        << "Focus, keys, config migration, heading transforms, bounded trail, malformed snapshots and geometry: PASS (10,000 fuzz cases)\n";
}
