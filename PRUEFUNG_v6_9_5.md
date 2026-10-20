# Prüfung 6.9.5

Die Korrekturen setzen die 6.9-Linie fort. Der verworfene Rastergraph aus 6.10 ist nicht enthalten. Windows-DLL und EXE wurden gegen die bereitgestellte Spielversion gebaut; die folgenden Prüfungen liefen als Asset-, Host- und GUI-Tests.

## Leere Hide-&-Seek-Karte

Der bisherige Filter verlangte, dass alle Eckpunkte eines Bodendreiecks nahe an der physischen Controller-Unterkante liegen. Das gebackene NavMesh hat jedoch eine eigene Höhenauflösung und Welttransformation; eine frühe Spielerposition ist kein verlässlicher Bodenanker. Beim Nachstellen mit den beiden gelieferten Assets und deren gespeichertem Weltversatz verwirft dieser Filter sämtliche Dreiecke. Anschließend fehlen gemeinsam genutzte Bodenflächen und Randkanten, also Karte und Routing.

Die neue Auswahl projiziert die echten Spawn-/Kristallpositionen auf darunterliegende, begehbare Flächen und behält deren zusammenhängende Komponenten. Sie ist unabhängig von der momentan erfassten Spielerunterkante. Dächer oberhalb der Ziele werden nicht als Bodenanker gewählt; zusammenhängende Rampen bleiben erhalten. Dieselbe Auswahl liefert Seed Map, Minimap und Replay. Exakte Box-Konturen ergänzen Anzeige und Spielerabstand; ungefähre Mesh-Rechtecke bleiben ausgeschlossen. Die Kartenbegrenzung schneidet den erfassten Boden nicht mehr anhand desselben Höhenfilters ab.

Geprüft:

- Gelieferte Innenkarte: 824 Vertices, 876 Dreiecke, 76 Dachdreiecke; Outside: 4743 Vertices, 4390 Dreiecke, 130 Dachdreiecke. Je 31 verbundene Ziele wurden erreicht; die ursprünglichen Wegstücke wurden unabhängig gegen die Boden-Dreiecke geprüft.
- Beide Assets zusätzlich mit ihrem gespeicherten Weltversatz (-6.3200407, 2.1241815, -5.5100975): Der frühere Filter findet bei physischer Unterkante Y=0 kein Dreieck. Die neue Auswahl enthält Boden und Randkanten und erreicht die Ziele auch bei Unterkanten -5, 0, 1,4 und 6. Fehlende alte Spielerprofile löschen die Bodendarstellung ebenfalls nicht.
- Synthetische Rampen, darüberliegende Dächer, exakte/schmale/gedrehte Hindernisse, falsche Mesh-Rechtecke, Spielerbreite, Geometrieänderungen und Cache-Wechsel. Ein Capture mit Unterkante Y=20 liefert im Worker und echten GUI-Test weiterhin Boden und Routing.
- Klassik-, Bonus-, Kamera-, Timer- und Ghost-Modelle behalten ihre bisherigen Wege. Innenkarte: 2000 Seeds mit 22 Kristallen; Outside: 128 Seeds mit 52 Kristallen aus 70 Punkten. Ein und sechs Worker ergeben jeweils dieselben Top 10 und vollständige Zählung.

## Taschenlampe und Kamera

Die frühere Aufzeichnung hing überwiegend an einer Monsterreaktion; der Classic-Postfix prüfte außerdem die Kamera-Leuchte statt der tatsächlichen Taschenlampe. Ein ausgelöster Blitz ohne Treffer war damit nicht abgedeckt.

Die neuen Beobachter hängen an den bestehenden Light-enable-Anweisungen von MouseLook.Update und der CameraFlash-Coroutine. Der ursprüngliche Setter wird mit demselben Wert ausgeführt. Eingabe-, Batterie-, Strahlen-, RNG- und Coroutine-Abläufe bleiben erhalten. Ein unmittelbar bestätigter Kontakt ergänzt den ursprünglichen Puls; eigenständige schnelle Aktivierungen bleiben getrennt.

Geprüft:

