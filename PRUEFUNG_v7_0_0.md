# Prüfung 7.0.0 / mab

20 Gruppen erfolgreich: mod, trainer, managed, native, ui, planner, search, settings, portable, playback, hide-seek-player, replay, layout, movement-managed, movement-native, hud-adapter, experience, audio-adapter, intro-adapter und milestone. Vollständige Logs unter tests/results-7.0.0, gerenderte UI-Ausgaben unter tests/previews-7.0.0.

## Was ausgeführt wurde

| Bereich | Ausgeführter Nachweis |
|---|---|
| Windows-Build | Helper gegen bereitgestellte Game-/Unity-/MelonLoader-/Harmony-Referenzen; 64-Bit-Trainer mit MinGW C++20. Warnungen als Fehler. |
| Gemeinsames Protokoll | Schema-Generator, Byte-Offsets und SDK-Marshalling: Version 16, Commands 136, Telemetrie 16292 Byte. Neue Retention-Felder bei Command-Offset 128/132. |
| Echte GUI | 144 Tab-/Viewport-/DPI-/Verbindungsfälle; 216 HUD-Konfigurationen; 128 Mode-Regeln; 288 Minimap-Ansichten; 24 tatsächliche Settings-Unterseiten. |
| Schriften und Alpha | Sieben native Atlas-Einträge, 18-/19-/16-/44-/32-/20-/56-Pixel-Stufen bei 100 % DPI; wiederholter Austausch zwischen Frames. Palette ohne Atlas-Neubau, eigene/geerbte HUD-Farben bei 0,25/1 Alpha; Pflichtstatus unabhängig. |
| Neue Bedienung | Tatsächlicher Mode-Umschalter, einzelne On-/Off-Signale und Verfall alter Signale; Replay-Block während aktiver Runs. Feste Abschlussdaten mit gültigem/ungültigem Ergebnis, passive Karten ohne Eingabe und ohne HUD-Überlappung bei 100/150 % DPI. Grüner Spawn-Ring; Optimalroute nach zwei neuen Seeds ausgeschaltet. |
| Zeitgesteuerte Run-Karten | 15-s-Grenze, exakte Replay-ID, Scene-/Epoch-/Sitzungswechsel, neuer Run, Trennung und keine erneute alte Abschlussmeldung beim Verbinden. Vorherige PB wird vor einer möglichen Aktualisierung festgehalten. |
| Platzierung | 10.000 zufällige Layoutfälle: ausgegebene Karten innerhalb des Viewports und mit Abstand zu belegten Flächen; Auslassen bei fehlendem Platz. |
| Integrität | Sämtliche 32 Warnbits: bekannte und unbekannte Fehler scheitern; reine Eventlog-Kürzung bleibt zulässig. Unbekannte Prüfstände werden nicht grün. Tatsächliche Hilfeaktivierung bleibt gesetzt; verbotene Cheat-Befehle werden vor Unterdrückung erfasst. Ein abgewiesenes verspätetes F8 setzt keinen falschen Fehler. |
| Replay-Regel | Reale Recorder-/RunStore-Pipeline: genau 10 s temporär, 10,001 s dauerhaft; 2/3 Kristalle unter/über Schwelle; Regeländerung während des Versuchs ohne Einfluss auf diesen Versuch; temporäre Daten beim Neustart weg; genau zwei richtige Replay-Dateien dauerhaft und dekodierbar. |
| Diagnose | Gesunde Verbindung sowie fehlender Helper, alte Daten, Cursor, Software-Renderer, hohe gemessene Arbeit, FPS/VSync, fehlende Hooks/Geometrie, WIP, Schreib-/Replay-Fehler, Integrität und Audio. Befund enthält Codes, beobachtete Werte und passende Vorschläge. |
| Seed-Suche | Eine Million eindeutige Zufallsseeds, inverse Permutation und C#-Vergleich; unabhängige Top-10-Referenz, beide Ziele, Exact/Fast, mehrere Worker, Navigation-/Kontextfehler und begrenzter Cache. Gepinnter Seed/Spawn/Kristalle bleiben während laufender 32-Bit-Suche und beim Stop identisch. |
| Bestehende Funktionen | Routing und reale Hide-&-Seek-Boden-Fixtures, Outside, abgegrenzte/fehlende Navigation, Zeitmodelle/Movement-Lernen, animierte Replay-Routen, Monster-/Flash-Toggles, orange ABORTED, SDK-Freecam und Collider. |
| Speicher-/Adapterpfade | Reale Settings-/Replay-Hintergrundarbeiter, Backups, unbekannte neuere Formate, beschädigte Dateien und fehlgeschlagene Commits. Produktive LayeredHud-/IntroBackdrop-/Audio-Klassen gegen typisierte Windows-/GDI-/D3D-/WinMM-Doubles, mit tatsächlichem Audio-Hintergrundthread. |

