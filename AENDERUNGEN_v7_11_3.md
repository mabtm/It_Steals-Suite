# PracticeSuite Trainer 7.11.3 / mab — Seed-Map-Hotfix

- Optimalroute in der Seed-Vorschau wächst vom Spawn bis zum Ziel. Bereits gezeichnete Teilstücke bleiben stehen; nach fünf Sekunden bleibt die vollständige Linie sichtbar.
- Show full route überspringt das Zeichnen sofort. Restart drawing beginnt von vorn. Pause animation / Resume animation steuert Zeichnung und den weißen Richtungsimpuls gemeinsam.
- Weißer Impuls läuft über den bestehenbleibenden türkisen Weg. Der abgeschlossene Weg wird beim Impuls-Neustart nicht gelöscht. Die Animation ist eine Darstellungshilfe, keine neue Run-Zeitschätzung.
- Reduzierte Bewegung zeigt sofort die vollständige Linie ohne Impuls. Neuer Seed blendet die Optimalroute wie bisher zunächst aus.
- Nur Trainer-EXE ersetzen. Helper bleibt die unveränderte 7.11.2-DLL; Encoder, Main29, Inspector3 und alle Dateiformate unverändert. Replay-/Run-Animationen und Routenberechnung bleiben unverändert. Import älterer 7.11.2-Pakete explizit erhalten.

Nachweise und Grenzen: PRUEFUNG.md. Kein Live-Windows-/Unity-Spieltest auf diesem Host.
