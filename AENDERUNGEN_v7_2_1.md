# PracticeSuite 7.2.1 / mab

- 3D verwendet die vorhandenen Level-Renderer direkt. Keine duplizierten Level-Meshes, keine verlorenen statischen Batch-Bereiche und keine doppelten Welttransformationen.
- Ursache im tatsächlichen Spielcode berücksichtigt: HideAndSeekIt.OccludeWalls und OccludeObjects deaktivieren entfernte ganze GameObjects. Die Analyse verwendet genau deren AllWalls-/AllProps-/CielingLights-Kataloge und erhält auch diese Geometrie. Beliebige inaktive Levelvarianten und gesammelte Ziele werden nicht aktiviert.
- Flache gemeinsame Farben für Wände, Boden, Decke, Objekte und Türen; keine Texturen, Schatten, zusätzliche Szenenbeleuchtung oder Postprocessing. Globale und materialbezogene Property-Blocks werden vorübergehend entfernt und anschließend wiederhergestellt, damit ursprüngliche Dunkel-/Alpha-Werte die Analyse nicht verdecken.
- Originale Welt-Updates und Spielkoroutinen pausieren während der Darstellung. Iteratoren werden nicht beendet oder neu gestartet. Außerhalb der Analyse bleiben die ursprünglichen Methoden und Ergebnisse erhalten.
- Monotone Replay-Uhr statt aufaddierter, gekappter Frame-Deltas. Kameraanwendung in jedem LateUpdate, Einstieg mit 1×, 3D-Zeitleiste immer an der tatsächlichen Aufnahmezeit. Keine Glättung über große Aufnahmelücken.
- Vorberechnete Kristall-Zeitpunkte ersetzen die wiederholte Suche in der Pickup-Liste; Sichtbarkeit wird nur bei tatsächlicher Änderung gesetzt. Keine laufenden Geometriescans während des Abspielens.
- Materialien, Property-Blocks, Schatten-/Aktivitätszustände und Kameras werden rücksetzbar verwaltet. Vorübergehend erlaubtes Hintergrund-Rendering hält die Ansicht bei der Menübedienung aktiv und wird danach zurückgesetzt. Fullbright wird vor dem Erfassen der Originalmaterialien sauber beendet.
- Diagnose enthält Rendererzahl, statische Batch-Zahl, wieder eingeblendete Katalogobjekte und verwendeten Shader. Fehlende Geometrie erzeugt eine erklärende Fehlermeldung mit Rücksetzung statt einer leeren Vorschau.
- Protokoll-/Aufnahmeformate bleiben unverändert; alte 7.2.0-Aufnahmen und Pakete bleiben lesbar. Neue Exporte zeigen 7.2.1 als Exporterversion.

Bewegungs-/Kamera-/Eingabeaufnahme, Flash-Erkennung, Navigation, Seed-Suche und Run-Validierung behalten ihre funktionierende Logik. 3D bleibt WIP für Windows-/Unity-Livetests, Monsteranimation und dynamische Weltzustände. Prüfungen: PRUEFUNG.md.
