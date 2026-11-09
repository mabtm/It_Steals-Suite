#include "TrainerPreferences.h"
#include "OverlayPolicy.h"
#include <cassert>
#include <iostream>

int main()
{
    using namespace suite_keys;
    assert(Count == 10 && Restart == 8 && FullReset == 9 && preferences.Enabled() == 1023);
    assert((Defaults[Restart] == Binding{'T'}));
    assert(!NeedsPolling(true, 1023) && NeedsPolling(true, 255) && !NeedsPolling(false, 0));
    assert(!NeedsPolling(true, 255, ~((1u << Restart) | (1u << FullReset))));
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
    assert(!preferences.bindings[Panels].key && preferences.Enabled() == 1022);
    assert(!NeedsPolling(true, 1022));
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
    preferences={};
    for(int i=0;i<5;i++)
    {
        int key=MouseKeys[i];assert(MouseKey(key) && !Problem({key,Control|Shift}));
        assert(Name({key,Control}).starts_with("Ctrl+M"+std::to_string(i+1)));
        Capture(Restart);OfferMouse(1u<<i,1u<<i,0,false);
        assert(recording==Restart); // wait for the opening/held click to release
        OfferMouse(0,0,0,false);
        OfferMouse(1u<<i,1u<<i,0,true);
        assert(recording==Restart); // clicking a modal control must not bind
        OfferMouse(1u<<i,1u<<i,Control|Shift,false);
        assert(recording==-1 && (preferences.bindings[Restart]==Binding{key,Control|Shift}));
        TrainerPreferences mouse;mouse.hotkeys=preferences;
        assert(TrainerPreferences::Decode(mouse.Encode()).hotkeys==preferences);
        assert(DispatchAllowed({key},Restart,false) && !DispatchAllowed({key},Restart,true));
        assert(DispatchAllowed({key},Menu,true)==(i>=3));
    }
    assert(!MouseKey(3) && !MouseKey(7));
    assert(DispatchAllowed({'T'},Restart,true));
    Capture(Menu);Offer(MouseKeys[4],Control|Shift);
    assert(recording==Menu && !error.empty()); // mouse conflicts share keyboard rules
    Offer(MouseKeys[3],Alt);assert(recording==-1 && Hint(Menu)=="Alt+M4 / Mouse back");
    preferences = {};
    std::cout
        << "Shortcut migration, quick/full reset rebinding, M1-M5 capture/release/control guards, exact modifiers, conflicts, persistence, focus edges and 10-bit polling mask: PASS\n";
}
