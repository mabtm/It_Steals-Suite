# Prüfung – Trainer 7.11.3 / mab

Gezielter Seed-Map-Hotfix auf dem verifizierten vollständigen Stand 7.11.2. Helper und Encoder bleiben byteidentisch. Main29, Inspector3 und alle Datenformate unverändert. Vorherige Nachweise sind historisch unter PRUEFUNG_v7_11_2.md und tests/results-7.11.2 erhalten.

Aktuelle Tests unter tests/results-7.11.3: produktive kumulative Routeninterpolation vom Spawn, Start/Mitte/Ende, doppelte Punkte, leere/ungültige/geschätzte Routen, sofortige Gesamtansicht, abgeschlossene Route über viele Impulsdurchläufe, Pause/Neustart und reduzierte Bewegung. Bestehende kurze Replay-Trails und Zeitinterpolation bleiben geprüft. Vollständiger Import/Export von Replaypaketen einschließlich markierter 7.11.2-Pakete und Originalschutz ist erneut geprüft.

Produktives Headless-ImGui bedient Optimal route, Show full route, Restart drawing, Pause/Resume bei 480/1280 und 100/150 % DPI; prüft sichtbare weiße Impulse über türkisem Weg, Bestand nach mehreren Impulsdurchläufen und reduzierte Bewegung. Die übrige vollständige ImGui-Suite prüft bestehende Replay-/Seed-/Inspector-/HUD-/Clip-/Finish-Ansichten. Windows-x64-C++20/O2/statischer Trainer und Host-ImGui mit Wall/Wextra/Werror gebaut. Neue Vorschau verwendet echte ImGui-Zeichenlisten mit synthetischen Testdaten, keinen Spiel-Screenshot.

Kein Live-Windows-/Unity-/GPU-Test und keine FPS-Messung. Keine Änderung an Routenberechnung oder Hide-and-Seek-Geometrie; H&S/Outside bleiben WIP. Animation ist nur Darstellung; Run-Zeitschätzung bleibt separat. Helperversion7.11.2 neben Trainerversion7.11.3 ist bei diesem rein nativen Hotfix beabsichtigt.
