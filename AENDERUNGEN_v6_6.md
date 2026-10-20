# PracticeSuite 6.6.0

- Automatische Suchlast anhand echter unscaled Spiel-Frames und Pipeline-Rückstau, optional verständliche manuelle Obergrenze statt Generation Budget.
- Dynamische Restzeit mit Sekunden/Minuten/Stunden/Tagen, Anlauf- und Stall-Behandlung.
- Spawn als einziger Filter; nächster Kristall, Required/Excluded und Distanzgrenzen entfernt. Zwei sinnvolle Routen-Suchziele bleiben.
- MST-Untergrenze beschneidet nicht konkurrenzfähige Shortest-Route-Layouts, ohne Ranglisten-Gleichstände zu verlieren; begrenzter Cache speichert auch verworfene Grenzen. Fortschrittsveröffentlichung während großer exakter Batches.
- Camera Bright (Szene 12) in Practice, Seed Map und Reports. Bisherige Level-Indizes unverändert.
- Timer-Marker von tatsächlichen WallAI-Objekten; nächster echter Gegner statt Controllerposition, Zustandsfarben und Überlaufmeldung.
- Timer-Wall-/Ceiling-Filter, verzögerte NavMesh-Erfassung, deduplizierte Dreiecke und vollständige gecachte NavMesh-Ränder auf der Minimap.
- Spawn/Layout-Suche bleibt ohne NavMesh möglich; keine erfundenen Routenergebnisse. Clean Mode bleibt gesperrt.
- Minimap-Geometrieanfrage und Seed-Research werden für die Run-Dokumentation getrennt erfasst.
- Config 11, Hauptprotokoll 9. DLL und EXE gemeinsam ersetzen; alte Configs und Karten-Caches bleiben lesbar.
- Weitere Sanitisierung für Gegner-/Budget-Telemetrie. Bestehende Maus-, Overlay-, ESC-, Seed- und Statistik-Regeln erhalten.

Binaries, Quellen, Regressionstests und Performance-Dokumentation sind enthalten. Windows-/Unity-Liveprüfung bleibt erforderlich; Testmaps sind synthetisch.
