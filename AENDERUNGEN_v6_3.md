# Änderungen 6.3.0

- Seed-Anzahl frei eingeben: 1 bis zum vollständigen 32-Bit-Seed-Raum (4.294.967.296), einschließlich Millionen. 64-Bit-Zähler, eindeutige Folge über die signed Grenze; keine 2000-Seed-Grenze.
- Layouts paketweise innerhalb eines einstellbaren Game-Frame-Budgets erzeugen, statt einem Layout pro Frame/60 pro Sekunde. Originaler Unity-RNG bleibt auf dem Spielthread, mit finally-Restore pro Paket und wiederverwendetem Scratch-Array.
- Geometrie-Distanzen einmal vorbereiten; Routen auf einstellbaren Threads parallel bewerten. Erzeugung und Bewertung laufen gleichzeitig. Pakete/Queues/Caches begrenzt; kein RAM-Wachstum proportional zur Seed-Anzahl, kein Datei-I/O pro Paket.
- Empfänger bestätigt Pakete erst nach Übernahme; voller Puffer pausiert Generation. Tickets, Prozess/Sitzung, Katalog, Seed, Rezept, 64-Bit-Zähler und fortlaufende Offsets prüfen; kein Verlust durch Überschreiben.
- Generierungs-/Routenfortschritt, Suchrate, Threads und verstrichene Zeit anzeigen. Stop und Kontextwechsel behalten veröffentlichte Treffer; späte Worker dürfen den Abbruch nicht überschreiben.
- Spawn-Statistik-Suche, Offline-Sample-Suche, Wahrscheinlichkeiten, Häufigkeiten, Heatmap und Statistik-Berechnungen entfernt. Ältere Karten-Dateien bleiben lesbar.
- Cache-Route nur wiederverwenden, wenn sie bereits vollständig berechnet wurde. Eine beim Start lediglich gelesene Offline-Karte bekommt beim Öffnen weiterhin ihre echte Hintergrundanalyse.
- Config 9 übernimmt vorhandene Einstellungen und große Suchanzahlen; alte Statistik-Keys entfallen. Hauptprotokoll 7 mit 92-Byte-Commands; Kartenkanal 3, alter Karten-Dateiformat-Decoder erhalten.
- Random Seed, vollständige Karte, Practice-Replay, Minimap-Route, zwei HUDs, Input/Fokus, Escape-Abbruch, Run-Daten und Verifikation erhalten.

DLL und EXE gemeinsam ersetzen. Tests/Builds und ausdrücklich nicht ausgeführte Live-Prüfungen: PRUEFUNG.md.
