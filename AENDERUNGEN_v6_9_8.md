# It Steals PracticeSuite 6.9.8

- Dunkle, einheitliche Oberfläche mit Türkis-Akzenten, eigener Route-Logo-Grafik und kompakter, verschiebbarer Kopfzeile.
- Kurze Reaktionen auf Hover und Drücken, weich schaltende Controls, Tab-Unterstreichung/Überblendung und animierte Detailbereiche. Animationen vollständig abschaltbar; Controls funktionieren weiter mit Maus und Tastatur.
- Kopieren bestätigt sich am selben Button ohne Änderung von Größe oder ID. Kurze Hinweise für gespeicherte Einstellungen/Replays und beendete Seed-Suchen. Erfolgsanzeigen folgen dem tatsächlichen Abschluss des Schreibens.
- Eigene leise Klick-, Tab-, Schalter- und Bestätigungstöne mit Lautstärkeregler und Vorschau. Kein Ton beim Hover. Während aktiver Runs, bei geschlossenem Menü und bei Fokusverlust stumm. Wartender eigener Audio-Thread, begrenzte Warteschlange, veraltete Töne verworfen, Gerätefehler ohne Ersatz-Systemton.
- Route-Logo beim ersten sichtbaren Overlay (0,65 s) und Beenden (0,24 s). Ohne Eingabefokus; in aktiven Runs/ausgeblendeten Fenstern übersprungen. Alt+Tab hält das Beenden nicht an.
- Settings → Interface enthält alle neuen Optionen. Einstellungen Format 13, bisherige Versionen bleiben lesbar; eigene Farben bleiben erhalten.

Die Spiel-/Routing-/Bewegungslogik aus 6.9.7 bleibt erhalten. Der verpflichtende Status und die passiven HUD-Timer bleiben ruhig. Die WIP-Kennzeichnung für Hide & Seek / Outside gilt weiter. Keine Spielzeit-/FPS-/RNG-Änderung durch die neuen Effekte.

Hauptprotokoll 15, 128 Bytes Commands, 16276 Bytes Telemetrie; Kartenkanal 9 / Payload 11. Windows-DLL und -EXE gemeinsam ersetzen.

18 Prüfgruppen bestanden; Details und Live-Grenzen in PRUEFUNG.md. Ton, Fokus und echte Spiel-FPS konnten nicht in einer laufenden Windows-/Unity-Sitzung überprüft werden.
