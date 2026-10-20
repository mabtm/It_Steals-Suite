# Codeprüfung – PracticeSuite 7.11.2 / mab

## Details

Der Helper lieferte Grundfelder als Object → Transform → Object und erweiterte Transform-Felder später erneut. Properties machte daraus fortlaufend gruppierte Header/Tabellen mit denselben Labels als IDs. GroupValues führt Gruppen nun nach erstem Auftreten zusammen und behält die Feldreihenfolge innerhalb der Gruppe bei. UI cached das Ergebnis pro unveränderlichem Detail-Snapshot; Group-ID, Tabelle und Feldzeilen haben eindeutige Scopes. Beim Öffnen/Objektwechsel wird der Scrollstand zurückgesetzt. Der tatsächliche Details-Knopf sowie gezielter Header-/Tabellen-Hover werden mit aktiver ImGui-ID-Diagnose geprüft.

## Ebenen

Die bisherige Physics-Standardauswahl konnte colliderfreie Dekoration nicht finden. Die vorhandene Hierarchiesuche registriert jetzt auch Renderer; Pointerabfragen kombinieren deren Geometrie mit dem bestehenden NonAlloc-Physics-Pfad. InspectorPickOrder dedupliziert pro GameObject, bevorzugt tatsächliche visuelle Geometrie vor Bounds, behält Colliderreferenzen für vorhandene Wire-Funktionen und sortiert deterministisch. Unsichtbare reine Trigger stehen hinter visuellen Flächen. Readable MeshRenderer verwenden nur zugehörige statische Submeshes; einfache Sprites enge Dreiecke und Flip. Der unnormalisierte lokale Strahl bewahrt Weltentfernungen unter nichtuniformer Skalierung. Keine Textur-Alpha-Auswertung; andere Quellen sind als Bounds estimate sichtbar.

Renderer-Bounds werden verteilt aktualisiert. Höchstens 128 Strahlkandidaten lesen frische Engineeigenschaften, maximal 32768 Dreiecke werden geprüft. Eine neue Geometriekopie/Frame, 8 MiB geteilter Cache, 20000 Renderer und 32 veröffentlichte Ziele begrenzen Last. Geometrie wird nicht auf Worker verschoben: Unity-Abfragen bleiben auf dem Spielthread. Keine zusätzlichen Colliders, Materialkopien, GPU-Auslese oder Szenenwrites. Neu erzeugte Objekte benötigen Katalogrefresh; laufende Topologieänderungen gleicher Mesh-Resource ebenfalls. Aktivität/Layer/Bounds werden für Kandidaten neu geprüft. Die Prüfung behauptet keine gemessene FPS-Verbesserung.

Preferred-ID wird vor Pointer-Serial publiziert. Mausrad/Tab-Liste ändern dieselbe Auswahl; die Liste friert den Strahl ein, damit der Weg zur UI das Ziel nicht verändert. Ein Klick bleibt bis zur Bestätigung eingefroren. Commit prüft dieselbe Szene/Session/Pointer/Objekt-ID und fragt exakt dieselbe Ebene frisch ab. Kein Nachbarersatz bei fehlender Ebene. Die letzte gültige Trefferliste kann auch nach Auswahl zwischen Wand und Dekoration wechseln.

## Kompatibilität und Nachweis

Main29 / Inspector3 wegen erweitertem Hover-Vertrag (bis 16 KiB, Ziele/Teilmenge/Scan, Preferred-ID). Bisherige Bytegrößen Hauptcommands/-telemetrie und Map-/Replay-/Clipformate unverändert. 7.11.1-Replaypakete bleiben explizit importierbar. Keine Änderung am Routenalgorithmus, H&S/Outside weiterhin WIP. Unabhängige Clean-/Replay-Sperren und sticky LiveInspector-Aid bleiben bestehen.

Tatsächlicher SDK-Helperbuild und statischer Win64-Trainer, vollständige produktive ImGui-Suite, Engine-Vertragsdoubles, C#→C++-Fixtures/Fuzz, ASan/UBSan und unabhängiges JSON bestanden. Keine Live-Windows-/Unity-GPU-/Spielperformance-Prüfung. Gerenderte Vorschaubilder nutzen produktive ImGui-Zeichenlisten mit synthetischen Objektdaten. Aktuelle Logs/Scope in PRUEFUNG.md.
