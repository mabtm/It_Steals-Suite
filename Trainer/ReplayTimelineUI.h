#pragma once
#include "SuiteDesign.h"
#include "RoutePlayback.h"
#include "ReplayAnnotations.h"
#include "ReplayMotion.h"

inline ImRect RenderReplayTimeline(const practice::RunReview &review, practice::ReviewPlayback &replay,
                                   double duration, const practice::ReplayAnnotations *annotations = nullptr,
                                   const practice::ReplayMotion *motion = nullptr)
{
    duration = suite_ui::SafeDuration(duration);
    float u = suite_ui::Unit();
    if (suite_ui::Button(replay.playing ? "Pause" : "Play"))
    {
        if (!replay.playing && replay.seconds >= duration)
            replay.Restart();
        else
            replay.playing = !replay.playing;
    }
    SameLineIfFits("Restart");
    if (suite_ui::Button("Restart"))
        replay.Restart();
    char clock[100];
    snprintf(clock, sizeof(clock), "%s  /  %s", FormatRunTime(replay.seconds).c_str(),
             FormatRunTime(duration).c_str());
    SameLineIfFits(clock);
    ImGui::TextDisabled("%s", clock);
    auto a = ImGui::GetCursorScreenPos();
    float width = std::max(20.f, ImGui::GetContentRegionAvail().x), h = 68 * u, pad = 8 * u,
          trackWidth = std::max(1.f, width - 2 * pad), y = a.y + 27 * u;

    struct Cache
    {
        const practice::RunReview *review = nullptr;
        int64_t id = 0;
        double span = -1;
        int pixels = 0;
        size_t pickups = 0, flashes = 0;
        suite_ui::TimelineMarkers markers;
    };

    static Cache cache;
    int pixels = std::clamp(int(trackWidth), 1, 2048);
    if (cache.review != &review || cache.id != review.id || cache.span != duration ||
        cache.pixels != pixels || cache.pickups != review.pickupSeconds.size() ||
        cache.flashes != review.flashes.size())
    {
        cache.review = &review;
        cache.id = review.id;
        cache.span = duration;
        cache.pixels = pixels;
        cache.pickups = review.pickupSeconds.size();
        cache.flashes = review.flashes.size();
        cache.markers.Reset(pixels, duration);
        for (auto time : review.pickupSeconds)
            cache.markers.Add(time, false);
        for (const auto &flash : review.flashes)
            cache.markers.Add(flash.seconds, true);
    }
    ImGui::InvisibleButton("##Review timeline", {width, h}, ImGuiButtonFlags_EnableNav);
    bool hovered = ImGui::IsItemHovered(), active = ImGui::IsItemActive(),
         activated = ImGui::IsItemActivated();
    float mx = ImGui::GetIO().MousePos.x;
    double fraction = std::clamp((double(mx) - a.x - pad) / trackWidth, 0., 1.);
    int closestBin =
        hovered ? cache.markers.Nearest(fraction, std::max(3, int(7 * u * pixels / trackWidth))) : -1;
    const practice::ReplayMarker *note = nullptr;
    float closest = 8 * u;
    if (hovered && annotations && ImGui::GetIO().MousePos.y < a.y + 19 * u)
        for (const auto &m : annotations->markers)
        {
            float distance =
                std::abs(float(m.seconds / std::max(.000001, duration)) * trackWidth + a.x + pad - mx);
            if (distance < closest)
            {
                closest = distance;
                note = &m;
            }
        }
    if (hovered && ImGui::GetIO().MouseWheel != 0)
    {
        replay.seconds = std::clamp(
            replay.seconds - ImGui::GetIO().MouseWheel * (ImGui::GetIO().KeyShift ? 1. : .1), 0., duration);
        replay.playing = false;
        ImGui::SetKeyOwner(ImGuiKey_MouseWheelY, ImGui::GetItemID());
    }
    if (activated && ImGui::IsMouseDown(0))
    {
        replay.seconds = suite_ui::TimelineAt(mx, a.x + pad, trackWidth, duration);
        if (note && ImGui::GetIO().MousePos.y < a.y + 19 * u)
            replay.seconds = note->seconds;
        else if (closestBin >= 0)
        {
            auto b = cache.markers.bins[closestBin];
            bool flash = ImGui::GetIO().MousePos.y < y;
            replay.seconds = flash && b.flashes ? b.flash : b.pickups ? b.pickup : b.flash;
        }
        replay.playing = false;
    }
    else if (active && ImGui::IsMouseDragging(0, 2 * u))
    {
        replay.seconds = suite_ui::TimelineAt(mx, a.x + pad, trackWidth, duration);
        replay.playing = false;
    }
    if (ImGui::IsItemFocused())
    {
        auto id = ImGui::GetItemID();
        for (auto key : {ImGuiKey_LeftArrow, ImGuiKey_RightArrow, ImGuiKey_Home, ImGuiKey_End})
            ImGui::SetKeyOwner(key, id, ImGuiInputFlags_LockThisFrame);
        auto pressed = [&](ImGuiKey key) { return ImGui::IsKeyPressed(key, ImGuiInputFlags_Repeat, id); };
        bool seeking = false;
        double step = ImGui::GetIO().KeyShift ? 1 : std::max(.01, duration / 100);
        if (pressed(ImGuiKey_LeftArrow))
        {
            seeking = true;
            replay.seconds = std::max(0., replay.seconds - step);
            replay.playing = false;
        }
        if (pressed(ImGuiKey_RightArrow))
        {
            seeking = true;
            replay.seconds = std::min(duration, replay.seconds + step);
            replay.playing = false;
        }
        if (pressed(ImGuiKey_Home))
        {
            seeking = true;
            replay.seconds = 0;
            replay.playing = false;
        }
        if (pressed(ImGuiKey_End))
        {
            seeking = true;
            replay.seconds = duration;
            replay.playing = false;
        }
        if (seeking)
            ImGui::NavMoveRequestCancel();
    }
    auto *d = ImGui::GetWindowDrawList();
    suite_ui::Surface(d, a, {a.x + width, a.y + h});
    auto color = [&](ImVec4 c) { return ImGui::GetColorU32(c); };
    auto muted = suite_ui::Blend(suite_ui::ink, suite_ui::background, .42f);
    d->AddLine({a.x + pad, y}, {a.x + pad + trackWidth, y},
               color(suite_ui::Alpha(suite_ui::Color(AppearancePreferences::Outline), .8f)), 3 * u);
    if (annotations && annotations->LoopValid())
    {
        float left = a.x + pad + trackWidth * float(annotations->a / std::max(.000001, duration)),
              right = a.x + pad + trackWidth * float(annotations->b / std::max(.000001, duration));
        d->AddRectFilled({left, y - 4 * u}, {right, y + 4 * u},
                         color(suite_ui::Alpha(suite_ui::heading, annotations->loop ? .3f : .12f)), 3 * u);
        d->AddLine({left, y - 19 * u}, {left, y + 17 * u}, color(suite_ui::heading), u);
        d->AddLine({right, y - 19 * u}, {right, y + 17 * u}, color(suite_ui::heading), u);
    }
    if (annotations)
        for (const auto &m : annotations->markers)
        {
            float px = a.x + pad + trackWidth * float(m.seconds / std::max(.000001, duration));
            ImVec2 marker[] = {
                {px, a.y + 4 * u}, {px + 5 * u, a.y + 9 * u}, {px, a.y + 14 * u}, {px - 5 * u, a.y + 9 * u}};
            d->AddConvexPolyFilled(marker, 4, color(g_accentCol));
        }
    for (int i = 0; i <= 4; i++)
    {
        float x = a.x + pad + trackWidth * i / 4;
        d->AddLine({x, y - 3 * u}, {x, y + 3 * u}, color(muted), u);
    }
    for (int i = 0; i < cache.markers.width; i++)
    {
        const auto &b = cache.markers.bins[i];
        float x =
            a.x + pad + trackWidth * (cache.markers.width > 1 ? float(i) / (cache.markers.width - 1) : 0);
        if (b.pickups)
        {
            ImVec2 gem[] = {{x, y + 5 * u}, {x + 3 * u, y + 9 * u}, {x, y + 13 * u}, {x - 3 * u, y + 9 * u}};
            d->AddConvexPolyFilled(gem, 4, color(suite_ui::Color(AppearancePreferences::MapCrystal)));
        }
        if (b.flashes)
            d->AddLine({x, y - 13 * u}, {x, y - 6 * u}, color(suite_ui::ink), 2 * u);
    }

    struct GameCache
    {
        const practice::ReplayMotion *source = nullptr;
        int64_t id = 0;
        size_t count = 0;
        std::array<uint8_t, 512> bins{};
    };

    static GameCache game;
    if (motion &&
        (game.source != motion || game.id != motion->id || game.count != motion->experience.events.size()))
    {
        game = {motion, motion->id, motion->experience.events.size(), {}};
        for (const auto &e : motion->experience.events)
            game.bins[size_t(std::clamp(int(e.seconds / std::max(.000001, duration) * 511), 0, 511))] |=
                e.kind == 1   ? 1
                : e.kind == 2 ? 2
                              : 0;
    }
    if (motion && motion->experience.recorded)
        for (size_t i = 0; i < game.bins.size(); i++)
        {
            float px = a.x + pad + trackWidth * float(i) / 511;
            if (game.bins[i] & 1)
                d->AddLine({px, y + 16 * u}, {px, y + 20 * u}, color(g_accentCol), u);
            if (game.bins[i] & 2)
                d->AddCircleFilled({px, y - 19 * u}, 1.5f * u, color(suite_ui::heading), 6);
        }
    float progress = duration > 0 ? float(std::clamp(replay.seconds / duration, 0., 1.)) : 0;
    float x = a.x + pad + trackWidth * progress;
    d->AddLine({a.x + pad, y}, {x, y}, color(suite_ui::heading), 3 * u);
    d->AddCircleFilled({x, y}, 5 * u, color(suite_ui::heading), 16);
    d->AddCircle({x, y}, 8 * u, color(suite_ui::Alpha(suite_ui::heading, .3f)), 20, u);
    char legend[130];
    snprintf(legend, sizeof(legend), "%zu pickups   /   %zu flashes   /   %zu notes",
             review.pickupSeconds.size(), review.flashes.size(),
             annotations ? annotations->markers.size() : 0);
    suite_ui::TextAt(d, {a.x + pad, a.y + 46 * u}, legend, muted, 14 * u,
                     {a.x, a.y, a.x + width - 8 * u, a.y + h});
    if (ImGui::IsItemFocused())
        d->AddRect(a, {a.x + width - 2 * u, a.y + h - 3 * u}, color(suite_ui::heading), 8 * u, 0, u);
    if (hovered)
    {
        ImGui::BeginTooltip();
        if (note)
        {
            ImGui::TextColored(g_accentCol, "Annotation: %s", note->name.c_str());
            ImGui::TextUnformatted(FormatRunTime(note->seconds).c_str());
        }
        else if (closestBin >= 0)
        {
            auto b = cache.markers.bins[closestBin];
            if (b.pickups)
                ImGui::Text("Pickup: %s (%u)", FormatRunTime(b.pickup).c_str(), b.pickups);
            if (b.flashes)
                ImGui::Text("Light event: %s (%u)", FormatRunTime(b.flash).c_str(), b.flashes);
        }
        else
            ImGui::Text("Seek to %s",
                        FormatRunTime(suite_ui::TimelineAt(mx, a.x + pad, trackWidth, duration)).c_str());
        ImGui::TextDisabled("Click event / drag to scrub. Arrows: seek; Shift: 1 s.");
        if (motion && motion->experience.recorded)
            ImGui::TextDisabled("Top dots: game audio events. Bottom ticks: observed state changes.");
        ImGui::EndTooltip();
    }
    return ImRect(a, {a.x + width, a.y + h});
}
