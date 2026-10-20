# Prüfung PracticeSuite 6.9.2

Stand: 13. Oktober 2026. Fortführung von 6.9/6.9.1; kein Rastergraph aus der verworfenen 6.10. Historische Prüfungen: PRUEFUNG_v6_9.md und PRUEFUNG_v6_9_1.md.

| Bereich | Tatsächlich ausgeführte Prüfung |
|---|---|
| Helper | PASS: 30 C#-Dateien gegen die gelieferten Game-/Unity-/MelonLoader-/Harmony-Referenzen, C# 7.2, Optimierung, Warnungen als Fehler; AnimationModule für den Head-Bob-Animator |
| Trainer | PASS: Windows-AMD64-EXE, MinGW-w64 GCC 13, C++20, -O2, Warnungen als Fehler, echtes ImGui/Win32/D3D11 |
| Protokoll | PASS: Generator --check, Version 14, 128-Byte-Commands, 16016-Byte-Telemetrie, neue Offsets einschließlich 64-Bit-Run-ID; Map-Format 7 und 4-MiB-Kanal |
| Game-DLL | PASS: alle 22 originalen RNG-Quellen in 15 Methoden, reale Start-Tag-Lookups, tatsächliche Bewegungsachsen und Escape-Wechsel |
| Hide-&-Seek-Culling | PASS gegen echte Coroutine-Instruktionen: genau eine Spieler-Positionsabfrage umschlossen, Wand-/Gegnerpositionen und vorhandene Position-Aufrufe erhalten, deaktivierte Freecam liefert originalen Ursprung |
| Konturen / Höhe | PASS: gedrehte/scalierte Box, acht transformierte Ecken, Boden-/Decken-Ausschluss, konservative Markierung geneigter Boxen |
| Spieler-Wandabstand | PASS: 1000 unabhängige dichte Referenzen, runde Ecken, keine Teilkanten aus später fehlerhaften vollständigen Wegen |
| Hide-&-Seek-Suche | PASS unter ASan/UBSan: echter C#-Graph/Encoder, Spielerherkunft vor rohem Gegnernetz, Cachewechsel, sechs weitere Modi, 22 Kristalle/2000 Seeds bei 1/6 Workern; gleiche Top 10, alle Seeds gezählt, ausgegebene Modellwege wandfrei |
| CPU-Suche | PASS unter ASan/UBSan: C#/C++-Vektoren, Raw/Base, 8/18 Kristalle, 1/3/6 Worker, Ranking-Parität, Spawn-Filter, vollständige Bilanz, Stop/Neustart, Verifikation; Generatoren/Seed-Worker aus 6.9 unverändert |
| Planner / Search | PASS unter ASan/UBSan: Decoder-Trunkierungen/10000 manipulierte Pakete, Löcher/Stockwerke, Korridore, exakte/heuristische Reihenfolgen, Schätzungen, Filter, Cancellation; Millionentest der Seed-Permutation |
| Aufzeichnung | PASS: produktiver C#-Encoder zum C++-Decoder, Pickup-Sequenzen mit Zeiten, Fixture-Distanz 18 und Distanz bis letztem Pickup 14, derselbe Goal-Subset-Vergleich, 50-Hz-Limit, maximal 32768 Trace-Punkte, ungültige/backward Samples und Trunkierungen |
| Wiederherstellung | PASS: letzte intakte Aufnahme im Hintergrund eingelesen, beschädigte neueste Datei gemeldet; Review weder als Live-Layout veröffentlicht noch in Live-Karten-Cache gespeichert |
| Run-Prüfung | PASS: jeder Aid-Bit und jeder ungültige Warnungsgrund, sofortige/sticky Hitbox-Aktivierung vor Telemetrie, verbotener Befehl vor Unterdrückung, erlaubter Such-Stopp, dauerhafte Gründe, alte Records ohne nachträgliches VALID |
| Externe Tools | PASS: Namensklassifizierung bekannter Editoren und erlaubter Spiel-/Aufnahmeprogramme; Windows-Prozess-/Debugger-API-Signaturen kompiliert. Echte Windows-Prozess-Erkennung hier nicht ausgeführt. |
| Tatsächliche ImGui-UI | PASS: 168 Tab/Viewport/DPI/Verbindungsfälle, 216 HUD-Konfigurationen, 128 Modus-/F2-/Telemetry-Fälle, 288 Minimap-Fälle; zusätzlich Pflichtstatus trotz deaktivierter Metriken/F2/fehlender Verbindung, Vordergrund-Farben, Alpha-Vertices, Koordinaten und feste Review-ID/Szene |
| UI-Sichtprüfung | Produktive Draw-Listen auf dem Host gerendert, Run-Analyse und 25%-HUD betrachtet. Schätzungen gekennzeichnet; Kartenlegende mit eigenem oberen Abstand. Keine Screenshots aus dem Windows-Spiel. |
| Persistenz | PASS: echter Settings-Worker, Coalescing, Backup-/Fehler-Recovery, zukünftige Versionen geschützt, Commit wiederholbar; Run-Journal/Reports und doppelte End-/Start-Ereignisse |

## Aussagegrenzen

Hier läuft weder das konkrete Unity-Spiel noch eine Windows-D3D-Oberfläche. Maus-Fokusübergänge, tatsächliche Player-Kamera, Live-Hitboxen, Freecam, privates natives NavMesh und echte Alpha-Ausgabe wurden gegen reale Referenzen gebaut, aber nicht live ausgeführt. Mono-Hosttests melden erwartete fehlende Unity-Internal-Calls; die überprüften Logik-/Datenmodelle beenden sich erfolgreich. Diese Meldungen belegen keine Ausführung der Unity-Native-APIs.

Hindernis-/Routingtests sind synthetisch. Unlesbare bzw. konvexe Meshes verwenden konservative Ersatzgrenzen, die Öffnungen schließen können. Aufgezeichnete Geometrie beschreibt den erfassten Zustand; spätere Bewegung/Zerstörung und Pickup-Radien/Movement-Tricks sind nicht vollständig optimiert. Mehr als 15 Kristalle verwenden eine Heuristik. Fehlende bestätigte Wege bleiben gestrichelte Distanzschätzungen und verwerfen ohne Spawn-Filter keine Seeds.

VALID bedeutet abgeschlossene lokale Suite-Prüfung. Prozessnamen/Debugger und ausgewählte Zustands-/Hook-Prüfungen können unauffällige externe Speicheränderungen nicht vollständig erkennen. Ein Editor-Treffer beweist keine Manipulation dieses Spiels. Historie/Reports sind lokal editierbar, keine externe Anti-Cheat- oder Leaderboard-Zertifizierung.

Passives HUD: 30 FPS, Readback wartet nicht auf die GPU. Spiel-FPS und Menülimit bleiben getrennt. Eine neue In-Game-FPS-Messung oder garantierte Steigerung liegt nicht vor. Die CPU-Suche bleibt unverändert und durch Paritätsprüfungen abgesichert.

## API-Grundlage

Gelieferte Assembly-CSharp.dll und Unity-Module; Game-DLL-SHA256: ead3f5f861ad7988ad5fdb6f188bb3f77639927dd496b73066860d6c853f0dd8.

- [Microsoft: UpdateLayeredWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-updatelayeredwindow)
- [Microsoft: BLENDFUNCTION / premultiplied alpha](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-blendfunction)
- [Microsoft: D3D11 Map / DO_NOT_WAIT](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-map)

Reproduzierbare Befehle/Referenzanforderungen: tests/README.md.
