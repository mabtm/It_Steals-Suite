#include "TrainerPreferences.h"
#include "OverlayPolicy.h"
#include <cassert>
#include <iostream>

int main()
{
    using namespace suite_keys;
    assert(Count == 9 && Restart == 8 && preferences.Enabled() == 511);
    assert((Defaults[Restart] == Binding{'T'}));
    assert(!NeedsPolling(true, 511) && NeedsPolling(true, 255) && !NeedsPolling(false, 0));
    assert(!NeedsPolling(true, 255, ~(1u << Restart)));
    assert(!Problem({'J', Control | Shift}));
    assert(!Problem({'T'}) && !Problem({'T', Control}));
    for (auto key : {Binding{0x70}, Binding{0x7b}, Binding{0x73, Alt},
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
    assert(!preferences.bindings[Panels].key && preferences.Enabled() == 510);
    assert(!NeedsPolling(true, 510));
    // Existing action indices and settings remain stable; the newly appended
    // restart gets T when loading settings written by the eight-action trainer.
    auto previous = TrainerPreferences::Decode("hotkey.1.key=74\nhotkey.1.modifiers=2\nhotkey.7.key=118\n");
    assert((previous.hotkeys.bindings[Menu] == Binding{'J', Control}));
    assert(previous.hotkeys.bindings[Picker] == Defaults[Picker]);
    assert(previous.hotkeys.bindings[Restart] == Defaults[Restart]);
    Capture(Menu);
    Offer('T', 0);
    assert(recording == Menu && !error.empty());
    Offer(0x1b, 0);
    Capture(Restart);
    Offer('R', Control);
    assert(recording == -1 && Hint(Restart) == "Ctrl+R");
    Capture(Menu);
    Offer('T', 0);
    assert(recording == -1 && Hint(Menu) == "T");
    TrainerPreferences custom;
    custom.hotkeys = preferences;
    auto restored = TrainerPreferences::Decode(custom.Encode());
    assert(restored.hotkeys == preferences && restored.hotkeys.bindings[Restart].modifiers == Control);
    assert((restored.hotkeys.bindings[Restart] == Binding{'R', Control}));
    Capture(Restart);
    Offer(8, 0);
    assert(!preferences.bindings[Restart].key);
    custom.hotkeys = preferences;
    assert(!TrainerPreferences::Decode(custom.Encode()).hotkeys.bindings[Restart].key);
    auto damaged = TrainerPreferences::Decode("hotkey.1.key=999\nhotkey.2.key=114\n");
    assert(damaged.hotkeys.bindings[Menu] == Defaults[Menu] && !damaged.hotkeys.bindings[Hud].key);
    KeyEdge edge;
    assert(!edge.Update(true, false) && !edge.Update(true, true));
    assert(!edge.Update(false, true) && edge.Update(true, true) && !edge.Update(true, true));
    preferences = {};
    std::cout
        << "Shortcut migration, quick restart/reset rebinding, exact modifiers, conflicts, reserved keys, capture/cancel/clear, persistence, focus edges and 9-bit polling mask: PASS\n";
}
