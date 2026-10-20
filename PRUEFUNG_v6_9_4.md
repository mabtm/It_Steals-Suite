# Prüfung 6.9.4

Die Änderungen setzen die 6.9-Linie fort. Der verworfene Rastergraph aus 6.10 ist nicht enthalten.

## Geometrie

- Der Screenshot und der vorherige Capture-Code zeigen das Problem: Rechtecke um komplette Mesh-Collider können echte Öffnungen schließen. Solche Rechtecke werden weder als feste Hindernisse noch als Hide-&-Seek-Wände verwendet, auch beim Lesen älterer Karten.
- Routing, Seed Map, Minimap und Replay teilen die im Worker ausgewählten Bodenflächen und ihre Grenzen. Die Auswahl berücksichtigt die Bodennähe, zulässige Steigung und zusammenhängende Flächen. Rampen bleiben erhalten, isolierte Wandoberseiten werden ausgeschlossen. Genau erfasste Box-Collider ergänzen die Spielerkontrolle.
- Das ursprüngliche Outside-Asset überschritt mit fünf Stützpunkten je Übergang das interne Knotenlimit. Gemeinsame Mesh-Vertices und ein Mittelpunkt genügen für ungeteilte Übergänge; geteilte Übergänge erhalten zusätzliche legale Punkte.
- Zwei vom Nutzer bereitgestellte NavMesh-Assets wurden geprüft: Hide & Seek mit 824 Vertices / 876 Dreiecken, Outside mit 4743 Vertices / 4390 Dreiecken. Pro Asset wurden 31 verbundene Boden-Ziele erreicht und sämtliche ausgegebenen Wegstücke unabhängig gegen die Originaldreiecke geprüft. Die Auswahl enthält keine Dachpolygone. Ein zusätzliches Rampen-Fixture prüft steigende Böden.
- Synthetische Szenen prüfen reale Box-Hindernisse, schmale und gedrehte Wände, irreführende Mesh-Rechtecke, Änderungen von Hindernissen/Spielerradius und Abbruch. Die vorhandenen Modelle für Classic, Bonus, Camera, Timer und Ghost bleiben erhalten.
- 22-Kristall-Suche: 2000 Seeds bei einem und sechs Workern; vollständige Zählung und gleiche Top 10. Outside: 52 Kristalle aus einem 70-Punkte-Katalog, 128 Seeds, gleiche Prüfung bei einem und sechs Workern. Die ursprüngliche Seed-Erzeugung wird nicht durch die Zeitmodelle verändert.

## Aufzeichnung und Archiv

- Builds gegen die bereitgestellten Unity-/Spiel-/Loader-DLLs. Der Kamerahook ersetzt genau die beiden privaten IsStunned-Schreibzugriffe der untersuchten Spiel-Coroutine durch einen äquivalenten Setter mit Beobachtung; Raycasts und Spielaktionen bleiben erhalten.
- Format 8 stimmt zwischen C# und C++ überein: Validierungssnapshot, Positionen, kontinuierliche Monster-Schlüsselbilder, Navigationswege und Licht-/Kameraereignisse. Format 7 bleibt lesbar und bekommt ohne gespeicherten Snapshot keine nachträgliche VALID-Einstufung.
- Defekte Prüfsummen, abgeschnittene Dateien, falsche IDs/Counts, ungültige Zeitfolgen, unbekannte Formate und widersprüchliche Validierungsdaten werden abgewiesen. Unbekannte Integritäts-Warnbits gelten als ungültig.
- Das Archiv wurde mit 14 abgeschlossenen/abgebrochenen Aufzeichnungen geprüft: wiederholtes Laden, Speichern, Umbenennen, Kategorien, Unicode-Notizen, Neustart, unveränderliche Gültigkeit und Wiederherstellung beschädigter Metadaten aus Backup. Dateinamen stammen nur aus der Run-ID; Namen und Kategorien dienen nicht als Dateipfade.
- IO, Scans und Dateien werden im Hintergrund bearbeitet. Bei unveränderten Replay-Dateien wird die bereits gelesene Zusammenfassung wiederverwendet. Das Spiel bleibt während der Aufnahme unverändert; Timer erfasst tatsächliche WallAI-Gegner statt des Monster3-Controllers.
- Gerade und gekrümmte Gegnerbewegungen wurden für je 10.000 Beobachtungen unabhängig aus den Schlüsselbildern rekonstruiert: Abweichung höchstens zwei Zentimeter. Beobachtungslücken und Teleports werden nicht als kontinuierliche Bewegung behandelt.

## Zeitmodell und GUI

- Variierende Geschwindigkeiten und Stopps ergeben wieder die aufgezeichnete Gesamtzeit. Gleich lange Routen in schnell bzw. langsam gelaufenen Regionen erhalten unterschiedliche Schätzungen; der vorgeschlagene Marker folgt dem variablen Zeitprofil. Lücken, nicht geprüfte Wege und inkompatible Modus-/Build-/Spielerprofile bleiben ausgeschlossen.
- Die Kalibrierung ist keine unabhängige Vorhersageprüfung. Eine andere Route kann andere Gegnerbegegnungen, Aktionen und Einsammelpunkte haben. Diese kontrafaktische Zeit bleibt eine Schätzung mit ausgewiesener Modellspanne.
- Die reale ImGui-Oberfläche wurde bei kleinen und großen Fenstern, mehreren DPI-Stufen und Verbindungszuständen geprüft. Tests prüfen Replay-Liste und Bearbeitungspopup, unabhängige Spieler-/Vorschlagsrouten sowie Monster-/Pfad-/Blitzschalter, große Karte und Wiederherstellung der Fenstergröße.
- Bestehende Prüfungen für obligatorisches VALID/UNVALID im HUD, Deckkraft, Koordinaten, Overlay-Exklusivität, IPC-Layout, Settings-Backups und Seed-Übernahme laufen weiter.

## Grenzen

Hier läuft kein Windows-Spielprozess und keine Unity-Szene. Die Asset-Prüfung enthält echte NavMesh-Geometrie, aber keine Live-Collider-Instanzen. Native Pfadkopie, Cursor/Fokus, erfolgreiche Blendereignisse, aktuelle Wandzustände und tatsächliche Frametime unter Spielbetrieb benötigen einen Live-Test. Die Builds und Host-Prüfungen ersetzen diesen Test nicht; eine garantierte FPS-Verbesserung oder eine genaue zukünftige Run-Zeit wird nicht behauptet. VALID bezeichnet aufgezeichnete lokale Prüfungen, keine fälschungssichere externe Anticheat-Bestätigung.

Ergebnisprotokolle liegen unter tests/results-6.9.4. Die Vorschauen unter tests/previews-6.9.4 zeigen die reale ImGui-Ausgabe mit ausdrücklich synthetischen Daten.
