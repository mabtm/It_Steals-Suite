# Prüfung PracticeSuite 6.9.0

Stand: 13. Oktober 2026. Ausgangspunkt: das tatsächlich materialisierte Release ItStealsSuite_v6_8.zip. Das Ausgangsarchiv und seine bestehende Version bleiben unverändert.

## Ausgeführte Prüfungen

| Bereich | Ergebnis |
|---|---|
| Vollständiger Helper | PASS: alle 23 C#-Dateien mit Mono 6.8, C# 7.2, Optimierung, Warnungen als Fehler; tatsächliche Game-/Unity-/MelonLoader-/Harmony-Referenzen |
| Vollständiger Windows-Trainer | PASS: MinGW-w64 GCC 13, C++20, Optimierung, Warnungen als Fehler, ImGui und tatsächliche Win32/DX11-Backends; statisch gelinkte AMD64-EXE |
| Protokoll | PASS: Hauptprotokoll 11, 100-Byte-Commands, 9014-Byte-Payload, Kartenkanal 6, Map-Format 4, Seed-Fallback-Format 3; Generator check erfolgreich |
| RNG-Verifier | PASS: unabhängige Referenzen für 40 Parameterkombinationen, 4160 Proben je Kombination, negative/signed Grenz-Seeds, alle Integer-Weiten 1–1024, seltene Multiply-High-Grenzfälle; abweichendes Modell fällt zurück |
| C#/C++-Erzeugung | PASS: 320 binäre Stock-Auswahlvektoren für alle Parametervarianten; identische Spawn-/Kristallfolge, identische Scene-/Site-/Base-Ableitung; tatsächlicher C#-Map-Format-4-Encoder zum nativen Decoder |
| CPU-Suche | PASS: identische Rangliste und Längen zur bisherigen Stream-Pipeline; Raw/Base, 8/18 Kristalle, 1/3/6 Worker; zehn Ergebnisse, Pfadlinie, Spawn-Filter, vollständige Seed-Bilanz |
| Abbruch / Neustart | PASS: Stop während tatsächlicher exakter 14-Kristall-DP mit voller 32-Bit-Anzahl, keine späte Veröffentlichung, anschließender Layout-only-Neustart; unbestätigtes CPU-Modell abgelehnt |
| ASan/UBSan / TSan | PASS: neue parallele Suchtests einschließlich Stream-Referenz, Cross-Language-Vektoren, 5000 manipulierte Format-4-Pakete, Filter, Abbruch und Neustart; TSan mit halt_on_error=1 |
| Bisherige Seed-/Routing-Regeln | PASS: voller Random-/Inverse-/Millionen-Test, C#-Seed-Pakete, 64-Bit-Zahlen, ACK/Queue, Auswahl-Kontext, Exact/Fast-Ranking, Filter und Cancellation; NavMesh-Lücken liefern weiterhin geschätzte Kandidaten statt Seed-Ausschluss |
| Map-/NavMesh-Tests | PASS: bestehende Decoder-/10.000-Fuzz-/Korridor-/Hindernis-/Stockwerks-/Tour-/Cache-Prüfungen unter ASan/UBSan |
| Run-/RNG-Patches | PASS: tatsächliche Game-DLL mit 22 RNG-Quellen in 15 Methoden, Start-Transpiler, ursprüngliche Spawn-Auswahl, Timer-Override und finally-Zustandswiederherstellung; Menüabbruch/Run-/Evidence-/Persistenzprüfungen |
| Replay-Katalog | PASS: geänderte Tag-Reihenfolge wird anhand von Koordinaten in eine vollständige eindeutige Permutation gebracht; fehlende/duplizierte Ersatzpunkte werden abgelehnt |
| Seed-Übernahme / Persistenz | PASS: tatsächliche UI-Funktionen übertragen Map, Zahl, Seed-Typ und Modus in Practice und Commands; passende Vorschau bleibt gleich; kein Load-Puls bei Use best; Raw/Base-Wechsel, bekannte Seed-Erkennung und Settings-Roundtrip |
| UI / Regressionen | PASS: tatsächliche ImGui-UI, 144 Tab-/Viewport-/DPI-/Verbindungsfälle, 216 HUD-Konfigurationen, 128 Modusregeln, 288 Minimap-Ansichten, 6 Planner- und 3 Suchzustände; neue Untertabs und gerenderte Draw-Daten visuell geprüft |
| Native / Settings | PASS: 10.000 Snapshot-Fuzzfälle, Config-Migration, Fokus-/Achsen-/Trail-/Clippingregeln; tatsächlicher Settings-Worker, Coalescing, Backup, Future-Version-Guard, fehlgeschlagener Commit und Retry |
| Große Mengen | PASS: eine Million Seeds je Routing-Benchmarklauf, neun Läufe, identische Top 10; zusätzlich zehn Millionen Layout-only-Seeds vollständig verarbeitet, Speicher bleibt begrenzt |

