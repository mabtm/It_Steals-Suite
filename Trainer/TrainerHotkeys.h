#pragma once
#include <array>
#include <cstdio>
#include <ostream>
#include <string>

namespace suite_keys
{
enum Action
{
    Panels,
    Menu,
    Hud,
    RefreshMap,
    Borderless,
    Speedrun,
    Analyze,
    Picker,
    Count
};

enum Modifier
{
    Alt = 1,
    Control = 2,
    Shift = 4
};

inline constexpr const char *Actions[] = {"Toggle overlay",       "Open / close menu",     "Compact HUD",
                                          "Refresh map",          "Borderless fullscreen", "Speedrun mode",
                                          "Analyze finished run", "Inspector picker"};

struct Binding
{
    int key = 0, modifiers = 0;
    bool operator==(const Binding &) const = default;
};

inline constexpr std::array<Binding, Count> Defaults = {Binding{0x71}, Binding{0x72}, Binding{0x73},
                                                        Binding{0x74}, Binding{0x75}, Binding{'T', Shift},
                                                        Binding{0x77}, Binding{0x76}};
inline std::string (*nativeKeyName)(int) = nullptr;

inline const char *Problem(Binding b)
{
    if (!b.key && !b.modifiers)
        return nullptr;
    bool key = (b.key >= '0' && b.key <= '9') || (b.key >= 'A' && b.key <= 'Z') ||
               (b.key >= 0x60 && b.key <= 0x87) || b.key == 9 || b.key == 13 ||
               (b.key >= 0x20 && b.key <= 0x28) || b.key == 0x2d || b.key == 0x2e ||
               (b.key >= 0xba && b.key <= 0xc0) || (b.key >= 0xdb && b.key <= 0xdf) || b.key == 0xe2;
    if (!key || b.modifiers < 0 || b.modifiers > 7)
        return "Choose a letter, number, function or navigation key.";
    if (b.key == 0x70 || (b.key == 'T' && !(b.modifiers & Shift)))
        return "F1 (Freecam) and T (restart) are game controls. Choose another shortcut.";
    if (b.key == 0x7b || (b.key == 0x73 && (b.modifiers & Alt)) || (b.key == 9 && (b.modifiers & Alt)) ||
        (b.key == 0x2e && (b.modifiers & 3) == 3))
        return "This combination is reserved by Windows.";
    return nullptr;
}

inline std::string Name(Binding b)
{
    if (!b.key)
        return "Unassigned";
    std::string name;
    if (b.modifiers & Control)
        name += "Ctrl+";
    if (b.modifiers & Alt)
        name += "Alt+";
    if (b.modifiers & Shift)
        name += "Shift+";
    if ((b.key >= '0' && b.key <= '9') || (b.key >= 'A' && b.key <= 'Z'))
        return name + char(b.key);
    if (b.key >= 0x70 && b.key <= 0x87)
        return name + "F" + std::to_string(b.key - 0x6f);
    if (b.key >= 0x60 && b.key <= 0x69)
        return name + "Num " + std::to_string(b.key - 0x60);
    switch (b.key)
    {
    case 9:
        return name + "Tab";
    case 13:
        return name + "Enter";
    case 0x20:
        return name + "Space";
    case 0x21:
        return name + "Page Up";
    case 0x22:
        return name + "Page Down";
    case 0x23:
        return name + "End";
    case 0x24:
        return name + "Home";
    case 0x25:
        return name + "Left";
    case 0x26:
        return name + "Up";
    case 0x27:
        return name + "Right";
    case 0x28:
        return name + "Down";
    case 0x2d:
        return name + "Insert";
    case 0x2e:
        return name + "Delete";
    case 0x6a:
        return name + "Num *";
    case 0x6b:
        return name + "Num +";
    case 0x6d:
        return name + "Num -";
    case 0x6e:
        return name + "Num .";
    case 0x6f:
        return name + "Num /";
    }
    if (nativeKeyName)
    {
        auto key = nativeKeyName(b.key);
        if (!key.empty())
            return name + key;
    }
    char key[16];
    std::snprintf(key, sizeof(key), "Key 0x%02X", b.key);
    return name + key;
}

struct Preferences
{
    std::array<Binding, Count> bindings = Defaults;
    bool operator==(const Preferences &) const = default;

    int Conflict(Binding b, int except) const
    {
        for (int i = 0; b.key && i < Count; i++)
            if (i != except && bindings[i] == b)
                return i;
        return -1;
    }

    void Validate()
    {
        for (int i = 0; i < Count; i++)
        {
            if (Problem(bindings[i]))
                bindings[i] = Defaults[i];
            for (int j = 0; j < i; j++)
                if (bindings[i].key && bindings[i] == bindings[j])
                    bindings[i] = {};
        }
    }

    unsigned Enabled() const
    {
        unsigned mask = 0;
        for (int i = 0; i < Count; i++)
            if (bindings[i].key)
                mask |= 1u << i;
        return mask;
    }

    void Encode(std::ostream &out) const
    {
        for (int i = 0; i < Count; i++)
            out << "hotkey." << i << ".key=" << bindings[i].key << "\nhotkey." << i
                << ".modifiers=" << bindings[i].modifiers << '\n';
    }

    void Decode(const std::string &key, int value)
    {
        for (int i = 0; i < Count; i++)
        {
            if (key == std::to_string(i) + ".key")
                bindings[i].key = value;
            if (key == std::to_string(i) + ".modifiers")
                bindings[i].modifiers = value;
        }
    }
};

inline Preferences preferences;
inline int recording = -1;
inline bool openRequested = false;
inline unsigned unavailable = 0;
inline std::string error;

inline const std::string &Hint(Action action)
{
    static std::array<Binding, Count> previous{};
    static std::array<std::string, Count> names;
    auto binding = preferences.bindings[action];
    if (names[action].empty() || binding != previous[action])
    {
        previous[action] = binding;
        names[action] = Name(binding);
    }
    return names[action];
}

inline std::string Label(const char *text, Action action)
{
    return std::string(text) + " [" + Hint(action) + "]";
}

inline void Capture(int action)
{
    recording = action;
    openRequested = true;
    error.clear();
}

// Called by the native key-message handler, before game/UI actions are dispatched.
inline void Offer(int key, int modifiers)
{
    if (recording < 0)
        return;
    if (key == 0x1b)
    {
        recording = -1;
        return;
    }
    if (key == 0x10 || key == 0x11 || key == 0x12 || (key >= 0xa0 && key <= 0xa5))
        return;
    Binding candidate = key == 8 ? Binding{} : Binding{key, modifiers};
    if (auto problem = Problem(candidate))
    {
        error = problem;
        return;
    }
    int conflict = preferences.Conflict(candidate, recording);
    if (conflict >= 0)
    {
        error = std::string("Already assigned to: ") + Actions[conflict];
        return;
    }
    preferences.bindings[recording] = candidate;
    recording = -1;
    error.clear();
}

inline bool NeedsPolling(bool allowed, unsigned registered)
{
    return allowed && (preferences.Enabled() & ~registered) != 0;
}
} // namespace suite_keys
