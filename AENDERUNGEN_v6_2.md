# Änderungen 6.2.0

- Random Seed im Kartentab: unabhängige OS-Zufallszahl, unmittelbare Vorschau von Spawn/Kristallen/Route, kein Reload, ausgewählte Seed-Interpretation erhalten.
- Brute-Force-Modus: 10–2000 aufeinanderfolgende Seeds, vollständige Spawn-zu-allen-Kristallen-Routen, Automatic/Exact bis 15 bzw. Fast heuristic, Fortschritt und Abbruch.
- Beste Route automatisch auf der Karte; bis zehn unterschiedliche Treffer, Weglänge/Methodenkennzeichnung, Show/Use/Copy. Unerreichbare Routen werden übersprungen.
- Vorhandene geprüfte Stichproben können auch ohne Live-Helper ausgewertet werden. Config 8 speichert Suchumfang/Genauigkeit; bestehende HUD-/Map-/Run-Einstellungen bleiben erhalten.
- Getrennter, schlafender Suchworker mit Distanz-/Layout-Caches, begrenzten Navigation-Suchbäumen, CPU-Pausen und Cancellation. Clean Speedrun, verborgenes Tool und ungültiger Kontext stoppen laufende Sucharbeit.
- Kartenkanal v2 mit Request-Ticket zur eindeutigen Zuordnung der generierten Layouts. v1-Datei-Caches bleiben lesbar; Hauptlayout v6 unverändert. DLL und EXE zusammen ersetzen.

Die beste gefundene Route gilt für den geprüften Bereich und das NavMesh-Modell. Exakte Besuchsreihenfolge ist keine Garantie für schnellste reale Laufzeit oder global optimalen Seed. Jeder Replay bleibt SET PRACTICE.

V6.1.0 bleibt unverändert erhalten. Vollständige Builds und neue/ bestehende Regressionen werden in PRUEFUNG.md dokumentiert. Native Windows-/Unity-Live-Tests sind hier nicht ausführbar.
