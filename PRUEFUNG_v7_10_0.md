# Prüfung – PracticeSuite 7.10.0 / mab

| Bereich | Ausgeführter Nachweis |
| --- | --- |
| Builds | Optimierter Helper gegen die tatsächlichen Game-/Unity-/Loader-/Harmony-Referenzen; Windows-x64-Trainer mit C++20/O2/statischem MinGW; produktives ImGui separat. Warnungen als Fehler. Gemeinsames Protokolllayout geprüft. |
| Spielmechanik | Original-IL für Point-Ereignis, Controller.Move, Kamera-Pitch und Culling-Kataloge; native Szenen-Collider und AI-Bake-Profile untersucht. Kamera-Pickup-Trigger und Bewegungskapsel ausdrücklich getrennt. |
| Collider-Mathematik | Box-/Dreiecks-Schnitt mit Höhenband, geneigte Box statt AABB, echte Sphere-/Capsule-Abstände und konservativer Box-Kernbereich, andere Etage, v13-Roundtrip, v12-Fallback und fehlerhaftes Flag. |
| Physische Routen | Acht Modi, direkt begehbarer Boden mit echter zusätzlicher Wand, Umweg/Abstand, kleinere AI-Ecken, sicherer Anker nahe der Wand, Triggerkontakt über dünne Wand ohne Bewegung durch sie, bereits überlappender Spawn, zusammenhängende Kontakte, exakte Gleichheit von Score und gezeichneter Länge, Cachewechsel und Abbruch. Zusätzlich ASan/UBSan. |
| Seed-Suche | Tatsächlicher Coordinator mit 4096 synthetischen 22-Kristall-Auswahlen, physischen Hindernissen und Ausreißer: 1/6 Worker, vollständige Seed-Buchhaltung, wirksame sichere MST-Aussortierung und gleiche Top 10. Unabhängige vollständige Auswertung aller 4096 Auswahlen bestätigt die Rangliste; zeichnende/vollständige Vorschau bestätigt ihre Wege. Host-Zeiten im Log sind kein Game-FPS-Benchmark. |
| Regressionen | Bestehende Planner-, Seed-, Portable-, Hide-and-Seek-/Outside-, Run-/Archiv-, Routenplayback-, Settings-, native Protokoll-, Clip-Pipeline-, Kamera-, Tempo-, Timeline-, Exportprofil- und Eingabe-/Pakettests. Vorhandene Aufnahmequalitäts- und adaptive Posentests. |
| Oberfläche | Echte ImGui-Zeichenlisten: vollständige bestehende Bedienprüfungen, neuer physischer Radar-/Kartenpfad bei 480/1920 und 100/150 % DPI, vorhandene HUD-/Replay-/WIP-Prüfungen. PNGs zeigen ausdrücklich synthetische Testdaten, keine Spielscreenshots. |

Aktuelle Logs und synthetische Fixtures: **tests/results-7.10.0**. Vorschauen: **tests/previews-7.10.0**. Frühere Prüfstände bleiben in ihren Versionsordnern erhalten. Keine Nutzerdaten wurden verändert; Originalspiel und Loader-/Harmony-Referenzen werden nicht mitgeliefert.

## Reproduktion

`build.ps1 -GameDir <Spielordner>` baut mit den eigenen lokalen Referenzen. `python generate-layout.py --check` prüft das gemeinsame Layout. Native Tests: C++20, `-ITrainer -ITrainer/imgui -pthread -Wall -Wextra -Werror`. Neue Quelltests: `physical_route_tests.cpp`, `physical_seed_tests.cpp`, `physical_map_tests.cs`, `physical_map_ui_cases.inl`.

Zuerst den Helper und `managed_tests.cs` gegen die lokalen Game-/Unity-/Harmony-Referenzen bauen. `physical_map_tests.cs` erhält einen frischen Ausgabeordner und die historische `tests/results-7.9.0/managed-data/input-map.isrun`; es schreibt `physical-map-v13.bin`. Dieses Argument nimmt `physical_route_tests.cpp` für die C#→C++-Prüfung entgegen. `physical_seed_tests.cpp` braucht keine externen Argumente. Die vorhandenen Seedtests erhalten `seed-batch-wire.bin` und `seed-order-wire.bin` aus den aktuellen managed Tests. Archiv-/Exporttests immer mit frischen Ausgabeordnern ausführen, damit der Schutz vorhandener Dateien absichtlich aktiv bleiben kann.

## Grenzen der Prüfung

Auf dem Host waren Windows/Unity und die Live-Spielphysik nicht ausführbar. Nicht geprüft wurden tatsächliche Kontaktzeitpunkte bei verschiedenen Kamera-Pitches/Headbob, jede reale Hide-and-Seek-Wand und die konkrete Game-/GPU-Performance. Der Kontaktbereich ist ein konservatives stehendes Level-Kamera-Modell mit endlich vielen Randproben, keine globale Optimalitäts- oder Live-Kollisionsgarantie. Die WIP-Markierung bleibt bestehen. Alte Replays erhalten keine nachträglich erfundenen Colliderdaten.

Spielassembly SHA256: **ead3f5f861ad7988ad5fdb6f188bb3f77639927dd496b73066860d6c853f0dd8**, Unity **2021.2.19f1**. Encoder unverändert; Hauptprotokoll **26**, Map-Kanal **10**, Map/Run-Payload **13**, Input **6**, Frame-Kanal **4**, Clip-Projekt **7**.