Die neue Settings-Version 15 erhält eigene Werte, migriert alte Standardbreiten, übernimmt alte HUD-Reihenfolgen korrekt und aktiviert entfernte Routenschalter nicht erneut. Der Zeit-/Kristallmodus wird im Rundlauf und mit fehlerhaften Zahlen geprüft. Vorhandene Aufnahmen werden durch die neue Aufzeichnungsregel nicht nachträglich gefiltert oder gelöscht.

13 zentrale Routing-/Movement-/Light-/Freecam-/Collider-Dateien wurden zusätzlich bytegenau mit dem gesicherten 6.9.9-Paket verglichen und sind unverändert. Vergleich unter tests/results-7.0.0/core-preservation.json. Dies ergänzt die ausgeführten Regressionen; es ersetzt keinen Spieltest.

## Visuelle Prüfung

Die Vorschauen stammen aus der tatsächlichen ImGui-Zeichenausgabe einschließlich ihres Font-Atlas. Menü, Intro, Compact-HUD, gültiger/ungültiger Abschluss, Modusmeldung, Spawn-Preview, ausgeschaltete Seed-Optimalroute und große Replay-Karte wurden gerendert und gesichtet. Die Daten sind ausdrücklich synthetische Testdaten; die Host-Schrift ist DejaVu Sans. Windows lädt verfügbare Systemschriften und meldet die tatsächlich verwendeten Familien in Diagnostics. Vorschauen zeigen keine Windows-/Unity-Liveaufnahme.

Der Intro-Blur bleibt ein einmaliger Ausschnitt mit höchstens 512 × 256 Pixeln; keine dauernde Nachbearbeitung. Die belegten Zeichenbereiche bleiben von den vorhandenen begrenzten HUD-Regionen abgedeckt. Farben lösen keinen zusätzlichen Font-Upload aus; neue Run-Karten werden nur während ihrer kurzen Laufzeit gezeichnet. Der Diagnose-Scan nutzt bestehende Beobachtungen, statt ein Lastprogramm im Spiel auszuführen.

## Grenzen

Kein laufendes Windows-/Unity-Spiel auf diesem Host. Die EXE/DLL sind gebaut, aber reale Grafik-/Fokus-/Audio-Treiber, Tastaturzustellung im Spiel und echte Spiel-FPS sind hier nicht live geprüft. Die SDK-Prüfungen laufen unter Mono mit Test-Doubles; Unity-Native-Stub-Hinweise sind im Log sichtbar. Native Logik-/Adapterprüfungen verwenden ASan/UBSan, LeakSanitizer ist auf diesem Host deaktiviert.

Hide & Seek und Outside bleiben für Wandkonturen/Minimap/Seed-Suche WIP. Keine Zusage einer schnellsten realen Route oder exakt vorhersehbarer Gegnerwartezeit. VALID steht für lokale Beobachtungen und kann versteckte Speicheränderungen nicht ausschließen. Das GUI bietet keine Replay-Löschfunktion; Dateien außerhalb der Suite sind dadurch weder lösch- noch manipulationsgeschützt.

Installation: Helper-DLL und Trainer-EXE zusammen austauschen. Einstellungen und alte Aufnahmen bleiben erhalten. Keine originalen Spiel-/Loader-DLLs enthalten.
