# PracticeSuite 7.1.0 / mab

Alle neun besprochenen Gestaltungspunkte sind enthalten: animierte Seitenleiste, eigenes Vektorsymbolsystem, Funktionskarten, Seed-Such-Dashboard, Replay-Ereignisleiste, verbesserte Minimap, dezente Flächen mit Schatten/Verläufen, gültiger PB-Effekt sowie Lade-/Leeransichten.

Die Seitenleiste bleibt bei kleinen Viewports bedienbar und lässt sich mit Tastatur bedienen. Die sechs Bereiche bleiben bestehen. Cheats verwenden unverändert die bisherigen Befehle und Clean-Run-Sperren. Freecam wird weiterhin im Spiel mit F1 umgeschaltet; die Karte zeigt den tatsächlichen Zustand und die Taste.

Die Seed-Suche zeigt echte aktuelle Zähler und die aktuelle Rate. Eine feste Tabelle merkt höchstens zehn sichtbare Seed-Identitäten und hebt neue Einträge 1,2 Sekunden hervor. Ein Platzwechsel allein startet den Effekt nicht neu. Der zugrunde liegende Generator, Filter, Route-Score und Seed-Transfer bleiben gleich.

Replay stellt die tatsächlichen Pickup- und Flash-Zeitpunkte unter der großen Karte dar. Klicks springen exakt zum Ereignis, Ziehen setzt eine frei gewählte Zeit. Pfeile springen um ein Prozent der Dauer, Shift um eine Sekunde, Home/End an Anfang/Ende. Manuelles Springen pausiert. Höchstens 2048 Bildschirmspalten bündeln bis zu 4096 Lichtbeobachtungen; der Cache wird erst bei geändertem Run, Ereignisbestand, Zeitraum oder Breite neu aufgebaut. Bestehende Aufnahmen ohne Ereignisse bleiben nutzbar. Monster, Flash-Effekte und getrennte Routen-Schalter bleiben erhalten.

Vier Himmelsrichtungen verwenden dieselbe Projektion wie die Minimap-Wände. Unter kleinen 180 logischen Pixeln bleibt nur N sichtbar. Spielermarkierung, Konturen und Boden-/Wandkontrast sind überarbeitet; Quellen und Berechnung der Geometrie bleiben erhalten. Hide & Seek / Outside bleiben ausdrücklich WIP.

Das PB-Abzeichen erscheint erst nach abgeschlossener Prüfsequenz, wenn das eingefrorene Ergebnis gültig, abgeschlossen und vom Helper als PB markiert ist und der Zeitvergleich dazu passt. Ungültige, abgebrochene oder nicht verbesserte Runs erhalten keinen PB-Effekt. Ein eigener kurzer Originalton respektiert private Lautstärke, Fokus-/Menü-/Run-Policy und die bestehenden Audio-Schalter. Reduzierte Bewegung ergibt statische Zustände. Das Ergebnis bleibt bis X/Analyse/Replays geöffnet.

Keine neue Game-Telemetrie, kein weiteres Speicher-/Prozessscannen, keine Änderungen an Routing, Movement-Lernen, Run-Aufzeichnung und den sieben Check-Prädikaten. Die nativen GUI-Vorschauen nutzen gekennzeichnete synthetische Testdaten; sie sind keine Liveaufnahme des Spiels. Windows-EXE und Helper-DLL sind gebaut, Prüfungen und Grenzen stehen in PRUEFUNG.md.

Installation: DLL und EXE gemeinsam ersetzen. Hauptprotokoll 16, Settings 15, Karten-/Replay-Payload 11. Persönliche Einstellungen und gespeicherte Replays werden nicht überschrieben.
