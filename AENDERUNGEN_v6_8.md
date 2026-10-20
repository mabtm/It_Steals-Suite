# PracticeSuite 6.8.0 – Korrektur der Seed-Suche

- Entfernt den impliziten NavMesh-Erreichbarkeitsfilter, der in 6.7 sämtliche Seeds ausschließen konnte. Eine Modelllücke ist kein Nachweis, dass ein Seed unspielbar ist.
- Bewährte vollständige Dreieckkarten haben wieder Vorrang; Besuchsreihenfolge und Weglängen bleiben dort unverändert.
- Top 10, Show, Best-Seed, Route auf der Seed-Karte und passende Practice-Minimap bleiben auch bei unvollständigen KI-Karten verfügbar. Fehlende Paardistanzen werden einmal als Luftlinie geschätzt; Estimated, amber gestrichelte Pfadstücke und TSV-Kennzeichnung verhindern eine Verwechslung mit bestätigten Wegen.
- Unitys vollständige CalculatePath-Korridore werden direkt übernommen, ihre Endpunkte kanonisch gebunden. Zusätzliche Raycasts aus 6.7 werden nicht mehr verwendet, um vollständige native Pfade nachträglich aufzutrennen.
- Jeder korrekt gelieferte Seed wird ausgewertet. Ein gesetzter Spawn-Filter ist weiterhin wirksam. Eine mathematische Top-10-Untergrenze spart Solver-Arbeit bei eindeutig nicht konkurrenzfähigen Kandidaten; auch diese zählen als geprüft.
- Automatische Laststeuerung, echtes Unity-RNG, zufällige Suche ohne Wiederholungen, Workerpool, bounded Caches, ETA und persistente Optionen bleiben erhalten. Timer-Gegner bleiben durchgehend rot.
- Geometrie-Caches berücksichtigen jetzt auch den Katalog der Spawn-/Kristallkoordinaten.
- Main-Protokoll 10, Config 11 und Kartenformate 1/2/3 bleiben kompatibel.

## Installation

Spiel/Trainer schließen, bin/ItStealsTrainer.exe und bin/ItStealsHelper.dll gemeinsam ersetzen. Nur eine Helper-DLL im Mods-Verzeichnis belassen. Map neu laden oder bei ruhender Suche Rebuild map geometry [F5] verwenden. Bestehende Config/Run-Daten nicht löschen.

## Prüfung und Grenzen

Windows-EXE und Helper-DLL gebaut. ASan/UBSan-, ThreadSanitizer-, managed-, Config-, Karten- und echte ImGui-Regressionen bestanden. Gemeldeten Katalogumfang und vollständig getrennte Navigation getestet. PERFORMANCE_v6_8.md enthält die aktuellen Host-Vergleiche. Tatsächlicher Unity-Game-Prozess steht hier nicht zur Verfügung; Schätzungen sind keine Garantie für kürzeste reale Laufwege.
