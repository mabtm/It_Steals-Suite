# Prüfung – PracticeSuite 7.11.2 / mab

| Bereich | Aktueller Nachweis |
| --- | --- |
| Details-Fehler | Produktionsmodell führt versetzte Object-/Transform-Felder stabil zusammen; jeder Wert genau einmal. Echte ImGui-Mausbedienung über Karte → Details; Header und Tabellen werden bei eingeschalteter ID-Konfliktprüfung gezielt gehovert. Kein Konflikt. |
| Visuelle Auswahl | Produktive Mesh-/Sprite-Abfrage gegen Engine-Vertragsdoubles: Colliderfreiheit, enge Dreiecke, nichtuniforme Transformation, dünne Bounds, Sprite-Flip/Resourcewechsel, statischer Batchbereich, Kameralayer, Aktivität, eigener Player, shared Cache, eine neue Kopie/Frame, Kandidaten- und Indexlimits. Keine Quelländerungen. Separater Helperbuild gegen originale SDK-Klassen. |
| Transport / native Logik | C#→C++ Fixtures einschließlich 2 Ebenen; Schema, UTF-8, Prüfsumme, jede Kürzung, 2000 Katalog-/Detail- und 1000 neu signierte Hover-Fuzzpakete. Szenen-/Session-/Pointer-Guards, Reconnect, Worker, Gruppen, Ebenenwechsel und exklusives Diagnose-JSON. JSON separat mit Python geparst. |
| Routenprobe | Vorhandene gemeinsame Boden-/Clearance-Prüfung in acht Modi, fehlender Boden, getrennte Komponenten, Budget und Abbruch. Routenalgorithmen unverändert. |
| Oberfläche | Vollständiges produktives ImGui: 168 Hauptseiten-/Viewport-/DPI-/Verbindungsfälle, 216 HUD-Fälle, 128 Modi, 288 Minimapfälle sowie Replay/Clip/Input/Finish/Recording-Check. Inspector bei 480/1920 und 100/150 % DPI, echte Details, Mausrad, Tab-Trefferliste, eingefrorener Weltstrahl beim Mausweg in die Liste, ausgewählte Ebenen-ID beim Commit, vorhandene Pin-/Cancel-/Fokus-/Clean-/Replay-Fälle. |
| Regressionen | Native core, settings, clip_camera, replay_motion einschließlich 7.11.1, replay_experience, physical_route, export_profile, hide_seek_player und seed_search. Managed core, physical_map, adaptive_capture, inspector. Historische übrige Prüfnachweise unverändert in den jeweiligen Versionsordnern; sie werden nicht als neue Läufe ausgegeben. |
| Speicher | Inspector-/Wire-/Fuzz-/Worker-/JSON-/Acht-Modi-Probe unter ASan + UBSan, halt_on_error=1, detect_leaks=0. Kein Leak-Scan. |
| Builds | C#7.2-Helper gegen originale Game-/Unity-/MelonLoader-/Harmony-Referenzen; Windows-x64-C++20/O2 statisch; Host-ImGui. Warnungen als Fehler. Gemeinsam generiertes Mainlayout geprüft. Encoder unverändert. |

Aktuelle Logs, Fixtures und Buildhashes: **tests/results-7.11.2**. Gerenderte produktive ImGui-Zeichenlisten mit synthetischen Objektdaten: **tests/previews-7.11.2**. Keine originalen Game-/SDK-/Loader-Referenzen im Download.

## Reproduktion

build.ps1 -GameDir <Spielordner> baut Helper und Trainer gegen eigene Referenzen. python generate-layout.py --check prüft das Hauptlayout. Native Tests: C++20, -ITrainer -ITrainer/imgui -pthread -Wall -Wextra -Werror; inspector_tests.cpp erhält Fixtureordner und frischen Ausgabeordner. inspector_tests.cs gegen Helper/SDK bauen und mit Fixtureordner ausführen. inspector_visual_tests.cs zusammen mit Mod/InspectorVisualPicking.cs und Mod/InspectorPicking.cs gegen UnityEngine.CoreModule.dll separat bauen, absichtliche Doubles mit -nowarn:0436; übrige Warnungen bleiben Fehler. Das sind Engine-Vertragsprüfungen ohne GPU/Unity-Spielprozess.

ui_tests benötigt einen absoluten Draw-Ausgabeordner, benachbarte managed-data/inspector-*.bin sowie bestehende Motion-/Audio-Fixtures aus results-7.9.0. Der optionale Schalter --inspector-only beschränkt auf die Inspector-Gruppe; der veröffentlichte Abschlusslauf umfasst alle Gruppen. tests/render_ui.py rendert die Draw-Dateien mit NumPy/Pillow. Vorhandene Nutzerdaten bleiben unangetastet.

## Praktische Grenzen

Kein Live-Windows-/Unity-Test und keine Messung von Spiel-/GPU-FPS. Reale Maus-/AltTab-/DPI-/Kamera-Viewport-Interaktion, tatsächliche Mesh-Lesbarkeit und dynamische Spielszene benötigen einen Zielsystemtest. Unity-ICall-Hinweise in Managed-Hostlogs sind erwartete Grenzen; geprüfte reine Logik-/Datenpfade bleiben davon getrennt.

Visuelle Auswahl testet lesbare Meshdreiecke und einfache Sprites, keine transparenten Pixel, Shader-Verformungen oder cooked Physikhüllen. Große/nicht lesbare/Skinned-Geometrie und ausgeschöpftes Dreiecksbudget sind klar als Bounds estimate markiert. Der Bildschirmrahmen ist eine Bounds-Projektion. Hierarchiekatalog und Bounds sind verteilt erfasste Stände, kein atomarer Szenensnapshot; bewegte colliderfreie Objekte können kurz nachlaufen. Neu erzeugte Objekte und gleichbleibende Mesh-Resources mit veränderter Topologie benötigen Refresh. Teilmengen werden angezeigt. Details/Watch sind beobachtete Werte, keine vollständige Kausalkette. Vollständige Budgets und Bedienung siehe LIVE_INSPECTOR.md.

Original Assembly-CSharp SHA256 ead3f5f861ad7988ad5fdb6f188bb3f77639927dd496b73066860d6c853f0dd8, Spiel9 / Unity2021.2.19f1. Main29 / Inspector3, Commands192 / Telemetrie16952. Map10 / MapRun13 / Input6 / Frame4 / Audio1 / ClipProject7 und Encoder unverändert. Keine neue H&S-Sonderregel, H&S/Outside bleiben WIP. Lokale Run-Sperren/Hilfen bleiben erhalten; keine Attestierung gegen manipulierte Prozesse.
