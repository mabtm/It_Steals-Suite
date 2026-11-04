void FontSelectionCases()
{
    auto savedCatalog = suite_fonts::catalog;
    auto saved = suite_ui::appearance;
    auto &io = ImGui::GetIO();
    io.DisplaySize = {1000, 700};
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    suite_fonts::catalog.faces = {{"Suite:alpha.ttf#0", "Alpha face", "Suite", "alpha.ttf", 0},
                                  {"Windows:zeta.ttc#1", "Zeta condensed", "Windows", "zeta.ttc", 1}};
    suite_fonts::catalog.scanned = true;
    suite_ui::appearance.fontIds = {};
    suite_ui::fontsDirty = false;
    ImVec2 combo;
    auto frame = [&] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({30, 30});
        ImGui::SetNextWindowSize({600, 500});
        ImGui::Begin("Font selection QA");
        AppearanceFont("Interface font", suite_ui::appearance.menuFont, 0);
        if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup))
        {
            auto a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
            combo = {(a.x + b.x) * .5f, (a.y + b.y) * .5f};
        }
        ImGui::End();
        ImGui::Render();
    };
    for (int i = 0; i < 3; ++i)
        frame();
    io.AddMousePosEvent(combo.x, combo.y);
    frame();
    io.AddMouseButtonEvent(0, true);
    frame();
    io.AddMouseButtonEvent(0, false);
    frame();
    ImGuiWindow *popup = nullptr, *list = nullptr;
    for (auto *window : GImGui->Windows)
        if (window->LastFrameActive == ImGui::GetFrameCount())
        {
            if ((window->Flags & ImGuiWindowFlags_Popup) && std::strstr(window->Name, "##Combo_"))
                popup = window;
            if (std::strstr(window->Name, "/Font list_"))
                list = window;
        }
    assert(popup && list);
    auto search = popup->DC.CursorStartPos;
    io.AddMousePosEvent(search.x + 40, search.y + ImGui::GetFrameHeight() * .5f);
    frame();
    io.AddMouseButtonEvent(0, true);
    frame();
    io.AddMouseButtonEvent(0, false);
    frame();
    io.AddInputCharactersUTF8("Zeta");
    frame();
    frame();
    auto itemId = [&](const char *id, const char *name) {
        return ImHashStr(name, 0, ImHashStr(id, 0, list->IDStack.back()));
    };
    // A filtered-out row must not activate; selecting the visible collection
    // face stores its stable ID, rather than its position in the current list.
    ImGui::ActivateItemByID(itemId("Suite:alpha.ttf#0", "Alpha face (Suite)"));
    frame();
    assert(suite_ui::appearance.fontIds[0].empty());
    ImGui::ActivateItemByID(itemId("Windows:zeta.ttc#1", "Zeta condensed (Windows)"));
    frame();
    assert(suite_ui::appearance.fontIds[0] == "Windows:zeta.ttc#1" && suite_ui::fontsDirty);
    suite_ui::appearance = saved;
    suite_fonts::catalog = savedCatalog;
    suite_ui::fontsDirty = false;
    std::cout << "Actual font picker search, filtered row exclusion and collection-face selection: PASS\n";
}
