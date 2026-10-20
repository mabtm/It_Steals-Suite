# PracticeSuite 6.9.0

## Seed-Erzeugung und Spielbelastung

- Neuer CPU-Generator mit Runtime-Prüfung gegen den echten Unity-RNG: 4.096 Initialisierungs-/Range-/Zustandsproben und 64 vollständige Stock-Layouts; signed Seed-Grenzen, alle Integer-Weiten 1–1024 und seltene Multiply-High-Grenzwerte. Bei Abweichung unverändert originale Unity-Erzeugung.
- Die geprüfte Suche erzeugt und bewertet Layouts in denselben nativen Workern. Keine laufende RNG-Arbeit auf dem Spielthread, keine pro-Layout-Kommunikation und keine Frame-/ACK-Kopplung.
- Wiederverwendeter Auswahl- und Paketpuffer pro Worker, höchstens 64 Layouts im lokalen Paket. Unveränderte zufällige, wiederholungsfreie Seed-Reihenfolge und vollständige Seed-Zählung.
- Windows-Worker mit niedrigerer Thread-Priorität; voreingestellte Worker-Anzahl bleibt erhalten. Kein automatisches Verändern von Grafik-, Physik-, Gegner- oder Framerate-Einstellungen.
- Gegnersuche nur nach Typen des aktiven Spielmodus. Live-Kristall-/Gegnerdaten im Menü nur bei Bedarf durch Overview/Minimap; Karte verwendet gecachte Ränder statt pro Frame jedes NavMesh-Dreieck zu füllen.

## Übernahme und Replay

- Use best seed / Use einer Zeile aktualisiert die tatsächlichen Practice-Felder einschließlich Map und Seed-Typ und zeigt genau den Ranglistentreffer. Kein neuer, eventuell abweichender Route-Solver-Auftrag bei der Übernahme.
- Use displayed seed aktualisiert Practice; Practice this layout übernimmt und lädt neu. Eingabeseed und tatsächlich dargestellter Seed sind eindeutig beschriftet.
- Bekannte eingegebene Treffer erkennen ihren Seed-Typ; der Practice-Typ wird gespeichert. Command-Synchronisierung hält den tatsächlichen Practice-Seed aktuell.
- Explizite Practice-Replays richten neu geladene Tag-Arrays auf die im Game-Prozess erfasste Katalogreihenfolge aus. Danach bleibt der tatsächliche Koordinatenvergleich aktiv. Unpassende Kataloge dürfen keinen erfolgreichen Replay behaupten.

## Seed Map

- Zwei Untertabs: Preview & replay sowie Seed search.
- Einfacher Suchbereich mit Seed-Anzahl, Spawn-Filter, Start/Stop, Restzeit und Top 10. Einstellungen, Performance und Prediction-Details eingeklappt.
- Manueller Spielthread-Aufwand nur sichtbar, wenn diese Erzeugungsmethode tatsächlich benötigt wird.
- Canvas-Größe richtet sich nach verfügbarem Platz; kompaktere Aktionen in der Rangliste.

Hauptprotokoll 11, Kartenkanal 6, Map-Format 4; Seed-Paket-Format 3 bleibt für den Unity-Fallback erhalten. Alte Map-Formate 1–3 und Einstellungen bleiben lesbar. DLL und EXE gemeinsam ersetzen.

Builds und Host-Regressionen: PRUEFUNG.md. Ausgeführte Benchmarks und Grenzen: PERFORMANCE_v6_9.md. Kein laufender Windows-/Unity-Prozess auf dem Prüfhost; Runtime-Prüfung, Harmony-Replay und tatsächliche FPS sind auf dem Zielsystem zu prüfen.