Die C#-Tests laufen mit Host-Adaptern für nicht ausführbare native APIs und tatsächlichen gelieferten Game-Referenzen. Das ursprüngliche Auswahlmodell und der neue Verifier werden getrennt mit unabhängigen Referenzen geprüft. Die Produktions-Freigabe verlangt zusätzlich den Abgleich mit dem tatsächlichen Unity-Prozess und wird nicht anhand der Host-Testwerte aktiviert.

## Performance und Spielbelastung

PERFORMANCE_v6_9.md enthält die ausgeführten Messwerte, Median, Testbedingungen und Grenzen. Die günstige synthetische Karte spart rund 99,8 % der Solver-Aufrufe. Daher liefern diese Zahlen keine zugesagte reale Seed-Rate. CPU-Generation entfällt nach erfolgreicher Runtime-Prüfung aus dem Game-Thread; Spiel-CPU und Worker teilen sich weiterhin Prozessor und Speicher. Windows-Worker sind niedriger priorisiert. Ein garantierter FPS-Gewinn oder ein GPU-Gewinn wird nicht behauptet.

## Nicht live geprüft

Kein laufender Windows-/Unity-Prozess und keine Ziel-GPU stehen hier zur Verfügung. Die eigentlichen Unity-RNG-/NavMesh-/PhysX-Aufrufe, private State-Reflektion im laufenden Player, Harmony-JIT-Hooks, Szene-/Objekt-Reihenfolge beim tatsächlichen Reload, Mauszustellung, Input-Lock, Windows-Threadpriorität und reale Spiel-Frametimes konnten hier nicht ausgeführt werden. Unpassender Unity-RNG führt zur originalen Erzeugung, kein Schätzwert darf als bestätigtes Seed-Layout erscheinen. Geschätzte Routing-Distanzen bleiben getrennt davon gekennzeichnet.

## Zielsystem-Checks

1. DLL und EXE gemeinsam ersetzen; alte DLL aus Mods entfernen, Configs und Run-Daten behalten. Map neu laden und Seed Map öffnen.
2. Navigationserfassung und RNG-Prüfung abwarten. Bei Generator: verified CPU workers 1/2/6 Worker mit derselben Map, Anzahl und Genauigkeit vergleichen; bei original Unity fallback die Begründung unter Performance details lesen.
3. Nach abgeschlossener Suche Use best seed drücken. Speedrun-/Practice-Felder auf dieselbe Map, Zahl und Captured-spawn-Typ prüfen. Practice this layout (SET): tatsächlich gleiche Spawn-/Kristallpositionen und kein Prediction-Mismatch.
4. Beide Seed-Typen, negative/Null-Seeds, wiederholte Reloads, verschiedene Maps und bekannte manuell eingefügte Suchtreffer prüfen. Ein nackter Seed allein enthält keinen Katalog oder dessen Reihenfolge aus einem anderen Game-Prozess.
5. Ohne Spawn-Filter zählt jeder angeforderte Seed; bei fehlender Navigation Top 10 und ausdrücklich geschätzte Linien. Kein Unreachable-Seed-Ausschluss.
6. Timer-Gegner bleiben rot, Camera Bright vorhanden; Minimap-/HUD-/Clean-Regeln, Stop, neue Suche, Scene-/Session-Wechsel und F3/F4/F5 testen. Normale Spiel-FPS mit/ohne laufende Suche getrennt messen.

