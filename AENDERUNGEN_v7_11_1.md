# PracticeSuite 7.11.1 / mab

- Eigener Inspector-Tab; F7 startet den Picker ohne geöffnetes Hauptmenü.
- Hover mit Objektname, Bildschirmrahmen und vorhandener tatsächlicher primitiver Collider-Drahtform; lautloser Hover, animierter Rahmen, Bestätigungston nach erfolgreicher Auswahl. Reduzierte Bewegung wird berücksichtigt.
- Kleine verschiebbare Objektkarte mit optionalem Pin, Fullbright, erneuter Auswahl, Objekt-ID, Freecam-Fokus und Details auf Anfrage; F3/Esc geben die Spielsteuerung zurück, X beendet die Sitzung.
- Native Mausklickposition statt späterer Renderframe-Position. Spielclient→Kamera-Viewport, gleicher Query für Hover/Bestätigung, erwartete Objekt-/Szenen-/Pointer-Identität und erneute Trefferprüfung. Keine zufällige Nachbarauswahl bei veraltetem Ziel oder gesättigtem Puffer.
- Trigger standardmäßig auswählbar. Renderer-Bounds bleiben ausdrücklich ungefähre und manuell gewählte Hilfe; Katalog-Snapshot und Refresh sind erkennbar.
- Katalog nur auf Anfrage; zusätzliche Transform-/Collider-/Material-/Scriptdaten nur bei geöffneten Details. Grundkarte bleibt klein; Watch-Abfragen bleiben auf ausdrücklich ausgewählte Werte begrenzt.
- End inspection beendet die Sitzung und kehrt zu Cheats zurück, ohne automatische Reaktivierung im nächsten Frame.
- Unabhängige Hover-Bank, Latest-only-Worker, 30-Hz-Abfrage, 5-Hz-Grundwerte; kein blockierender Unity-Mutex und keine zusätzliche vollständige Geometrieaufnahme für den reinen Picker.
- Main 28 / Inspector 2. Helper und Trainer zusammen ersetzen, Spiel neu starten. Keine Änderung am Encoder, Karten-/Replay-/Clipformat oder Routenalgorithmus. Hide and Seek / Outside bleiben WIP.

Host-Build und tatsächliche ImGui-Bedienung werden geprüft; ein Live-Test im Windows-Spiel ist hier nicht möglich. Details und nachvollziehbare Grenzen stehen in PRUEFUNG.md.