- Im bereitgestellten Spiel-DLL-Code werden genau zwei Taschenlampen-Schreibstellen und zwei Kamera-Schreibstellen erfasst; der Kamera-Stun-Hook erfasst weiterhin beide Zustandswechsel. Die Tests prüfen die tatsächlichen Spiel-Instruktionen und Beobachterziele.
- Ereignislogik: Auslösung ohne Gegner, Anreicherung ohne doppelte Darstellung oder verschobene Zeit/Position, schnelle getrennte Auslösungen, ungültige Zeit-/Typ-/Gegnerdaten und begrenzte Speicherung.
- C#-Format-9-Aufnahmen ohne Monster werden von C# und C++ gelesen. Taschenlampen- und Kamerapulse bleiben erhalten. Bestehende Format-8-Aufnahmen mit bestätigten Kontakten bleiben lesbar; unbekannte und negative Gegner-IDs werden abgewiesen. Prüfsummen und zeitliche Grenzen bleiben geprüft.
- Die reale ImGui-Ausgabe zeigt weiße Pulse auch bei ausgeblendeten oder fehlenden Monstern sowie bei ausgeblendeten Spieler-/Vorschlagsrouten. Alle acht Kombinationen von Monster-, Navigations- und Blitzschalter laufen weiter.

## ABORTED und bestehende Funktionen

Die Replay-Anzeige unterscheidet den Versuchsausgang von der gespeicherten lokalen Integritätsprüfung. Reset, Tod, Unterbrechung und Aufgabe heißen unabhängig vom Prüfergebnis orange ABORTED. Abgeschlossene Runs zeigen weiterhin grün VALID oder rot UNVALID. Der Tooltip eines Abbruchs enthält den Grund und die getrennte lokale Prüfung. Beide Ergebnisse bleiben an der Aufnahme erhalten.

Alle vier Abbruchausgänge wurden mit beiden Integritätsergebnissen geprüft. Die Listen-Zeichenausgabe eines sauberen, aber abgebrochenen Versuchs enthält die orange Markierung und keine grüne VALID-Markierung. Auch die geöffnete Wiedergabe zeigt ABORTED. Vorhandene Dateien erhalten den neuen Anzeigestatus ohne Neuschreiben.

Weiterhin bestanden: Hintergrundarchiv mit mehr als acht Aufnahmen, Speichern/Umbenennen/Kategorien/Unicode-Notizen, Wiederherstellung beschädigter Metadaten, variable Zeitprofile und Run-Zeit-Rekonstruktion, unabhängige Routen, verpflichtendes HUD-VALID/UNVALID, Deckkraft, Koordinaten, IPC-/GUI-Fuzzing, Layoutgenerierung, Einstellungen, Seed-Übernahme sowie deterministische CPU-/RNG-/Ranking-Prüfungen.

## Prüfartefakte und Grenzen

Alle 13 Build-/Prüfgruppen sind erfolgreich abgeschlossen: mod, trainer, managed, native, ui, planner, search, settings, portable, playback, hide-seek-player, replay und layout. Ergebnisse: tests/results-6.9.5. Die Vorschauen unter tests/previews-6.9.5 stammen aus der realen ImGui-Zeichenausgabe mit ausdrücklich synthetischen Aufnahmen. Fixtures und Herkunft: tests/fixtures/README.md.

Hier lief kein Windows-Spielprozess und keine Unity-Szene. Die Assets enthalten echte NavMesh-Geometrie, aber keine Live-Collider-Instanzen. Die Spiel-Instruktionen wurden überprüft; Lichtauslösung und Monsterreaktion wurden nicht im laufenden Spiel ausgeführt. Aktuelle Wandzustände, native Pfadkopie, Cursor/Fokus und tatsächliche Spiel-Frametime bleiben deshalb live zu prüfen. Alte Aufnahmen ohne Flash-Ereignisse können diese nicht nachträglich erhalten. Eine zukünftige Routendauer bleibt eine Schätzung; VALID bezeichnet lokale Prüfungen und keine fälschungssichere externe Anticheat-Bestätigung.
