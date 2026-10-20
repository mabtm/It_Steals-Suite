# Prüfung 7.2.1 / mab

Elf gezielte Prüfgruppen erfolgreich: mod, trainer, managed, motion-managed, motion-native, renderer-adapter, movement-managed, ui, milestone, replay und layout. Logs: tests/results-7.2.1. Die 22 Prüfgruppen der 7.2.0-Basis bleiben dokumentiert unter tests/results-7.2.0 und PRUEFUNG_v7_2_0.md; nicht betroffene Prüfgruppen wurden nicht erneut als aktuelle Tests ausgegeben.

## Ursachen und Umsetzung

Die tatsächliche Game-DLL enthält zwei HideAndSeekIt-Koroutinen zur Distanz-Ausblendung. OccludeWalls verwendet AllWalls und deaktiviert ganze GameObjects außerhalb der Spieler-/Monsternähe. OccludeObjects verwendet AllProps und CielingLights. Die frühere 3D-Kopie ließ inaktive Objekte aus und fror diese lückenhafte Auswahl ein. Außerdem kopierte sie die gesamten Meshfilter-Daten ohne ursprüngliche Renderer-Batch-Bereiche. Die Live-Anzahl statischer Batches wird jetzt gemeldet; die fehlenden ursprünglichen Batch-Zuordnungen der Kopie sind ein strukturelles Problem, keine gemessene Behauptung über deren Zahl in einer konkreten Szene.

Die neue Ansicht verwendet die vorhandenen Renderer, ursprüngliche Transform-Hierarchien und Batch-/Submesh-Zuordnungen. Nur die tatsächlichen Culling-Kataloge werden wieder angezeigt. Gemeinsame flache Materialien ersetzen Originalmaterialien vorübergehend; getrennte globale und per Material gesetzte Property-Blocks sowie Schattenzustände werden gesichert und zurückgesetzt. Andere inaktive Varianten und eingesammelte Ziele bleiben erhalten. Originale Spiel-Updates und Iteratoren pausieren, während skalierte Zeit angehalten ist. Nach Ende laufen ursprüngliche Iteratoren weiter.

## Neue Prüfungen

renderer-adapter führt die produktiven ReplayRendererScope-, ReplayRestoreScope- und ReplayPlaybackClock-Dateien gegen kleine Host-Doubles aus. Geprüft: zwei Renderer mit verschiedenen Bereichen desselben simulierten statischen Batches, ursprüngliche Materialslot-Anzahl, gemeinsame Materialreferenz, globale/per Slot gesetzte Property-Blocks, Schatten-Aus, Wiederanzeige nur bekannter inaktiver Objekte, unberührte Varianten, Actor-Ausblendung, keine neu erzeugten Szeneobjekte und vollständige/idempotente Rücksetzung. Ein simulierter Fehler während der Materialzuweisung muss rücksetzbar bleiben. 2000 ungleiche Framezeiten einschließlich längerer Stalls müssen auf der tatsächlichen Wiedergabeskala bleiben; zusätzlich Pause, Seek, Geschwindigkeitswechsel, exaktes Ende und ungültige Werte.

motion-managed prüft anhand der realen Game-DLL die Update-Ziele aller Hauptgegner/Spieler und die beiden ursprünglichen Culling-Koroutinen einschließlich ihrer tatsächlichen GameObject-SetActive-Aufrufe und Feldnamen. Pause-Prefix muss Iteratorfortsetzung erhalten; normales Spiel behält den ursprünglichen Aufruf und Rückgabewert. Bisherige Eingabe-/Wire-/Kameradaten-/Layout-/Writer-Prüfungen bestehen weiter.

motion-native führt vollständige Paket-Exporte und -Importe einschließlich eines 7.2.0-Manifests aus, prüft Originalaufnahme, Notizen/Prüfbericht, beschädigte Daten und unveränderliche Identität. Der Test wartet ausdrücklich auf das Ende des Archiv-Scans; ein Zwischenstand mit busy=true gilt nicht als fertiger Import. Die exportierte ZIP wird zusätzlich unabhängig mit Python zipfile/json/hashlib geprüft.

Die produktive ImGui-Oberfläche besteht ihre bisherigen 144 Tab-/Viewport-/DPI-/Verbindungsfälle, 216 HUD-Konfigurationen, 128 Overlay-Regeln und 288 Minimap-Fälle sowie Replay-Eingabe-/3D-Befehlsprüfungen. Bewegung, tatsächliche Run-Zeit, Flash-/Monsterdaten, ältere Replays und lokale Gültigkeitsmodelle behalten ihre Regressionen. Native Modell-/Archivprüfungen verwenden AddressSanitizer/UndefinedBehaviorSanitizer. Helper und Windows-x64-Trainer sind mit strengen Warnungen gegen die tatsächlichen Game-/Unity-/Loader-Signaturen gebaut.

## Grenzen

Die Renderer-Adapter prüfen Zustandsverwaltung und Wiederverwendung; sie führen keine echte Unity-/D3D-Rasterisierung aus. Ein Windows-/Unity-Livetest und reale FPS-/Frametime-Messungen sind hier weiterhin nicht möglich. Daher wird weder garantiert, dass jede Treiber-/Shaderkonfiguration live identisch zeichnet, noch ein konkreter FPS-Gewinn behauptet. Runtime-Diagnose meldet Renderer-/Batch-/Culling-Zahlen und Shader. Null verwendbare Level-Renderer brechen die Vorschau mit Rücksetzung ab.

3D verwendet flache sichtbare Originalgeometrie, keine vollständige Weltzustand-Neusimulation. Monster bleiben Positionsmarker; Animationen, Radarbildschirm, Ton und dynamische Weltgeschichte fehlen. Die funktionierende Aufnahme wird nicht neu quantisiert oder geglättet. Große Kameralücken bleiben erkennbar. Die bisherigen Datenformate, Input- und Route-Aufnahme, Flash-Erkennung und 18 Kern-Dateien bleiben zur 7.2.0-Basis erhalten; Versionsfallbacks werden separat dokumentiert. Kein Eingriff in globale Windows-/Spiel-Grafikeinstellungen und kein neues Anticheat-Versprechen.
