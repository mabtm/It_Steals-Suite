> Historischer Stand 7.3.4: Dieser Mesh-/Readback-Ansatz wurde nach fehlgeschlagenem Ingame-Test auf Nutzerwunsch in 7.3.5 zurückgenommen. Die damaligen Host-Prüfungen belegen keine funktionierende Ingame-Darstellung.

# Prüfung 7.3.4 / mab

Aktuelle Gruppen: mod, trainer, native, managed, motion-managed, motion-native, renderer-adapter, replay-visuals, ui, milestone, layout. Vollständige Ergebnisse: tests/results-7.3.4. Die direkte Basis bleibt unter PRUEFUNG_v7_3_3.md dokumentiert.

## Reproduktion und gezielte Regressionen

Der neue Bodenfall wurde zuerst gegen 7.3.3 ausgeführt und scheitert dort: horizontaler Boden mit Walls-Layer und Wall-Material erhält fälschlich Wandrolle. Das Ergebnis liegt als regression-7.3.3-floor.txt bei. Die aktuelle Fassung schützt dieselbe Fläche.

Produktive Geometrie-/Sichtklassen werden mit Host-Unity-/Buffer-/GL-Doubles ausgeführt:

- CPU-unlesbare Meshes: tatsächlicher Produktionszweig liest einmalig nachgebauten Vertex-/Indexpuffer, einschließlich interleaved Stride/Position-Offset, Float32/Float16, UInt16/UInt32, indexStart und baseVertex. Handles werden freigegeben, geteilte Meshes nur einmal gelesen.
- Statischer Batch: ausschließlich eigener Submesh-Abschnitt; Renderer-Matrix statt abweichender Transform-Matrix; fremde Bodenabschnitte desselben kombinierten Meshes bleiben ausgeschlossen. Ungültige Bereiche werden abgewiesen.
- Dieselben Wall-Materialien für Boden, zwei Wände und Decke: nur die tatsächlich zwischen Kamera und Spieler liegende Wand erhält Alpha. Boden, andere Wand und Uhr behalten Deckkraft; Decke wird unabhängig ausgeblendet. Gemeinsame Materialobjekte und große Renderer-Bounds erzeugen keinen Übersprung.
- Boden und Wand im selben Materialslot: getrennte Replay-Abschnitte, unveränderte Originalmesh-Daten und deckender Boden trotz Wand-Fade. Rotierte horizontale Flächen werden in Weltkoordinaten erkannt. Prop-Tags sowie Prop-/Clock-Layer an Objekt oder Vorfahr schützen Einrichtung.
- Dreieckstest beidseitig, einschließlich Kamera innerhalb eines Wandvolumens. Türöffnung trotz umfassender Bounds bleibt frei. Alpha-Verlauf, 20-Hz-Abfragegrenze, Options-/Kameramoduswechsel und idempotente Rücksetzung sind geprüft.
- Fehlgeschlagener GPU-Zugriff, ungültige Indexbereiche und nichtfinite Positionen bleiben explizit unbrauchbar und lösen keinen geratenen Fade aus. Diagnose nennt fehlende Geometrie.
- Räumlicher Baum mit 20.000 Dreiecken: 1000 wechselnde echte Wandtreffer und freie Korridore innerhalb gemeinsamer Gesamtgrenzen. Host-Laufzeit steht im Log und ist kein Ingame-FPS-Versprechen.
- Bestehende Orbit-/Zoom-/Collider-/Trail-/Shader-/Nahgrenzen-Tests laufen weiterhin. Renderer-/Restore-Adapter prüft ursprüngliche Materialslots, statische Batch-Zuordnung, Lichtdaten, Property-Blocks und Wiederherstellung auch bei Fehlern.

## Weitere Prüfungen

Helper gegen reale Unity-/Game-/MelonLoader-/Harmony-Signaturen gebaut, Windows-x64-Trainer gebaut; Warnungen als Fehler. Hauptprotokoll 19, Structgrößen und Layoutgenerator geprüft. Native Sanitizer/Fuzz-Fälle, C#-zu-C++-Begleitdateien, alte Formate und Hintergrund-Archivpfade einschließlich annotierter 7.3.3-Pakete. Python zipfile/json/hashlib prüft exportierte Originalbytes und Paketmanifest unabhängig. Produktive ImGui-Oberfläche behält ihre Bedien-/Viewport-/DPI-Fälle; Finish-/Diagnose-/Versionsanzeige geprüft.

21 Kern-Dateien einschließlich Routen-/Kartenberechnung, Seed-Suche, Run-Validierung, Bewegungs-/Replay-Erfassung und Spielpause sind bytegleich zur direkten 7.3.3-Basis; Hashliste in preserved.json. Nicht erneut ausgeführte historische Prüfgruppen werden nicht als aktuelle Ergebnisse ausgegeben.

## Performance und Grenzen

Vorbereitung findet nur bei pausierter 3D-Szene statt. Maximal 262144 Vertices pro Originalmesh, 1572864 Indizes und 262144 vorbereitete Dreiecke insgesamt; Originalmesh-Cache maximal 64 MiB. GPU-Rücklesen erfolgt einmal pro geteiltem, CPU-unlesbarem Mesh und kann den Ladevorgang beeinflussen. Danach wird der Raw-Cache freigegeben. Gemischte Renderabschnitte erhalten nur ihre eigenen Dreiecke als Replay-Mesh; keine vollständigen statischen Batch-Mesh-Kopien. Zusätzliche Replay-Meshes werden beim Beenden zerstört. Die Sichtprüfung verwendet vorbereitete Bäume, vorab erstellte Materialarrays und Property-Blocks; maximal 20 Hz, keine GPU-Rücklesung im Abspielpfad.

Kein Windows-/Unity-/D3D-Livetest. Die Adapter prüfen Codepfade, Bufferdecodierung, eingereichte Geometrie, Materialien und Rücksetzung, keine tatsächliche Treiber-Rücklesung oder Rasterisierung. Die angehängten Assets enthalten Mesh-Metadaten mit isReadable=false; zugehörige .resS-Daten und vollständige Leveldateien liegen hier nicht vor. Deshalb wird kein vollständiger Originallevel-Nachbau oder Livevergleich mit dem Screenshot behauptet. Reale Darstellung, Ladezeit und Frametime müssen im Spiel beurteilt werden. Fehlgeschlagene Bufferzugriffe erscheinen als missing im Diagnosebericht und mit Ursache im MelonLoader-Log.

Die Replay-Aufnahme und der geplante unabhängige Moderator-Prüfer sind unverändert. Datei-Hashes belegen Konsistenz, keine manipulationsfreie Spielausführung.

Boden-/Deckenstücke aus gemischten Renderern werden nach den vier bestehenden Farbvarianten gebündelt: höchstens acht gemeinsame horizontale Zeichenabschnitte pro Szene. Wandabschnitte behalten unabhängige Fade-Steuerung. Bestätigte Props werden ohne unnötigen Mesh-Readback direkt geschützt.
