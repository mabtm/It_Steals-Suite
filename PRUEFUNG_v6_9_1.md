# Prüfung PracticeSuite 6.9.1

Stand: 13. Oktober 2026. Ausgangspunkt ist das unveränderte ItStealsSuite_v6_9.zip. Der Ansatz aus 6.10 wurde verworfen; dessen Raster- und Rechteckgraph ist nicht im Projekt enthalten. Die vorherige Prüfung von 6.9 bleibt in PRUEFUNG_v6_9.md erhalten.

| Bereich | Aktuell ausgeführte Prüfung |
|---|---|
| Wiederhergestellte Basis | Originaldateien aus Release 6.9; keine PlayerWallRouting.inl und keine PlayerWallCapture.cs aus 6.10 |
| Helper-Build | PASS: 24 C#-Dateien, Mono/C# 7.2, Optimierung, Warnungen als Fehler, tatsächliche Game-/Unity-/MelonLoader-/Harmony-Referenzen |
| Windows-Trainer | PASS: MinGW-w64 GCC 13, C++20, Optimierung, Warnungen als Fehler, tatsächliches ImGui/Win32/DX11, AMD64-EXE |
| Spielerbreite / Wandecken | PASS: direkte Wanddurchquerung gesperrt; ausreichender Abstand und ein gültiger Abstand an einer runden Ecke bleiben erhalten; 1.000 zufällige Fälle gegen eine unabhängige dichte Referenz |
| Vollständige Wegprüfung | PASS: ein späteres fehlerhaftes Segment darf keine zuvor geprüften Kanten oder zusätzliche Knoten in den Graphen übernehmen; gültige Umwege werden vollständig übernommen |
| C# zu C++ | PASS: tatsächlicher C#-Encoder einschließlich geprüftem Spielergraphen und Provenienz zum C++-Decoder; der vollständig abdeckende rohe Gegner-NavMesh darf den Umweg nicht ersetzen |
| Hide-and-Seek-Rangliste | PASS unter ASan/UBSan: 30 Kandidaten, 22 Kristalle, 2.000 Seeds mit 1 und 6 Workern; identische Top 10 und Distanzen, alle Seeds gezählt, keine ungeforderten Filter, alle ausgegebenen Top-10-Wege wandfrei |
| Alte / fehlende Navigation | PASS: keine bestätigten Spielerwege aus alten Gegner-Daten; gekennzeichnete Schätzungen bleiben verfügbar, Seeds werden dadurch nicht verworfen |
| Cache / Herkunft | PASS: identische Punkte und Graphdaten mit geänderter Navigationsherkunft erzwingen neue Analyse; keine Übernahme der alten AI-Bewertung |
| Andere Modi | PASS: Classic, Camera, Timer, Ghost, Classic Bonus und Camera Bright behalten das Routing-Verhalten aus 6.9 |
| CPU-Suche aus 6.9 | PASS unter ASan/UBSan: unabhängige C#/C++-Vektoren, Raw/Base, 8/18 Kristalle, 1/3/6 Worker, Stream-Parität, Spawn-Filter, vollständige Bilanz, Stop/Neustart und Validierung |
| Planner / Search | PASS unter ASan/UBSan: bestehende Decoder-Fuzztests, Bodenlöcher, Stockwerke, native Korridore, Exact/Fast, Ranking, Schätzungen, Filter und Cancellation |
| Managed / UI | PASS: RNG-Verifier, tatsächliche Start-Patches, Run/Persistenz, Katalogreihenfolge, Paketkanal und tatsächliche ImGui-Tests einschließlich Seed-Übernahme, HUD und Minimap |
| Ressourcen / Abbruch | Der temporäre Bake besitzt eigenen Agententyp, NavMeshData und Bodenmesh; Cancel/Dispose entfernt ausschließlich diese eigenen Ressourcen. Die realen API-Signaturen sind kompiliert; das native Laufzeitverhalten ist hier nicht ausführbar. Deferred-Cancel prüft zusätzlich das Zurücksetzen der Spieler-Provenienz. |
| Protokoll | PASS: Hauptprotokoll 13, weiterhin 100-Byte-Befehle und 9014-Byte-Telemetrie; Kartenkanal 8, Map-Format 6, Seed-Fallback-Format 3; Layout-Generator --check |

## Grenzen der Aussage

Die tatsächliche Unity-Erzeugung des temporären NavMesh kann auf diesem Host nicht im laufenden Spiel getestet werden. Die Geometrie- und Ranglistentests verwenden ausdrücklich synthetische Hindernisse; der C#-Graph und der binäre Encoder sind die produktiven Implementierungen. Ein Live-Test des Updates auf der konkreten Hide-and-Seek-Karte steht aus.

Die privaten Build-Sources berücksichtigen Collider einschließlich inaktiver Objekte und die Spieler-Kollisionsmatrix. Lesbare, nicht konvexe Meshes werden als Mesh erfasst; nicht lesbare bzw. konvexe Meshes verwenden konservative Grenzen, damit Unity keine Wände stillschweigend auslässt. Solche Grenzen können schmale Öffnungen schließen. Unlesbare Bodengeometrie wird nicht als gefüllter Rechteckboden ergänzt. Die erfasste Boden-Triangulation bleibt als Bodenquelle verfügbar.

Spielerradius, Höhe, Steigfähigkeit und Neigung kommen aus dem tatsächlichen CharacterController. Ein eigener Agententyp trennt die neuen Abfragen von den Gegner-Daten. Der einmalige native Aufbau ist asynchron auf einen Job-Worker begrenzt; Wegabfragen behalten das bisherige Zeitbudget. Bei nicht verfügbarer Navigation werden Wegschätzungen ausdrücklich markiert.

Die CPU-Generatoren und die Seed-Worker-Schleife sind gegenüber 6.9 unverändert. Dieses Update enthält keine neue Messung der Live-Seeds/s oder Spiel-FPS und beansprucht keine globale optimale Tour für 22 Kristalle. Später zerstörte oder bewegte Hindernisse ändern die erfasste Suchgrundlage nicht automatisch.
