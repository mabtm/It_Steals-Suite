# Finish-Screen 7.0.2 / mab

Aus der tatsächlichen ImGui-Zeichenausgabe einschließlich ihres Font-Atlas gerendert. Synthetische Run-Daten mit echten UI-Aktions-/Datenpfaden, Host-Schrift DejaVu Sans; keine Windows-/Unity-Liveaufnahme. Windows verwendet die gewählte verfügbare Systemschrift.

- scanning.png: aktive vierte Prüfung, bereits abgeschlossene und noch wartende Zeilen.
- result.png: vollständige Prüfungen, Zeit/PB-Differenz/Seed, Copy seed, Analyse Run und Replays.
- invalid.png: tatsächliches ForbiddenCommand-Warnbit mit rotem Tag und unveränderter ungültiger Gesamtwertung; Animationen ausgeschaltet.
- small-150.png: 480×360 bei 150 % DPI; sichtbare Version/Autor, festes X und erreichbare Aktionen, scrollbarer Mittelteil.
- finish-scan.gif: dieselben tatsächlichen Draw-Ausgaben zu 18 Zeitpunkten, auf das Ergebnisfenster zugeschnitten; Schlussbild bleibt zum Lesen stehen. Kein entworfener Mock-up und keine zusätzliche Grafik im Trainer.

Die funktionalen Prüfungen einschließlich realer Maus-/Tastaturaktivierung sowie Archiv-Laden stehen in ui_tests.cpp / FinishVerificationCases. Abweichungen durch native Windows-Schriftwahl und Treiber-Präsentation sind hier nicht live geprüft.
