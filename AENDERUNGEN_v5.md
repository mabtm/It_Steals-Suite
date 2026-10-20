# Version 5.0 – HUD, Speedrun und Eingabe

Fertige Dateien: `bin/ItStealsTrainer.exe` und `bin/ItStealsHelper.dll`. Beide gemeinsam ersetzen; diese Version verwendet Protokoll v4. Vollständiger Quellcode und ImGui liegen bei. Installationshinweise in `README.md`, ausgeführte und offene Prüfungen in `PRUEFUNG.md`.

## Maus und Fokus

- Sichtbarer ImGui-Cursor bei aktivem Menü, auch wenn Unity den Hardware-Cursor ausblendet.
- Scope für Unity-Cursor-Lock/-Sichtbarkeit; die tatsächlichen Cursor-Anweisungen der Spielmethoden werden erfasst und auf Rückkehr wiederhergestellt.
- F3 öffnet aus dem HUD das bedienbare Menü. Das Menü startet geschlossen; ein gespeichertes HUD nimmt keinen Fokus.
- Alt+Tab blendet die Anzeigen aus. Ein noch geöffnetes Menü nimmt bei Rückkehr wieder Fokus; bei verweigerter Aktivierung schließt es sich. HUDs bleiben ohne Maus-/Keyboard-Eingabe.
- Held-key-Sperre für Trainer-Hotkeys nach Menüwechsel; kein globales `ShowCursor`-Hoch-/Runterzählen.
- Fensterklasse verwendet keinen für Layered-Fenster ungeeigneten eigenen Klassen-DC.

## Darstellung und Bedienung

- Bis zu 60 Hz Telemetrie bei sichtbaren Anzeigen, statt ungleichmäßig getakteter 30 Hz.
- Gleichmäßiger Render-Takt mit Waitable-Timer, ohne Spin-Wait; 30/60/120 FPS bleiben wählbar.
- Interpolierte Positionen und kurze Winkelwege; Teleports, Szene-/Session-Wechsel und lange Lücken springen unmittelbar auf den neuen Zustand. Daten werden nicht über längere Aussetzer extrapoliert.
- Einfache Timer-Zwischenwerte, begrenzt auf 150 ms. Finish-/History-Zeiten bleiben ausschließlich Mod-Messwerte.
- Minimap-Maßstab außerhalb innerer Schleifen; Clipping nur für schneidende Wandrechtecke; bestehende Caches bleiben bestehen.
- Alpha-Renderer über DirectComposition, mit bestehendem DX11/Color-Key-/WARP-Fallback. Hintergrund-Rendering pausiert weiterhin.
- Compact HUD oben rechts, Speedrun-Preset oben links; klare Kennzahlen, größerer Timer, Seed-Badges und gemeinsame ruhige Farbwelt.

## Anzeigeprofile

Settings → **Overlay configuration** bietet zwei getrennte Tabs. Auswahl und Reihenfolge der Werte, vier Presets, Skalierung, Breite, Position, Opazität, Innenabstand und Zeilenabstand werden automatisch im bestehenden `trainer.ini` gespeichert.

Die 25 auswählbaren Werte sind Game FPS, Timer, Seed, Seed-Typ, Attempts, Resets, Completed, Completion Rate, Run-Nummer, Session-Zeit, Local PB, Average Run, Average Reset Point, aktueller/furthest Fortschritt, je drei Tages-/Session-Zähler, Session Success Rate, Deaths, Speed und Level/Mode. Speed bleibt in Clean-Speedruns gesperrt. SET-/UNKNOWN-Seed- und Assistenzwarnungen können nicht über einzelne Feldschalter entfernt werden.

## Seed-Erkennung und Integrität

Alle 22 RNG-Callsites werden geprüft: 20 TickCount-, eine Millisekunden- und eine Abstandsquelle. Der erste normale RNG-Seed wird erfasst und bleibt für den Run gespeichert. Er ist **kein einzelner reproduzierbarer Welt-Seed**, da das Spiel mehrfach reseedet. Feste Helper-Seeds werden anhand tatsächlich angewendeter RNG-Hooks erkannt und als **SET SEED** hervorgehoben.

Erkannte Trainingshilfen/Set Seed ergeben `RUN VALID: NO`. Ohne diese Beobachtungen bleibt die Verifikation `UNKNOWN`; die Version behauptet keine vollständige Prüfung externer Mods oder Speicheränderungen. Bestzeiten heißen deshalb ausdrücklich **Local PB**.

## Run-Daten

- Versuche zählen bei einem echten Run-Start, nicht pro Reload-Anforderung oder doppeltem Hook.
- Tod = ein Reset + ein Death; manueller Neustart nur für einen laufenden Run = ein Reset. Neustart nach Abschluss erzeugt keinen zusätzlichen Reset.
- Abschluss, unbekannte/interrupted Zustände und Prozessabbruch werden getrennt gespeichert.
- Seed, Seed-Typ, UTC-Start/Ende, Level, Ergebnis, Zeit, Fortschritt, Hilfen und PB-Flag pro Run.
- 64 Runs gespeichert, 16 in der Menü-History; Tages- und Session-Werte sind getrennt.
- Ein Worker schreibt atomar mit Backup; ungeänderte Render-Frames schreiben keine Stats. Laufendes Journal alle fünf Sekunden, wichtige Übergänge direkt eingereiht, Shutdown mit Flush.
- Vorhandene V3/V4-Zähler und Level-PBs bleiben erhalten. Nicht erfasste alte manuelle Resets werden nicht rekonstruiert; Reset-Durchschnitt nutzt nur neue zeitlich erfasste Resets.

Die Version ist kompiliert und gezielt geprüft. Ein Lauf im Zielspiel unter Windows wurde hier nicht durchgeführt; tatsächliche Fokus-, Grafik- und Harmony-Laufzeiteffekte bleiben im ersten Spieltest zu prüfen.
