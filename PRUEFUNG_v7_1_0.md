# Prüfung 7.1.0 / mab

20 Prüfgruppen erfolgreich: mod, trainer, managed, native, ui, planner, search, settings, portable, playback, hide-seek-player, replay, layout, movement-managed, movement-native, hud-adapter, experience, audio-adapter, intro-adapter und milestone. Logs: tests/results-7.1.0. Native Bilder/GIF: tests/previews-7.1.0. Die vorherige Prüfung bleibt als PRUEFUNG_v7_0_3.md erhalten.

## Oberfläche und Interaktion

Die reale ImGui-Oberfläche wird in 144 Kombinationen aus sechs Bereichen, Viewport, DPI und Verbindung/Run-Modus ausgeführt. Zusätzlich: 24 Settings-Unterseiten, 216 HUD-Konfigurationen, 128 Overlay-Policy-Fälle und 288 Minimap-Kombinationen. Die bestehende Mindestgröße der Replay-Karte wird weiter geprüft: mindestens 350 px bei 1280×720 und 650 px bei 1920×1080 bei 100 % DPI. Für die große Karte entfällt die doppelte Seitenüberschrift; Wechsel in andere Bereiche stellt die vorherige Fenstergröße wieder her.

Maus und Tastatur bedienen die neue Seitenleiste und die Funktionskarten tatsächlich. Clean-Speedrun- und Offline-Sperren werden gegen echte Aktivierungen geprüft. Pickup- und Flash-Marker springen auf ihre exakten gespeicherten Zeitpunkte. Dragging wird mit der tatsächlichen Bildschirm-Pixelauflösung geprüft; Home/End setzen Anfang/Ende. Pfeiltasten und Shift-Seek werden mit aktivierter ImGui-Tastaturnavigation geprüft; der Fokus bleibt dabei auf der Timeline. Manueller Seek pausiert, ein anderer Run verwirft alte Ereignis-Bins. Zeichnungen bleiben in den tatsächlichen UI-/HUD-Flächen; Opacity und die Pflicht-Validitätsanzeige behalten ihre Prüfungen.

Die Vorschau führt den echten Map-Worker, die Seed-Suche, Replay-Ausgabe und Ergebnisdarstellung mit synthetischen Daten aus. Sie prüft ausdrücklich, dass die gezeigte Replay-Aufnahme vollständig geladen ist. Die Galerie ist damit eine native Renderer-Vorschau, keine nachgebaute Webseite und keine Windows-/Unity-Liveaufnahme. Ein eigenständiges HTML-Dokument bietet vergrößerbare Einzelbilder. Die HUD-Kachel ordnet getrennte Profilvorschauen zusammen an; die Minimap bleibt im echten Clean-Speedrun gesperrt. Host-Schrift: DejaVu Sans als Ersatz; unter Windows gilt die gewählte verfügbare Schrift.

## Begrenzung und PB

ResultHighlights verwendet höchstens zehn feste Einträge und unterscheidet neue Seeds von Rangwechseln. Neue Suche/Sitzung setzt die kurzen Effekte zurück, reduzierte Bewegung beendet die Animationen. TimelineMarkers enthält höchstens 2048 Spalten für bis zu 4096 Flash-Beobachtungen. Tests decken ungültige Zahlen, Null-Dauer, Ränder, exakte Ereigniszeiten und Neubelegung ab. Der Ereignis-Cache wird nur bei Änderungen aufgebaut, nicht bei jedem Replay-Frame.

Die PB-Prüfung verlangt den tatsächlichen Helper-PB-Status, einen abgeschlossenen gültigen Run und eine passende endliche Verbesserung beziehungsweise den ersten PB. Nicht verbesserte Zeiten, fehlendes Flag, NaN, ungültige, abgebrochene oder aktive Runs lösen keine PB-Feier aus. Die native GUI prüft genau einen PB-Ton und kein PB-Abzeichen bei ungültigem Ergebnis. PCM-Gain, leise Hüllkurve, Ende bei Null, Gerätestörungen, Laufzeitgrenze, Mute und Abbruch laufender Audio-Puffer werden mit dem produktiven Audio-Worker und WinMM-Testadapter geprüft. Keine Änderung am System-/Spielmixer.

## Unveränderte Systeme und Grenzen

13 zentrale Routing-/Movement-/Light-/Freecam-/Collider-Dateien sind bytegenau zum gesicherten 7.0.3-Paket unverändert (core-preservation.json). Die sieben Check-Prädikate und FinalValid sowie SharedProtocol/SharedLayout, Map-Dekodierung, Map-Worker und SeedSearch sind ebenfalls unverändert (validation-preservation.json). Keine neue Spiel-Telemetrie, kein zusätzlicher Prozess-/Speicherscan. Hide & Seek / Outside bleiben WIP. Bestehende Einstellungen und Replays bleiben kompatibel.

Windows-x64-EXE und Helper-DLL wurden mit strengen Warnungen gebaut. Native Logik-/Adapter-Prüfungen verwenden AddressSanitizer/UndefinedBehaviorSanitizer, die GUI-Prüfung führt das echte ImGui aus. Die verwalteten Prüfungen verwenden Mono und geeignete Unity-Testreferenzen; deren bekannte Runtime/Referenz-Hinweise sind keine Live-Spielausführung.

Ein Windows-/Unity-Livetest ist auf diesem Host nicht möglich. Daher keine Zusage gemessener realer Spiel-FPS, live gehörter Audiogeräteausgabe, endgültiger Game-Fokus-Behandlung oder Grafiktreiber-Ausgabe. Diese Grenzen betreffen den Host; sie ersetzen nicht die erfolgreichen Modell-, Adapter-, Wire-, Archiv-, Build- und GUI-Prüfungen. Die lokale Run-Validierung ist weiterhin kein vollständiger Nachweis gegen jede fremde Speicheränderung.
