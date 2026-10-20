# Prüfung PracticeSuite 6.9.3

Stand: 14. Oktober 2026. Fortführung der 6.9-Linie; kein Raster aus 6.10. Historische Berichte bleiben erhalten. Die Tests arbeiten gegen den produktiven C#/C++-Code, die gelieferte Game-DLL und originale Unity-Referenzen.

| Bereich | Ausgeführte Prüfung |
|---|---|
| Helper | C# 7.2 gegen gelieferte Game-/Unity-/MelonLoader-/Harmony-DLLs, Optimierung, Warnungen als Fehler |
| Trainer | Windows-AMD64-EXE, C++20/-O2, echtes ImGui/Win32/D3D11, Warnungen als Fehler |
| Protokoll | Generator --check; Protokoll 14, Map-Format 7, Telemetrie und Commands bleiben kompatibel |
| Hide-&-Seek-Spielgeometrie | Aus `sharedassets5.assets` extrahiertes NavMesh-Fixture: 824 Vertices, 876 Dreiecke, davon 76 Dreiecke auf Wandoberseiten. 31 Ziele in der großen Bodenkomponente erreichbar; jeder ausgegebene Abschnitt unabhängig auf Bodenflächen geprüft. Wandoberseiten ausgeschlossen. Das Fixture enthält keine Live-Collider/Spielermaße. |
| Wandgeometrie / Cache | ASan/UBSan: grobe Bodendreiecke, Umweg um eine Wand, geänderte Wandkontur und Spielerradius invalidieren den Cache; geneigte/gescherte und dünne Konturen konservativ. Begrenzter dichter Graph mit 60 Hindernissen. |
| Hide-&-Seek-Suche | ASan/UBSan: 22 Kristalle, 2.000 Seeds, 1/6 Worker, identische Top 10, vollständige Zählung, alle ausgegebenen Modellabschnitte wandfrei. Andere Modi behalten ihre bisherigen Modelle. |
| Bestehende Seed-Pipeline | ASan/UBSan: C#/C++-Vektoren, Raw/Base, unabhängiges Ranking, zufällige Permutation über den 32-Bit-Raum, vollständige Bilanz, Spawn-Filter, Abbruch/Neustart und Verifikation |
| Planner / Wire | ASan/UBSan: Trunkierungen, 10.000 manipulierte Pakete, Löcher/Stockwerke, native Korridore anderer Modi, exakte/heuristische Reihenfolge, Cancellation und atomare Caches |
| Freecam | Kompiliert gegen echte Camera-/Controller-APIs. IL-Vertrag: Kamera-Setup kopiert nur Camera-Einstellungen und fügt Camera/HitboxDrawer hinzu; kein Actor-Klon, keine Collider, kein Detach. Harmony-Culling-Patch umschließt exakt eine echte Spielerabfrage; Spiel-/Wand-/Gegner-Aufrufe bleiben erhalten. |
| Aufnahme / Review | C#-Encoder zum C++-Decoder, tatsächliche Pickup-Reihenfolge/Zeitstempel, 50-Hz-Limit und begrenzte Aufzeichnung, persistente Recovery, feste Review-ID trotz neuem Run; Review bleibt vom Live-Kartencache getrennt |
| Animationslogik | ASan/UBSan: Zeit-/Distanzinterpolation, kurze Spuren, Start/Ende, doppelte Punkte, Aufnahme-Lücken, ungültige Daten, Pause/Restart und getrennte Schalter |
| Laufzeitschätzung | `81 / 10 = 8,1 s`, `81 / 8,1 = 10 s`, `90 / 9 = 10 s`; unvollständige/nicht endliche/negative Distanzen liefern keine Schätzung. Vorschlagsgeschwindigkeit begrenzt auf 8,1–10 u/s. |
| Tatsächliche ImGui-UI | 168 Tab/Viewport/DPI/Verbindungsfälle, 216 HUD-Konfigurationen, 128 Modus-/F2-/Telemetrie-Fälle, 288 Minimap-Fälle; zusätzlich vier Routenschalter-Kombinationen und sechs Review-Fenster-/DPI-Szenarien mit Rückkehr zur vorherigen Größe; Kartenhöhe mindestens 350 Pixel bei 1280×720 und 650 Pixel bei 1920×1080 |
| HUD / Run-Prüfung | Pflichtbadge VALID/UNVALID, Vordergrund-Farben, Panelalpha, Koordinaten; sofortige und dauerhafte Aid-Erfassung, verbotene Befehle, Migration alter Journale und fehlender Validierungsnachweise |
| Persistenz | Settings-Worker, Coalescing, Fehler-/Backup-Recovery, zukünftige Versionen geschützt; Review/Run-Journal behalten ihr Format |

## Sichtprüfung

Die produktiven ImGui-Drawlisten wurden im Host gerendert. Große Run-Review-Ansicht bei 1280×720 und 1920×1080 sowie die gekürzten Einstellungs-/Seed-Map-Ansichten geprüft. Diese Bilder stammen aus Testdaten und sind keine Screenshots eines laufenden Windows-Spiels.

## Grenzen

Das tatsächliche Windows-/Unity-Spiel läuft hier nicht. Der native Kameralebenszyklus, echte Trigger-Unterdrückung, Spielereignisse, Spiel-FPS und D3D-Ausgabe wurden gebaut bzw. anhand des gelieferten Codes geprüft, aber nicht live ausgeführt. Mono-Hosttests melden erwartete fehlende Unity-Internal-Calls; ihre Logik-/Datenprüfungen enden erfolgreich. Das belegt keine Ausführung dieser nativen Aufrufe.

Das neue Hide-&-Seek-Routing basiert auf erfassten Bodendreiecken und statischen Collider-Konturen. Unlesbare Meshes/gescherte Geometrie können nur konservative Grenzen liefern; das kann Türen schließen und einen vorgeschlagenen Weg unverfügbar machen. Später bewegte/zerstörte Objekte, andere Stockwerke, Sprünge und reale Pickup-Radien werden nicht vollständig optimiert. Große Dreiecke verwenden eine begrenzte Auswahl sichtbarer Verbindungen. Der Graph garantiert für seine veröffentlichten Abschnitte Wandabstand im erfassten Modell; er behauptet keine globale physikalische Optimalität. Über 15 Kristalle bleibt die Reihenfolge heuristisch. Ohne vollständige Navigation bleiben alle Seeds erhalten, aber ungeprüfte Luftlinien werden ausgeblendet.

Die Laufzeitspanne beruht auf der vom Nutzer genannten optimalen Bewegung von 8,1–10 u/s. Sie beschreibt reine Laufstrecke und schätzt keine zusätzliche Zeit für Abbremsen, Einsammeln, Gegner oder Bewegungstricks.

VALID bleibt eine lokale Suite-Prüfung. Unauffällige externe Speicheränderungen und lokale Änderungen der Historie können damit nicht vollständig ausgeschlossen werden. Keine externe Leaderboard- oder Anti-Cheat-Zertifizierung.

Reproduktion: tests/README.md. Game-DLL-SHA256: ead3f5f861ad7988ad5fdb6f188bb3f77639927dd496b73066860d6c853f0dd8.
