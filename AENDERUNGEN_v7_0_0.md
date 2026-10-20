# PracticeSuite 7.0.0 / mab

Meilenstein für Lesbarkeit, übersichtliche Bedienung und den Übergang vom Run zur Analyse.

- Größere native Schriftgrößen statt dauerhaft verkleinerter HUD-Texte; Atlas-Neuaufbau nach Skalierung zwischen Frames. Intro 640 × 272 bei Standard-DPI mit 32-/20-Pixel-Schrift. Menü 18, HUD 19, Labels 16, Timer 44 Pixel.
- Aufgeräumte HUD-Karten, getrennte Zahlen und Labels, Koordinatenzeile und begrenzte Anzahl sichtbarer Messwerte auf kleinen Viewports. Bestehende Farben/Fonts/Alpha bleiben anpassbar.
- Overview entfernt, Practice in Cheats umbenannt. Tatsächliche Spieler-/Monsterzustände sind im Compact HUD wählbar. Speedrun erhält von den neuen Feldern nur Rotation. Geometrie-Refresh nach Diagnostics verschoben.
- Seed-Suche läuft bis Stop über den 32-Bit-Raum und bewertet immer vollständige Routen. Kein Suchzahl-/Vollrouten-Schalter und kein Selected-result-Block. Spawn-Filter mit pulsierender Kartenmarkierung. Show öffnet und fixiert die passende Vorschau bei fortlaufender Suche. Vorschlagsroute startet ausgeschaltet. Ein Use-displayed-Knopf überträgt Seed, Typ und Karte nach SET Practice.
- Shift+T schaltet sauberen Speedrun mode auf dem aktuellen unterstützten Level. Animierte Benachrichtigung, leiser eigener On-/Off-Ton, keine unbeabsichtigte T-Neuladung mit gedrücktem Shift.
- Finale Zeit, echte vorherige PB/Differenz, Layout-Seed und sieben animierte Validierungsgruppen. Analyse Run? [F8] für 15 Sekunden; neuer Run, Scene-/Sitzungswechsel oder Ablauf entfernt die Meldung. Karten vermeiden vorhandene HUD-Flächen; ohne freien Platz wird eine Karte unterdrückt.
- Replay behält Speedrun mode bei. Aktive Runs blockieren Replay auf Trainer- und Helper-Seite. Ein verspäteter abgewiesener Replay-Befehl beschädigt nicht die Validierung des neuen Runs.
- Automatische dauerhafte Replays erst bei mehr als 10 Sekunden oder wählbarer Kristallzahl; Regel wird beim Start eingefroren. Nicht qualifizierte Aufnahmen bleiben temporär und verschwinden beim Neustart. Statistik und Validierung bleiben unabhängig. Keine Löschaktion im GUI; alte Aufnahmen werden nicht nachträglich entfernt.
- Replay-Ansicht mit größerer Zeit, größerem Inspector, großer Karte, sauberer Seed-/Fortschrittsanzeige; Rename / notes für behaltene Aufnahmen. ABORTED, Monster, Pfade und Flashes bleiben erhalten.
- Diagnose-Scan mit Codes, Befund, Handlungsvorschlägen und kopierbarem Bericht; umfangreiche Rohdaten nur in Technical details. Replay-Lesefehler nennen die betroffene Datei.
- Settings Format 15: alte HUD-Reihenfolgen korrekt erweitern; alte Standardbreiten migrieren; entfernte Optionen nicht reaktivieren. Hauptprotokoll 16 / Commands 136 / Telemetrie 16292 Byte. DLL und EXE gemeinsam ersetzen.

Hide & Seek / Outside bleiben für Karten und Seed-Suche WIP. Routing, Light-Events und kontinuierliches Movement-Lernen verwenden die bestehende Pipeline. Prüfung und Grenzen stehen in PRUEFUNG.md. Version und Autor bleiben in Menü, Intro, HUD und Diagnostik sichtbar.
