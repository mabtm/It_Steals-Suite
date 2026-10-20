# Prüfung 7.3.3 / mab

Aktuell ausgeführte Gruppen: mod, trainer, native, managed, motion-managed, motion-native, renderer-adapter, replay-visuals, ui, milestone und layout. Vollständige Ergebnisse: tests/results-7.3.3. Frühere Prüfläufe behalten ihre Versionsangabe; PRUEFUNG_v7_3_2.md dokumentiert die direkte Basis.

## Gezielte Regressionen

- Produktiver Sicht-Controller mit tatsächlichem Code und Host-Unity-Doubles: Wandgrafik und Collider in getrennten Wurzeln; Zuordnung über Walls-Layer, Wall-Tags an Vorfahren, ursprüngliches CementWall-/WallSkin-Material und Wandnamen. Ursprüngliche Wandidentität bleibt nach Palettenwechsel erhalten.
- Breite, generisch benannte Hauptwände erhalten die Wandrolle statt des bisherigen horizontalen Bounds-Fallbacks. Explizite Boden-, Decken- und Türrollen bleiben erhalten. Nicht identifizierte Flächen verwenden den bisherigen Geometrie-Fallback.
- Physischer Raycast-Treffer weiterhin erforderlich: leere Trefferliste und Treffer außerhalb der Renderer-Bounds lösen keine Transparenz aus. Einseitige Wandidentität genügt nicht, um unzugehörige überlappende Objekte zu verbinden.
- Bestehende Sichtprüfungen bleiben ausgeführt: Rückstrahl bei Kamera innerhalb einer Wand, maximal 20 Hz, weicher Alpha-Verlauf, gemischte Wand-/Deckenslots, Kameramoduswechsel, idempotente Rücksetzung. Verfolgerwinkel, Zoom, breite konturierte Spieler-Hitbox, Nahgrenzen-Clipping und Shader-Fallbacks behalten ihre produktiven Adaptertests.
- Renderer-/Restore-Adapter prüft Originalmaterialslots, statische Batch-Bereiche, Property-Blocks, Lichtdaten und Fehler-Rücksetzung. Replay-Uhr prüft Pause, Seek, Ratenwechsel und A–B-Schleifen.
- Helper gegen echte Game-/Unity-/MelonLoader-/Harmony-APIs und Windows-x64-Trainer gebaut; C#/C++ behandeln Warnungen als Fehler. Hauptprotokoll 19, generierte Layouts und Structgrößen geprüft.
- C#-zu-C++-Begleitdateien, alte Eingabeformate, strikte Werte-/Zeit-/Identitätsprüfung und native Sanitizer/Fuzz-Fälle. Archivtests prüfen zusätzlich annotierte 7.3.2-Pakete und unveränderte Originalbytes. Python zipfile/json/hashlib prüft den aktuellen Export unabhängig.
- Echte ImGui-Testoberfläche prüft Replay-Buttons, Timeline, Optionen und unterschiedliche Viewports/DPI. Milestone-Tests prüfen aktuelle Versions-/Autoranzeige und bestehende Finish-/Diagnose-Regeln.
- 21 Kern-Dateien einschließlich Karten-/Routenberechnung, Seed-Suche, Bewegungserfassung, Run-Validierung und ursprünglicher ReplayCapture sind bytegleich zur direkten 7.3.2-Basis; Hashliste in preserved.json. Nicht erneut ausgeführte historische Tests werden nicht als aktuelle Prüfläufe ausgegeben.

## Performance und Grenzen

Keine zusätzliche Szenensuche im Frame-Pfad. Zuordnung und Materialarrays entstehen einmal beim Laden; maximal zwei RaycastNonAlloc-Abfragen bei 20 Hz. Keine neuen Meshkopien, Materialinstanzen pro Frame oder Änderungen an Weltcollidern. Die bewährte Wiederherstellung bleibt erhalten.

Kein laufendes Windows-/Unity-Spiel und keine D3D-Rasterisierung auf diesem Host. Die Adapter prüfen eingereichte Geometrie, Rollen, Verbindungen, Materialpfade, Uhr und Rücksetzung, keine realen Pixel oder Ingame-FPS. Headless-Mono kann Hinweise auf fehlende native Unity-Aufrufe ausgeben; diese Pfade gelten nicht als live getestet. Die tatsächliche Wandtransparenz, statische Batch-Darstellung und Eingabefokus müssen im Spiel beurteilt werden.

Dateiformate bleiben unverändert. Replays zeigen beobachtete Bewegung, keine vollständige deterministische Spielsimulation. Die bestehenden Hashes belegen Konsistenz, keine manipulationsfreie Ausführung. Der unabhängige Moderator-Prüfer ist ein geplanter Ausbau und in 7.3.3 nicht enthalten.
