# Änderungen in 6.0.0

Ausgangspunkt ist die funktionierende 5.0.1, keine Rückkehr zum älteren Projektstand. Der Maus-/Focus-Fix und der Ausschluss von Compact HUD und Speedrun-Overlay bleiben erhalten.

- Trainer kann vor dem Spiel starten, nach Spielende weiterlaufen und sich erneut verbinden. Ein Tray-Menü bietet Öffnen und Beenden.
- Alte Steuerbefehle und Hilfen werden bei Prozess-/Helper-Session-Wechsel verworfen; Snapshots werden anhand der Prozessidentität geprüft.
- Positions-Segmenttraining mit Start-/Finish-Punkten, F7-Wiederholung, bestätigtem Start, PB, Versuchen und Erfolgsrate.
- Minimap mit Follow/Overview, North-Up/Player-Up, Zoom, Radius, Markerfiltern, begrenztem Trail und Practice-Markern. Unity-Orientierung korrigiert.
- Vier neue Segmentfelder, Practice-/Verification-Presets und insgesamt 29 konfigurierbare HUD-Werte.
- Game-Dateihash, gemeldete Game-/Unity-Version und tatsächliche RNG-Abdeckung fließen in die konservative Seed-Einordnung ein.
- Run-Warnungen, latched Hilfen und beobachteter Start werden gespeichert. Fallback-Anbindung kann keinen vollständig beobachteten Start oder PB behaupten.
- Verification Summary und optionales JSON-Eventlog; keine VALID-/YES-Freigabe, Reports sind lokale editierbare Evidence.
- Versionierte, gesicherte Stats/Config, Recovery, beschädigte Dateien aufbewahren, neuere Formate vor Überschreiben schützen.
- Gleiche UTC-Startzeitpunkte erhalten getrennte Run-IDs. Bestehende Reports werden bei Crash-Recovery nicht überschrieben.
- File-I/O für normale Speicherung außerhalb der Render-/Unity-Schleifen; coalesced Aufträge und begrenzte Queues.
- Bedarfsabhängige Map-Kategorien, wiederverwendete Geometrie-Listen, gecachte Hitbox-Grundformen und verbesserte Material-/Collider-Ressourcenfreigabe.
- Scene-Epoch und erfolgreiche Teleport-Serials verhindern falsche Interpolation, Bewegungs-Spikes und alte Trails.
- Practice-Kernfunktionen stehen vor erweiterten Szenenwerkzeugen; Diagnose und Speicherfehler sind besser zugänglich.

Protokoll v5 erfordert DLL und EXE aus diesem Paket. Details, Tests und Einschränkungen stehen in README, AUDIT_v6 und PRUEFUNG.
