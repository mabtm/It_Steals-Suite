#include "TrainerPreferences.h"
#include "OverlayPolicy.h"
#include <cassert>
#include <iostream>

int main()
{
    using namespace suite_keys;
    assert(preferences.Enabled() == 255);
    assert(!NeedsPolling(true, 255) && NeedsPolling(true, 127) && !NeedsPolling(false, 0));
    assert(!Problem({'J', Control | Shift}));
    for (auto key : {Binding{0x70}, Binding{'T'}, Binding{'T', Control}, Binding{0x7b}, Binding{0x73, Alt},
                     Binding{9, Alt}, Binding{0x2e, Control | Alt}, Binding{0x5b}})
        assert(Problem(key));
    auto old = TrainerPreferences::Decode("version=19\nseed=17\n");
    assert(old.hotkeys.bindings == Defaults && old.seed == 17);
    Capture(Menu);
    Offer(0x11, Control);
    assert(recording == Menu);
    Offer(0x71, 0);
    assert(recording == Menu && !error.empty() && preferences.bindings[Menu] == Defaults[Menu]);
    Offer('J', Control | Shift);
    assert(recording == -1 && error.empty() && Hint(Menu) == "Ctrl+Shift+J");
    Capture(Picker);
    Offer(0x1b, 0);
    assert(recording == -1 && preferences.bindings[Picker] == Defaults[Picker]);
    Capture(Panels);
    Offer(8, 0);
    assert(!preferences.bindings[Panels].key && preferences.Enabled() == 254);
    assert(!NeedsPolling(true, 254));
    TrainerPreferences custom;
    custom.hotkeys = preferences;
    auto restored = TrainerPreferences::Decode(custom.Encode());
    assert(restored.hotkeys == preferences && restored.hotkeys.bindings[Menu].modifiers == 6);
    auto damaged = TrainerPreferences::Decode("hotkey.1.key=999\nhotkey.2.key=114\n");
    assert(damaged.hotkeys.bindings[Menu] == Defaults[Menu] && !damaged.hotkeys.bindings[Hud].key);
    KeyEdge edge;
    assert(!edge.Update(true, false) && !edge.Update(true, true));
    assert(!edge.Update(false, true) && edge.Update(true, true) && !edge.Update(true, true));
    preferences = {};
    std::cout
        << "Shortcut migration, exact modifiers, conflicts, reserved keys, capture/cancel/clear, persistence, focus edges and 8-bit polling mask: PASS\n";
}
