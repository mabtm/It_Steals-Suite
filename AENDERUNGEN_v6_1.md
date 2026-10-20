# Änderungen 6.1.0

Ausgangspunkt: ausgeliefertes 6.0.1-Paket, vor Beginn bytegenau abgeglichen. Das vorherige Paket bleibt erhalten.

- Segment Practice vollständig entfernt: Runtime, Punkte, F7, Settings, Minimap-Marker und vier HUD-Felder. Config 7 erhält die relative Reihenfolge aller übrigen 25 Felder.
- Neuer Seed Map-Tab: vollständige Karte, Spawn-/Kristallkatalog, ausgewählte Verteilung, Zoom/Pan/Fit, Koordinaten, Route und Heatmap.
- Echte Spawn-/Point-Arrays vor der Auswahl erfassen; Unity-RNG mit finally-Restore verwenden; Modell und Replay gegen tatsächliche Spawns/Koordinaten prüfen. Timer-Erststart und UNKNOWN berücksichtigen.
- Erfassten Random-Spawn-Seed und bestehenden Practice-Base-Seed getrennt eingeben. Jeder explizite Replay bleibt SET PRACTICE; keine vollständige Welt-Reproduktion behaupten.
- Tatsächliches NavMesh: exakte Besuchsreihenfolge bis 15 Kristalle, Multi-Start/2-opt darüber; Korridorprüfung, getrennte Stockwerke, unerreichbare Ziele, Cancellation und Grenzen. Matching-Route optional auf der Live-Practice-Minimap.
- Abbrechbares Seed-Sampling, Häufigkeiten, empirische Prozentwerte, Eröffnungsdistanzen und bestgemessener Spawn. Atomare Karten-/Layout-/Stichproben-Caches im Hintergrund.
- 2-MiB-Kartenkanal außerhalb der Haupttelemetrie, nicht wartende Synchronisierung, Helper-Sitzungsbindung. Navigation/Decoding/Dateizugriffe auf einem Worker; Kartenfehler blockieren keine Run-/Reload-Funktionen.
- Tatsächliche Spieler-/Kristall-Collider statt Schätzungen; Shapes standardmäßig aktiv, Deduplizierung, Spielerpriorität, Details und klare Farben. Mesh/Terrain bzw. Bounds bleiben Annäherungen.
- Hauptprotokoll v6: 80-Byte-Commands, 7338-Byte-Payload. DLL und EXE gemeinsam ersetzen.

Erhalten: Eingabestrategie, Timer-Menüabbruch, Minimap-Vorwärtsachse, ausschließlich Compact/Speedrun ohne Presets, Run-Daten, PB-Regeln und Reports. Clean Speedrun sperrt den Planner; vergangene Nutzung wird als SpawnPlanner-Hilfe erfasst.

Builds und automatisierte Tests bestehen. Die Game-/Harmony-/RNG-Prüfungen verwenden echte Game-DLL/IL und reine Modelle; native Unity-RNG-Aufrufe und PhysX sind hier nicht live ausführbar. Details: PRUEFUNG.md.
