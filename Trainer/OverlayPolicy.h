#pragma once
#include <algorithm>

enum class TrainerMode
{
    Normal,
    Practice,
    Speedrun
};

inline TrainerMode ResolveTrainerMode(bool speedrun, bool practice)
{
    return speedrun ? (practice ? TrainerMode::Practice : TrainerMode::Speedrun) : TrainerMode::Normal;
}

inline bool CompactHudAllowed(bool speedrun, bool practice)
{
    return ResolveTrainerMode(speedrun, practice) != TrainerMode::Speedrun;
}

inline bool SpeedrunHudAllowed(bool speedrun, bool practice)
{
    (void)practice;
    return speedrun; // A practice attempt also needs a mandatory UNVALID status.
}

// Independent of Win32 so focus transitions can be checked without a game.
struct OverlayState
{
    bool enabled = true, gameExists = false, gameVisible = false;
    bool minimized = false, cloaked = false, clientValid = false;
    bool gameForeground = false, overlayForeground = false;

    bool ShouldShow() const
    {
        return enabled && gameExists && gameVisible && !minimized && !cloaked && clientValid &&
               (gameForeground || overlayForeground);
    }
};

class KeyEdge
{
    bool down = false;

  public:
    bool Update(bool pressed, bool allowed)
    {
        bool edge = pressed && !down;
        down = pressed; // Consume presses in other apps instead of replaying them.
        return edge && allowed;
    }
};

inline int ClampRenderRate(int value)
{
    return value == 30 || value == 60 || value == 120 ? value : 60;
}
