# Aktuelle Prüfungen 7.11.4

Siehe ../PRUEFUNG.md und results-7.11.4. routing_reliability_tests.cpp erhält tests/fixtures/hide-seek-boxes.routefixture und hide-seek-outside-boxes.routefixture. Es prüft bekannte Pfadnetze/Closure, alle Linien, verworfene schlechtere AI-Anker, unabhängige Auswahl trotz Katalogausreißer, 22/40/52 Kristalle mit ETA, Masken für partielle Zeichnung, Snapshotidentität, Abbruch/Retry und Reihenfolgenvergleich gegen den vorherigen NN-/2-opt-Solver. large_pickup_tests.cpp ergänzt zwei synthetische 52-Ziel-Karten mit aktiver Kontaktoptimierung. Fixture-Scope steht in fixtures/README-routing-7114.md.

navigation_capture_tests.cs führt Mod/UnityNavigationCapture.cs mit Engine-Vertragsdoubles und SDK-Vektoren aus (-warnaserror, -nowarn:0436 nur für absichtliche Klassenüberschneidungen). Der Helperbuild nutzt separat ausschließlich originale Referenzen. Produktive ImGui prüft dieselbe Vorschau während Refresh und Lückenmasken; --seed-route-only als drittes Argument nach dem absoluten Drawordner wählt die Routenfälle. Der volle Lauf umfasst weiterhin alle Gruppen. Main29 / Inspector3, keine geänderten Dateiformate.

# Aktuelle Prüfungen: nativer Hotfix 7.11.3

Siehe ../PRUEFUNG.md und results-7.11.3. route_playback_tests.cpp prüft kumulatives Seed-Zeichnen getrennt von bestehenden Replay-Trails; replay_motion_tests.cpp ergänzt Import alter 7.11.2-Pakete. ui_tests.cpp --seed-route-only als drittes Argument nach dem Draw-Ausgabeordner führt echte Produktionsknöpfe und Canvas bei zwei Größen/zwei DPIs aus; die vollständige Suite ohne Schalter wurde ebenfalls ausgeführt. Die neuen Draw-Ausgaben verwenden synthetische Seeds. Helper und Encoder bleiben 7.11.2 byteidentisch. Kein Live-Windows-/Unity-Test.

# Aktuelle Prüfungen 7.11.2

Siehe ../PRUEFUNG.md, results-7.11.2 und previews-7.11.2. Main29 / Inspector3, bisherige Dateiformate unverändert. inspector_tests.cs erzeugt C#-Katalog-/Detail-/Hover-Fixtures einschließlich mehrerer visueller Ebenen. inspector_tests.cpp prüft diese Bytes, Gruppierung nicht zusammenhängender Felder, Mausradwechsel und identitätsgebundenen Commit, Paketgrenzen/Fuzzing sowie die unveränderte gemeinsame Routenprobe.

inspector_visual_tests.cs führt die produktiven InspectorVisualPicking.cs / InspectorPicking.cs gegen kontrollierte Engine-Vertragsdoubles aus. Unity-Vektoren, Bounds und Matrizen stammen aus den SDK-Referenzen. Die übrigen Engineobjekte sind Testdoubles, kein laufendes Unity. Mit -nowarn:0436 werden ausschließlich deren absichtliche Namensüberschneidungen erlaubt; der tatsächliche Helper wird separat gegen ausschließlich originale Engineklassen mit Warnungen als Fehler gebaut. Geprüft sind colliderfreie Dreieckstreffer, enge Sprites/Flip/Resourcewechsel, statischer Submesh-Bereich, Layer/Aktivität, Spielfigur-Ausschluss, Cache-/Kopier-/Kandidatenlimits und ausbleibende Quelländerungen.

inspector_ui_cases.inl bedient produktives ImGui: echter Details-Knopf der Karte, Hover über Object-/Transform-Gruppen bei eingeschalteter ID-Konfliktprüfung, Mausrad, Tab-Liste mit eingefrorenem Strahl und Commit der gewählten colliderfreien Ebene. Vorhandene Hauptseite/Karten/Details/Locks und vollständige übrige UI-Suite bleiben geprüft. Screenshotdaten sind ausdrücklich synthetisch.

# Historischer Prüfstand 7.11.1

Siehe ../PRUEFUNG.md, results-7.11.1 und previews-7.11.1. inspector_tests.cs erzeugt C#-Katalog/Detail/Hover-Fixtures mit expliziten Zeiten. inspector_tests.cpp prüft diese Bytes, Koordinaten/Scene-/Token-Guards, Worker, Reports und gemeinsame Routenprobe. inspector_ui_cases.inl führt produktive Hauptseite, technische Tabs und kompakte Karte aus; Klickposition A bleibt korrekt, während sich der aktuelle Cursor bereits bei B befindet. Auch Abbruch in Compact/F2-hidden-Kontext und unabhängige Sperren werden geprüft. Hover-MMF2 und Main28; Dateiformate unverändert. Originale SDK-/Loader-Referenzen werden nicht mitgeliefert.

# Aktuelle Prüfungen 7.10.0

Siehe ../PRUEFUNG.md und results-7.10.0. Neue produktive Prüfungen: physical_route_tests.cpp, physical_seed_tests.cpp, physical_map_tests.cs und physical_map_ui_cases.inl. Beispiele und frische Ausgabeordner stehen im Prüfbericht. previews-7.10.0 enthält gerenderte echte ImGui-Ausgaben mit synthetischen Karten. Originale Spiel-/Unity-/Loader-Referenzen sind weiterhin lokal erforderlich. Die folgenden Abschnitte sind historische Prüfstände.

# Aktuelle Prüfungen 7.9.0

Siehe ../PRUEFUNG.md, results-7.9.0 und previews-7.9.0. Neue produktive Tests: clip_pipeline_tests.cpp (3-Frame-Fenster), clip_channel_tests.cs (Unity-GPU-Vertragsdoubles), adaptive_capture_tests.cs (Helper-Referenz, Fehlergrenzen/Detailrate/Abschluss), replay_health_tests.cpp und replay_health_ui_cases.inl (Kanalqualität, Lücken, Kopie/Seek). Experience-Fixtures verwenden jetzt input-v6.isinput. Historische Format-5-Fixtures bleiben in results-7.8.1/experience erhalten.

C++-Healthtest: optional zwei Argumente, neuer Experience-Fixtureordner und bisherige Format-5-Datei. Channel-Doubles separat mit den vier Produktions-CS-Dateien bauen; je Prozess bgra-bottom / bgra-top / rgba-bottom / rgba-top / unsupported. Adaptive-Test referenziert ItStealsHelper und UnityEngine.CoreModule; kein Unity-Native-Aufruf. Originalspiel-/Loader-Referenzen werden nicht mitgeliefert.

Die folgenden Hinweise sind historische Prüfungen und weiterhin vorhandene Regressionsgruppen.

# Prüfungen ab 7.8.0

Aktuelle Zusammenfassung: ../PRUEFUNG.md. Neue Quelltests: replay_experience_tests.cs (Helper-Referenz und alte Fixture-Dateien), replay_experience_tests.cpp (neue C#-Fixtures, frischer Ausgabeordner). verify_audio_exports.py <Ausgabeordner> <FFmpeg> prüft mit unabhängigem ffprobe/ffmpeg echte Video-/Audiostreams. UI verwendet benachbarte managed-data- und experience-Ordner; neue v5-Testdaten sind unter results-7.8.0/experience enthalten. Audiofixtures sind synthetische Rampensamples, keine Tonassets aus dem Spiel. Originale Game-/Unity-/Loader-Referenzen werden lokal benötigt und nicht mitgeliefert.

Die folgenden Abschnitte dokumentieren frühere Prüfungen.

# Reproduzierbare Prüfungen

## Neu in 7.7.0

Aktuelle Ergebnisse: `results-7.7.0`; aktuelle Zieltestgrenzen: `../PRUEFUNG.md`. `fullbright_tests.cs` führt die produktive SceneBrightness-Verwaltung gegen Host-Doubles aus; kein Unity-Lichttest. `export_profile_tests.cpp` prüft Presets, 30/60/120, 1x, Größenbudget und schreibt `codec-cases.json`/`budget-cases.json` aus den Produktionsargumenten. C++20 mit `-ITrainer -ITrainer/imgui -pthread`; optional ein frischer Ausgabeordner als erstes Argument.

`verify_export_profiles.py <cases-ordner> --encoder <aus gebündelten Quellen gebauter ffmpeg> --output <frischer-prüfordner>` erzeugt Testbilder, exportiert alle vier externen Codecs bei 30/60/120 FPS und prüft mit unabhängig installiertem ffprobe/ffmpeg Bildzahl, PTS, Laufzeit, Frame-IDs, exakte Lossless-Pixel, BT709 und drei Dateigrößenlimits. Benötigt Python/numpy und FFmpeg mit den getesteten Encodern. Windows-Prozesssteuerung wird dadurch nicht ausgeführt. Der Encoder wird mit den Quellen und aktivierten Komponenten aus Encoder gebaut; siehe dortige NOTICE.md.

## Neu in 7.4.0

`clip_tests.cpp` prüft Zeit-/Frameabbildung, Eingabe-/GIF-Palette, NV12, Hintergrundkodierung, Abbruch, erhaltene Ziele/temporäre Dateien, HUD-Snapping und Annotationen 1/2. Einen frischen Ausgabepfad verwenden, da bestehende Ziele absichtlich geschützt sind:

```bash
g++ -std=c++20 -O1 -g -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -ITrainer tests/clip_tests.cpp -o clip-tests
ASAN_OPTIONS=detect_leaks=0 ./clip-tests clip-output-new
```

`Workshop740Cases` in `ui_tests.cpp` bedient produktives ImGui: HUD ziehen, Undo, Apply, Cancel/Escape, aktiver Run, passive vollständige Eingaben, gemeinsame Clip-Komposition und responsive Exportwerkstatt. Ausgabepfad `draw` braucht einen benachbarten Ordner `managed-data` mit den generierten C#-Fixtures; `results-7.4.0/managed-data` enthält diese Testdaten. GUI-Zeichenlisten lassen sich mit `render_ui.py` prüfen.

`replay_motion_tests.cs/.cpp` prüfen Eingabeformate 1/2/3, tatsächliches Pass-through-IL, native Gerätewerte, beobachtete Abdeckung, Abbruch der Abdeckung bei Pufferverlust, Archiv-/Paket-/Originalschutz und ältere Exporterversionen einschließlich 7.3.5. `replay_visual_tests.cs` führt die produktive Capture-Sicherung für Wandfades und Diagnoseuhr mit Engine-Doubles aus.

Aktuelle Evidenz: `results-7.4.0`, Vorschau: `previews-7.4.0`, genaue Grenzen: `../PRUEFUNG.md`. Windows-MP4, GPU-Bildkanal und echte Eingabegeräte sind auf dem Host nicht ausgeführt. Die folgenden Abschnitte sind historische Prüfhinweise.

## Neu in 7.1.0

DesignModel prüft stabile Seed-Identitäten, begrenzte Animationstabellen, sichere Timeline-Werte und 4096 Ereignisse in höchstens 2048 sichtbaren Spalten. Echte ImGui-Interaktion prüft Seitenleiste per Maus/Tastatur, Clean-/Offline-Sperren der Funktionskarten, exakt anklickbare Pickup-/Flash-Marker, Dragging und Home/End. PB-Modell/GUI/Audio prüfen tatsächliche Verbesserung, finale Gültigkeit, einen einzelnen Ton und fehlende Feier bei ungültigen Runs.

Die Galerie zeigt den echten nativen Renderer mit ausdrücklich synthetischen Testdaten. Aktuelle Logs: results-7.1.0; Vorschau: previews-7.1.0. Vorherige Prüfberichte bleiben historisch erhalten.

## Neu in 7.0.3

FinishVerificationCases ergänzt die produktive technische Detailansicht: echte Mausauswahl einer Prüfzeile, festgehaltene Auswahl, Follow scan, Copy report mit vollständigem Hash und exakter Run-ID; kleine Detail-Popups bei 480×360 und 100/150 % DPI behalten einen erreichbaren Kopierknopf und schließen ohne Verlust des Ergebnisses. 21 Zeitpunkte erfassen die neue 5,04-s-Sequenz. Bestehende Ergebnis-, Analyse-, Replay-, X- und Intro-Regressionen bleiben enthalten.

milestone_tests.cpp prüft alle 42 technischen Felder, begrenzte nicht terminierte Strings, vollständigen Bericht, fehlende Nachweise, korrekte UTC-Umrechnung, monotone 0–42-Eintragsdarstellung, automatische Auswahl des Builds/ersten Fehlers, Pinning und Cache-Aktualisierung. Beide Abbildungen verwenden dieselben unveränderten sieben lokalen Check-Prädikate. Neue Daten entstehen aus dem bereits erfassten Snapshot, nicht aus zusätzlicher Game-Telemetrie.

Aktuelle Logs: results-7.0.3; native gerenderte GUI: previews-7.0.3. Frühere Prüfungen unten sind historische Ergänzungen.

## Neu in 7.0.2

FinishVerificationCases führt die echte GUI mit schrittweisem Scan, 18 erfassten Animationszeitpunkten, weißen Texten und grünen/roten Tags sowie sofortiger Darstellung bei ausgeschalteten Animationen aus. Dauerhafter Analyse-Knopf nach einer Stunde, echtes Maus-Klicken vor Ende der Animation bei 480×360 / 1920×1080 und 100/150 % DPI, Replays-Aktion und Clipboard-Kopie werden geprüft. Das Archiv enthält drei tatsächlich geschriebene/dekodierte Testaufnahmen; die Analyse muss genau die mittlere ID öffnen, auch bei neuerer Helper-Aufnahme, Trennung oder neuer Sitzung. Aktiver Run, ausstehender Befehl, fehlende Aufnahme, falsche ID/Szene/Zeit/Seed dürfen keinen Ersatz-Run öffnen. Die Regressionen zu dauerhaftem Ergebnis und Intro bleiben enthalten. Acht native Schriften im Atlas; die neue 16-px-Ergebnisschrift ist unabhängig von der HUD-Skala.

milestone_tests.cpp prüft die reine Scan-Zeitleiste einschließlich Pause, mehrfacher identischer Renderzeit, reduziertem Bewegungsmodus und einmaligem Abschlusssignal, begrenzte technische Strings, alle unverändert geltenden Warnflags sowie spätes Auffüllen ausschließlich passender Replay-Metadaten. managed_tests.cs prüft aktuelle, ältere, fehlende und zur Szene unpassende Recorder-IDs neben der bisherigen vollständigen Aufzeichnungsregel.

Historische Logs: results-7.0.2; echte gerenderte Draw-Ausgaben: previews-7.0.2. Native Logik-/Adaptertests nutzen ASan/UBSan; echte Windows-/Unity-Präsentation ist auf dem Host nicht ausführbar.

## Neu in 7.0.1 (weiter enthalten)

PersistentFeedbackCases in ui_tests.cpp prüft Ergebnis nach automatischer Endszene und einer Stunde, realen Maus-X-/Tastaturabschluss, vorherige Eingabe-/Menüansicht und kleine scrollbare Fenster bei 100/150 % DPI. Der Intro-Test übt die echte Render-/Commit-Reihenfolge mit fehlerhaften Präsentationen, aktiver Session, fehlender Bereitschaft und Ausblenden aus. milestone_tests.cpp trennt Ergebnis-Lebensdauer und 15-s-Hinweis. experience_model_tests.cpp prüft aufgeschobenen Start, tatsächliche Erstpräsentation, angehaltene Anzeigezeit und ruhiges Intro ohne Animation.

Historische Logs: results-7.0.1. Gesicherte ältere Prüfungen bleiben in den jeweiligen Versionsordnern.

## Neu in 7.0.0 (historisch)

20 Prüfgruppen; die aktuellen Ergebnisse stehen unter results-7.0.0, gerenderte Ansichten unter previews-7.0.0. PRUEFUNG.md beschreibt Nachweise und Grenzen.

milestone_tests.cpp prüft echte Abschlussdaten, alle Warnbits, 15-s-/Scene-/Epoch-/Session-Grenzen, 10.000 kollisionsfreie Benachrichtigungsplatzierungen, Diagnose-Codes samt Befund und Vorschlag, v15-Settings und reale HUD-Zustandslabels:

```bash
g++ -std=c++20 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -ITrainer tests/milestone_tests.cpp -o milestone-tests
ASAN_OPTIONS=detect_leaks=0 ./milestone-tests
```

managed_tests.cs ergänzt die vollständige Recorder-/RunStore-Speicherregel: strikt mehr als 10 Sekunden, tatsächliche Kristallzahl, beim Start eingefrorene Regel, temporäre Daten beim Neustart verwerfen und identische behaltene Run-IDs auf Datenträger. ui_tests.cpp prüft sechs Tabs/144 Basiskombinationen, neue passive Ergebnis-/Hinweiskarten bei 100/150 % DPI, aktive Replay-Sperre, einmalige Modustöne, pulsierten Spawn-Ring und abgeschaltete Optimalroute bei neuen Seeds. seed_search_tests.cpp hält die Vorschau während laufender 32-Bit-Suche fest. Die endlichen Testbatches bleiben absichtlich endlich.

GUI-Zeichendaten können mit dem mitgelieferten Python-Renderer (NumPy/Pillow) als PNG überprüft werden:

```bash
python3 tests/render_ui.py draw/milestone-intro.draw draw/milestone-finish-valid.draw
```

Alle folgenden früheren Abschnitte dokumentieren die historischen Ergänzungen und vorhandenen Regressionen. UI-Namen/Tab-Zahlen früherer Versionen sind nicht die aktuelle Bedienung.

Für alle Tests: Projekt unverändert verwenden und Ausgaben in einen separaten Testordner schreiben, niemals in echte Nutzerdaten.

## Ergänzungen 6.9.2 (historisch)

managed_tests prüft echte Hide-&-Seek-Coroutine-Instruktionen (genau ein Freecam-Ursprung, unveränderte Wand-/Gegnerpositionen), Kamera-/Controller-Felder, Konturen, Boden-/Decken-Ausschluss, sofortige Aid-Markierung, Warnungsgründe, Journalmigration, 50-Hz-/Speichergrenze und Wiederherstellung mit beschädigter neuerer Aufnahme. Protokoll 15: 128 Bytes Commands, 16276 Bytes Telemetrie. Fehlende Unity-Internal-Calls auf dem Mono-Host sind von geprüften Logik-/Datenpfaden getrennt; native Engine/Fokus/D3D müssen im Spiel getestet werden.

review-wire.bin aus managed_tests wird von planner_tests zusätzlich gelesen: Distanz bis zum letzten Pickup, derselbe Goal-Subset, komplette Distanz, Zeitsequenzen, Trunkierung, doppelte Pickup-IDs sowie Review/Live-Cache-Trennung. Map-/Run-Format 7; frühere Formate 1–4 und 6 bleiben getestet. planner_tests dazu mit dem zusätzlichen Pfad zur aktuellen map-wire.bin starten, neben der review-wire.bin liegt.

ui_tests rendert echten Pflichtstatus im Vordergrund bei deaktivierten Metriken/F2/fehlender Verbindung, prüft grün/rot, Panel-Alpha, Koordinaten und eine Analyse mit inzwischen abweichender letzter Run-ID/Szene. 26 aktuelle HUD-Felder; alte Segment-IDs aktivieren Koordinaten nicht. Prüfung der Draw-Listen, keine native Windows-Präsentation.

## Hide & Seek 6.9.3 und animierte Routen

```bash
g++ -std=c++20 -O1 -g -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -ITrainer -ITrainer/imgui tests/hide_seek_player_tests.cpp -o player-tests
ASAN_OPTIONS=detect_leaks=0 ./player-tests /tmp/practicesuite-managed-test/checked-player-navigation-wire.bin /tmp/practicesuite-player-cache tests/fixtures/hide-seek-floor.assetmesh
g++ -std=c++20 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -ITrainer tests/route_playback_tests.cpp -o playback-tests
ASAN_OPTIONS=detect_leaks=0 ./playback-tests
```

Aktuelle managed_tests erzeugen die C#-Wire-Fixtures. Navigation: erfasster Boden, physische Konturen und Spielerprofil; native Korridore aus älteren Hide-&-Seek-Paketen ersetzen das neue Modell nicht. Geprüft werden Wandumwege, Bodenhöhe/Wandoberseiten, Kontur-/Radius-Cachewechsel, unveränderte andere Modi, vollständige Seed-Bilanz und 1/6-Worker-Parität. Das kleine Fixture aus der gelieferten Assetdatei umfasst 824 Vertices und 876 Dreiecke; fixtures/README.md beschreibt Herkunft und Grenzen. Dichter Graph mit 60 Hindernissen sowie dünne, gedrehte und gescherte Konturen sind eigene Szenarien.

Animation: kurze zeit-/distanzbasierte Spuren, interpolierte Marker, Lücken, wiederholte Punkte, Pause/Restart, getrennte Schalter und 8,1–10 u/s. ImGui prüft tatsächliche Farbausgabe für alle vier Routenschalter-Kombinationen, große Kartenansicht und Wiederherstellung der vorherigen Fenstergröße. Keine native Unity-Kameratest-Ausführung auf dem Host.

## Neue CPU-Pipeline 6.9

```bash
g++ -std=c++20 -O1 -g -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -ITrainer tests/portable_search_tests.cpp -o portable-tests
ASAN_OPTIONS=detect_leaks=0 ./portable-tests /tmp/practicesuite-managed-test
g++ -std=c++20 -O1 -g -Wall -Wextra -Werror -pthread -fsanitize=thread -ITrainer tests/portable_search_tests.cpp -o portable-tsan
TSAN_OPTIONS=halt_on_error=1 ./portable-tsan /tmp/practicesuite-managed-test
g++ -std=c++20 -O3 -Wall -Wextra -Werror -pthread -ITrainer tests/cpu_search_benchmark.cpp -o cpu-benchmark
./cpu-benchmark
```

Das optionale Verzeichnis enthält portable-rng-wire.bin und portable-map-wire.bin aus den aktuellen managed_tests. 320 unabhängige C#-/C++-Auswahlvektoren, Map-Format 4/Fuzzing, Raw-/Base-Seeds, 8/18 Kristalle, 1/3/6 Worker und Ranking-Parität zur bestehenden Stream-Pipeline. Zusätzlich Spawn-Filter, vollständige Zählung, Stop bei exakter 14-Kristall-DP, Neustart und Ablehnung nicht verifizierter Modelle. Die C#-Tests prüfen den Verifier gegen unabhängige Referenzen für alle 40 Parameterkombinationen, einen absichtlich falschen RNG und Katalogreihenfolge nach shuffled Tag-Lookup. Die tatsächlichen Unity-Native-APIs sind auf dem Host weiterhin nicht ausführbar: Der produktive Verifier muss im Spiel erfolgreich abschließen, bevor der CPU-Pfad benutzt wird.

ui_tests prüft zusätzlich tatsächliche Practice-Übernahme samt Seed-Typ, ausgewählter Vorschau, Command-Zahl/Optionen und Persistenz. Die normalen Random-/Search-/HUD-/Minimap-/Modusprüfungen bleiben erhalten. PERFORMANCE_v6_9.md enthält die ausgeführten Millionentests; sie verwenden ausschließlich synthetische Distanzmodelle.

## Layout und native Regeln

```bash
python3 generate-layout.py --check
g++ -std=c++20 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -ITrainer -ITrainer/imgui tests/native_tests.cpp -o native-tests
ASAN_OPTIONS=detect_leaks=0 ./native-tests
```

Geprüft: Konfigurationsmigration/Defaults, 25-Feld-Reihenfolge, Mode-/Fokusregeln, Interpolation, tatsächliche Minimap-Achsen, Trail, Clipping und 10.000 defekte Snapshots.

## Seed-Map, Routing und Cache

```bash
g++ -std=c++20 -O1 -g -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -fno-sanitize-recover=all -ITrainer tests/planner_tests.cpp -o planner-tests
ASAN_OPTIONS=detect_leaks=0 ./planner-tests /tmp/practicesuite-planner-test
```

Optional als zweites Argument die von managed_tests erzeugte map-wire.bin übergeben. Geprüft: C++/C#-Interoperabilität, begrenzter Decoder, Trunkierungen und 10.000 manipulierte Pakete, tatsächlicher Worker/Dateisystem, sichere Hinderniskorridore, getrennte Stockwerke, exakte Touren gegen vollständiges Durchprobieren, Heuristik, unerreichbare Ziele und Cancellation. seed_map_fixture.h enthält ausschließlich eigene synthetische Geometrie.

## Tatsächliche ImGui-UI

```bash
g++ -std=c++20 -O1 -Wall -Wextra -Werror -pthread -ITrainer -ITrainer/imgui tests/ui_tests.cpp Trainer/imgui/imgui.cpp Trainer/imgui/imgui_draw.cpp Trainer/imgui/imgui_tables.cpp Trainer/imgui/imgui_widgets.cpp -o ui-tests
./ui-tests /tmp/practicesuite-draw-test
```

Ausgabeordner vor dem Aufruf anlegen. 785 bisherige Ansichtsfälle plus Spawn-/Last-Modusansichten und drei Timer-Gegner-Ansichten: 144 Tabs, 216 HUD-Konfigurationen, 128 Mode-Regeln, 288 Minimap-Ansichten sechs Planner-Zustände und drei Suchzustände; zusätzlich neun Live-Route-Gates und Statusprüfungen. Draw-Dateien enthalten die tatsächlich erzeugte ImGui-Geometrie/Fontdaten. Windows-Eingabe/D3D sind damit nicht getestet.

## Persistenz und Game-DLL

settings_tests.cpp baut mit -pthread und den Trainer-/ImGui-Includes. Es führt den echten Settings-Worker mit Host-Dateiadaptern gegen einen ausschließlich für Tests bestimmten Ordner aus.

managed_tests.cs gegen die gebaute Helper-DLL sowie die eigenen Game-/Unity-/MelonLoader-/Harmony-Referenzen kompilieren. Ein Ausgabeverzeichnis als Argument übergeben. Geprüft: 100-Byte-Packing/UTF-8, Spawn-Selektion/Timer-Sonderfall, Harmony-__instance und tatsächlicher Start-Transpiler, RNG-finally, Koordinatenvergleich, Run-/Menüabbruch/PB/Evidence/Reports/Recovery. Die echte Game-DLL wird auf 22 Seed-Quellen in 15 Methoden sowie Movement und Escape analysiert. map-wire.bin prüft das native Paket unabhängig.

Die Mod referenziert UnityEngine.CoreModule/PhysicsModule/AIModule/InputLegacyModule und die Game-eigenen mscorlib/netstandard/System/System.Core. native Unity-RNG-/PhysX-/Scene-Aufrufe sind auf dem Host nicht ausführbar. Testquellen dürfen deshalb keine Live-Verifikation behaupten.

Gesamter Windows-Build: build.ps1. Aktuell ausgeführte Ergebnisse und Zielsystem-Checks: PRUEFUNG.md.

## Seed-Suche

```bash
g++ -std=c++20 -O1 -g -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -fno-sanitize-recover=all -ITrainer tests/seed_search_tests.cpp -o search-tests
ASAN_OPTIONS=detect_leaks=0 ./search-tests
```

Prüft OS-Zufallsinitialisierung, breite Seed-Abdeckung, eine Million eindeutige Permutationswerte, unabhängige Referenzvektoren, inverse Abbildung und C#/C++-Gleichheit, Rangliste gegen separat berechnete vollständige Routen, Exact/Fast, deduplizierte Treffer, Request-Tickets, Kontext-/Coverage-Stop, 64-Bit-Anzahlen, bestätigte Pakete, begrenzte Warteschlangen, parallele deterministische Ranglisten und späte Publikationen und Abbruch während tatsächlicher 14-Kristall-DP. UI-Tests rufen zudem die echten Random-/Search-Aktionen und Resultat-/Fortschritt-/Stop-Controls auf. Map-Tests prüfen v1-/v2-Cache-Kompatibilität und C#/C++-Tickets. Managed-Tests vergleichen den wiederverwendeten Selektor mit der unveränderten Stock-Liste und führen den echten Kartenkanal mit ACK/Backpressure aus. Optional seed-batch-wire.bin als erstes und seed-order-wire.bin als zweites Suchtest-Argument für C#/C++-Interoperabilität übergeben; seed-large-wire.bin aus demselben Verzeichnis wird dann ebenfalls gelesen. Jedes Layout trägt seinen tatsächlichen Seed; falsche Seed-/Schlüssel-Zuordnungen werden geprüft.

## Große Suchmengen und Performance

```bash
g++ -std=c++20 -O3 -Wall -Wextra -Werror -pthread -ITrainer tests/seed_search_benchmark.cpp -o search-benchmark
./search-benchmark
```

Ein Millionendurchlauf speist fortlaufend synthetische, wiederkehrende Test-Layouts in dieselbe Pipeline ein. Geprüft werden vollständige Verarbeitung, begrenzte Warteschlangen, zehn Treffer und unveränderte Source-Größe. Das ist keine Messung des echten Unity-RNG, der In-Game-Frametime oder einzigartiger realer Karten.

selection_benchmark.cs vergleicht nur das Stock-Auswahlmodell und das wiederverwendete Scratch-Array mit System.Random auf dem Host; keine garantierte Unity-Beschleunigung. Ein ThreadSanitizer-Build der Suchtests prüft dieselben parallelen Worker zusätzlich auf Datenrennen. Dieser wurde hier erfolgreich ausgeführt.

## Suchkriterien und Puffer

seed_search_tests.cpp prüft zwei vollständige Routen-Ziele, ausschließlich Spawn-Filter, Katalog-Ownership, Timer-Sonderfall, ungültige Ziele und TSV-Snapshots. Die MST-Rangliste wird bei 180 unterschiedlichen Layouts gegen unbeschnittene Bewertungen geprüft, in Exact/Fast, beiden Zielen und 1/mehreren Threads. Zusätzliche Prüfungen: ETA-Anlauf/Stall/Tagesformat, Spawn/Layout-Scan ohne Navigation, doppelte IDs, negativer Paketstatus und große C#-Pakete.

ui_tests.cpp prüft eingefrorene Spawn-Kriterien, automatische/manuelle Last, optionales Routing und Timer-Marker für 0/3/128 Gegner. managed_tests.cs vergleicht die wiederverwendete Auswahl und direkte Pakete mit Stock/BinaryWriter, prüft den tatsächlichen MemoryMappedFile-Kanal und ACK/Buffer-Reuse sowie die Lastregelung bei stabilen/langsamen Frames/Stalls. Native Config-Tests decken Config 11, alte Goals und entfernte Filter ab. NavMesh-Tests prüfen deduplizierte identische Dreiecke und die Untergrenze gegen vollständig durchprobierte Touren.

## Vergleichsbenchmarks

```bash
g++ -std=c++20 -O3 -Wall -Wextra -Werror -pthread -DSEARCHLAB_WORKSPACE -ITrainer tests/route_order_benchmark.cpp -o route-benchmark
./route-benchmark
```

3.000 unterschiedliche synthetische Layouts je Heuristik-Größe und 48 exakte 14-Kristall-Touren, feste RNG-Eingaben und ein Worker. Die alte Version kann dieselbe Datei ohne SEARCHLAB_WORKSPACE gegen ihre Trainer-Header bauen. Prüfsummen von Längen und Reihenfolgen müssen übereinstimmen. route_order_reference.h dient nur als vorheriger Solver für die Regressionen und ist nicht in der Runtime eingebunden.

generation_buffer_benchmark.cs mit SEARCHLAB_BUFFER gegen den neuen Helper (bzw. ohne gegen den alten) bauen. Das prüft Stock-kompatible System.Random-Auswahl plus tatsächlichen Mono-Paketkanal mit synthetischem sofortigem ACK über 50.000 Layouts pro Kataloggröße. Keine echte Unity-InitState-, Game-Frametime- oder native Routenmessung. PERFORMANCE_v6_5.md dokumentiert die ausgeführten Vergleichswerte und ihre Grenzen.

## MST-Vergleich 6.6

```bash
g++ -std=c++20 -O3 -Wall -Wextra -Werror -pthread -DADAPTIVE_PRUNING -ITrainer tests/pruning_benchmark.cpp -o pruning-benchmark
./pruning-benchmark 14 0 1
./pruning-benchmark 22 1 1
./pruning-benchmark 40 1 1
```

Argumente: Kristallanzahl, Fast-Flag, gleichmäßige Auswahl (1) oder gezielt günstige Startlayouts (0). 600 synthetische Layouts, feste Eingaben, ein Worker, echte Pipeline inklusive Distanzvorbereitung. Für den Vergleich gegen 6.5 dieselbe Datei und neue Test-Fixtures verwenden, aber zuerst die alten Trainer-Header in den Include-Pfad aufnehmen und ADAPTIVE_PRUNING weglassen. Die zehn Index-/Längen-Prüfsummen müssen übereinstimmen. Drei Wiederholungen je Fall, Median in PERFORMANCE_v6_6.md. Kleine Laufzeiten sind anfällig für Host-Scheduling. Kein Unity-RNG, keine Spiel-Frametime, keine echten Karten.

## Native Unity-Korridore und Laststeuerung 6.7

Managed-Tests prüfen den reinen Graph-Builder: 3D-Deduplizierung, gültige/abgelehnte Segmente, bounded Caches und verzögerte Cancel-Zustände ohne Native-Objekt. native-navigation-wire.bin geht vom tatsächlichen C#-Encoder in den C++-Decoder. Planner-Tests prüfen Format 3, kaputte Indizes/Anker, 3.000 manipulierte neue Pakete, Korridorpfade, getrennte Komponenten und alte Formate 1/2.

Suchtests enthalten wartende Navigation mit automatischem Übergang, einen Layout-Scan während wartender Navigation und einen vollständigen 19-Kristall-/2.000-Layout-Fall ohne Roh-Dreiecke: zehn Kandidaten und Linie in beiden Modi. Alles sind synthetische Fixtures, keine behaupteten echten Timer-Maps. Unity-Native-APIs werden hier nur gegen die realen DLL-Signaturen gebaut; echte NavMesh-Abfragen benötigen den Game-Prozess.

load_controller_benchmark.cs gegen den jeweiligen 6.6-/6.7-Helper bauen und auf getrennten Mono-Pfaden ausführen. Der Test spielt 120 FPS und eine ACK-Bestätigung jeden zweiten Frame nach; seine angenommene Seed-Kostenrate ist keine ausgeführte Unity-Messung. Werte und Grenzen stehen in PERFORMANCE_v6_7.md.


## Navigation-/Top-10-Regression 6.8

seed_search_tests.cpp prüft vollständige, isolierte, fehlende und teilweise verbundene Navigation gegen unabhängig berechnete Ranglisten: Shortest/Longest, Exact/Fast, ein/mehrere Worker, kein Filter und echter Spawn-Filter. Ohne Filter gilt evaluated = scored + pruned; mit Filter kommt filtered hinzu. Kein Seed wird wegen einer NavMesh-Lücke verworfen. Zusätzlich: Screenshot-großer Katalog mit neun Spawns, 34 Punkten, 18 Kristallen und ohne Verbindungen; Top 10 und Pfad in beiden Modi. Vollständige bestehende Dreieckkarten müssen identische Ergebnisse liefern, selbst wenn eine neue KI-Korridor-Erfassung defekt ist.

planner_tests.cpp prüft, dass getrennte native Graphen weiterhin keine bestätigten Graphpfade erzeugen, die Practice-Routenanzeige aber klar gekennzeichnete Schätzungen liefert. managed_tests.cs prüft kanonisch gebundene Endpunkte vollständiger Korridore, leicht versetzte Unity-Endpunkte, Deduplizierung und Ablehnung falscher Endpunkte. ui_tests.cpp rendert Estimated-Ergebnisse und amber gestrichelte Pfadstücke mit realem ImGui. PERFORMANCE_v6_8.md dokumentiert gepaarte Host-Vergleiche; die Live-Suchrate ist damit nicht gemessen.


## Neu in 6.9.7

movement_tests.cs prüft originale Input-Instruktionen, fünf gemessene Physikklassen, ausgeschlossene Bewegung, begrenzte Histogramme, Restart/Checksummen, atomare Speicherung, Backup/Corrupt-Erhalt und zukünftige schreibgeschützte Formate. Zusammen mit managed_tests.cs und -main:MovementManagedTests kompilieren; dessen Datenverzeichnis mit review-wire.bin als Argument übergeben.

movement_native_tests.cpp prüft movement-wire.bin aus dem C#-Test, Format 10/11, Messkontext, Live-/Frozen-Profile, Kurven/Steigungen, Start über kurze Segmente, unbekannte Klassen, 2000 Routen-Fuzzfälle und 20.000 Forecasts. hud_adapter_tests.cpp bindet die echte LayeredHud.h mit tests/hud_stubs als erstem Include-Pfad ein. Die Doubles prüfen D3D-/Win32-Aufrufe und gespeicherte Pixel ohne echte Windows-/GPU-Ausführung.

UI prüft alle belegten Zeichenregionen, Status-Badge/Alpha, WIP-Labels für 5/13 und Timing-Cache. Suchtests prüfen Wiederverwendung/Invalidierung und begrenzte LRU-Verdrängung. Aktuelle Ergebnisse: results-6.9.7 und PRUEFUNG.md.


## Neu in 6.9.8

experience_model_tests.cpp prüft Settings-13-Migration, eigene Farben, endliche Lautstärke, Audio-Gates, feste Animationstabelle, reversible/reduzierte Bewegung, Start/Exit ohne Renderfortschritt und eigene begrenzte PCM-Töne. interface_audio_tests.cpp führt die produktive InterfaceAudio.h gegen tests/audio_stubs aus; diesen Pfad vor Trainer in den Include-Pfad setzen, mit -pthread bauen. Gerätefehler, Mute/Abbruch, Warteschlangen und Cleanup sind damit ohne echten Treiber prüfbar.

ui_tests.cpp enthält zusätzliche Maus-/Keyboard-/Disabled-/Copy-/Mute-/Section-/Lifecycle-Prüfungen und Zeichenausgabe für Overview, Practice, Interface-Einstellungen, Start und Exit. Reale Windows-Fokus-/Audiogeräte bleiben außerhalb des Host-Tests. Ergebnisse unter results-6.9.8.


## Neu in 7.2.0

`replay_motion_tests.cs` zusammen mit `managed_tests.cs` gegen die gebaute Helper-DLL und die eigenen Game-/Unity-/Loader-Referenzen kompilieren; Einstiegspunkt `ReplayMotionManagedTests`. Ein Ausgabeverzeichnis übergeben. Zusätzlich zu bisherigen Referenzen wird UnityEngine.AudioModule benötigt. Der Test erzeugt `input-wire.isinput`, `input-map.isrun` und einen echten Hintergrund-Speichersatz unter `input-store-test/RunReviews`. Er prüft originale Spiel-IL, unveränderte Input-Rückgabewerte, getrennte Frame-/Tick-/Ereigniszeiten, tatsächliche Lichtdaten, beschädigte Daten, Layoutbindung, Gameplay-Sperren und LIFO-Rollback. Native Unity-Aufrufe bleiben dem Live-Spiel vorbehalten.

```bash
g++ -std=c++20 -O1 -g -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -fno-sanitize-recover=all -ITrainer -Itests tests/replay_motion_tests.cpp -o motion-tests
ASAN_OPTIONS=detect_leaks=0 ./motion-tests /tmp/managed-replay-data /tmp/replay-package-test
```

Erstes Argument ist das Ausgabeverzeichnis der verwalteten Tests, zweites ein ausschließlich für Tests bestimmter Archivordner. Der produktive C++-Decoder liest die C#-Dateien. Vollständige Hintergrund-Exporte/-Importe, Originaldateien trotz geänderter Transport-PID, Notizen/Validierungsbericht, Stage3D-Inbox, Konflikterhalt, SHA256-/CRC32-Referenzvektoren, ZIP-Prüfung, beschädigte Daten und ältere Aufnahmen werden geprüft. Das erzeugte `package-for-python.zip` kann zusätzlich unabhängig gelesen werden.

`ui_tests.cpp` enthält `replay_input_ui_cases.inl`: echte Eingabeanzeige, Frame-/Event-Bedienung und 3D-Befehle sowie passive Wiedergabemarkierung. Neue Testbilder verwenden synthetische Run-Daten; sie sind keine Windows-/Unity-3D-Aufnahme. Aktuelle Ergebnisse: `results-7.2.0`, `previews-7.2.0` und `PRUEFUNG.md`.


## 3D-Korrektur 7.2.1

`replay_renderer_tests.cs` zusammen mit den produktiven Dateien `Mod/ReplayRendererScope.cs`, `Mod/ReplayRestoreScope.cs` und `Mod/ReplayPlaybackClock.cs` kompilieren. Keine Unity-DLL referenzieren: der Test stellt gezielte Host-Doubles für Renderer, Property-Blocks und Aktivitätszustände bereit. Mit Mono ausführen. Geprüft werden unveränderte Batch-Bereiche/Materialslot-Zahlen, Material- und Sichtbarkeits-Rücksetzung, inaktive Varianten, simulierte Fehler sowie eine monotone Wiedergabe über 2000 ungleiche Frames/Stalls. Das ist kein GPU-/Unity-Renderer-Test.

`replay_motion_tests.cs` prüft zusätzlich anhand der realen Spiel-IL die pausenfähigen Update-/MoveNext-Ziele und die tatsächlichen Wand-/Prop-/Deckenlicht-Culling-Kataloge. `replay_motion_tests.cpp` importiert auch ein 7.2.0-Paket. Auf die vollständige Archiv-Operation warten: ein Zwischenstand mit `busy=true` zählt nicht als fertiger Import.

Elf aktuelle Prüfgruppen unter `results-7.2.1`; unveränderte frühere Prüfungen unter `results-7.2.0`. Details und Grenzen: `PRUEFUNG.md`. Die Eingabe-/Kameraaufnahme bleibt Format 1, Karten-/Replay-Payload bleibt 12 und Protokoll bleibt 17.


## Neu in 7.5.1

`clip_studio_cases.inl` ergänzt die echte ImGui-Suite um Free/Follow-Wechsel während der Wiedergabe, absichtlich veraltete Bildposen, Recorded→Free, direktes Keypoint-Editing, Bereichsneustart, Projekt-/Settings-Migration, Release-Glow, die gemeinsame Activity-Karte und isoliertes Zeichnen im skalierten Vorschau-Viewport. Optionaler Ausgabeordner enthält Studio-Frames und 60 native HUD-Animationsframes mit ausdrücklich synthetischen Daten. `render_ui.py` rendert diese Zeichnungen samt tatsächlich verwendetem Host-Fontatlas.

`clip_camera_tests.cpp` schreibt die tatsächlichen `LosslessArguments` für 30/60 FPS ins Testverzeichnis. `verify_clip_outputs.py` übernimmt diese Argumente für den nativen Encoder und prüft mit einem unabhängigen Decoder jedes BGRA-Pixel, vollständige Bildfolge, Stream-FPS und PTS. Weitere Modellfälle decken Fit-Auflösungen, 100% sowie Zeitbasis/Quellzeit bei drei Abspielgeschwindigkeiten ab. Aktuelle Ergebnisse und Grenzen: `PRUEFUNG.md`, `results-7.5.1`, `previews-7.5.1`.

## Neu in 7.6.0

`clip_speed_tests.cpp` prüft Cuts, weiche Speed-Übergänge gegen eine unabhängige Simpson-Referenz, inverse absolute Seeks, Cache und feste Ausgabe-FPS. Optionales Argument: frischer Ausgabeordner für eine 0,5x-PNG-Sequenz. `clip_studio_cases.inl` prüft Projektformat 4 und Migration von 1/2/3. `replay_motion_tests.cs` erzeugt Format-4-Grafikdaten mit echten Typen, prüft ihre Grenzen und auditiert die gebaute Grafik-Proxy-IL. Der C++-Decoder-/Pakettest erwartet dafür die frisch erzeugten Format-4-Fixtures; alte Fixtures bleiben über alte Formatleser und UI abgedeckt. Details und Grenzen: PRUEFUNG.md.
