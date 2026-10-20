# Änderungen 7.3.5 / mab

- Auf Nutzerwunsch den 7.3.4-Meshansatz zurückgenommen. Wiederverwendung ursprünglicher Renderer/Batches; ReplaySurfaceGeometry und ReplayHorizontalBatch, Mesh-Splitting, Ersatzrenderer und GPU-Rücklesung entfernt.
- Tatsächliche Spiel-Shader-Metadaten geprüft: Standard FORWARD enthält 22 Vertex-/28 Fragment-Alpha-Varianten, darunter keine Kombination mit INSTANCING_ON. Fade-Materialien fordern Instancing nicht mehr an und erhalten vollständigen Transparent-Renderzustand. Dies belegt die Variantenkombination in den gelieferten Daten, nicht den alleinigen Grund des beobachteten Bildfehlers.
- Alpha wird direkt über eigene vorbereitete Wandmaterialien gesetzt. Keine gemeinsamen Boden-/Prop-Farben oder indizierten Alpha-Property-Blocks. Originale Materialslotzahl und Batch-Bereiche bleiben erhalten; eigene Materialien werden beim Beenden freigegeben.
- Replay-Kamera respektiert die Originalmaske; die bisherige Freischaltung jedes gefundenen Renderer-Layers entfällt. Die tatsächliche RadarBlink-Spielroutine verwendet getrennte Radar-/Enemy2-/Walls-/RadarSphere-Ebenen.
- Verfolgerkamera und Sichtstrahlen verwenden den tatsächlich aufgenommenen Kamerafokus. Kein pauschaler Body-plus-1,2-Höhenwert mehr im produktiven Replay-Pfad.
- Floor-/Clock-/Prop-/Decken-Layer, Prop-Tags, gemessene Controller-Bodenhöhe sowie horizontal/ungültige Physiknormalen sind abgesichert. Floor-Schutz wird vor Materialnamen angewendet.
- Maximal 20-Hz-Physikprüfung, vorbereitete Materialarrays und RGB-Werte, keine neuen Materialien pro Frame. Diagnosezähler werden höchstens einmal pro Sekunde aktualisiert.
- 7.3.5 / mab, Protokoll 19, Datei-/Paketformate gleich. Beide DLL/EXE ersetzen und Spiel/Trainer vollständig neu starten. Annotierte 7.3.4-Pakete bleiben lesbar.
- 12 aktuelle Prüfgruppen mit Shader-/Layer-Metadatenprüfung, tatsächlichen IL-Assertions, Produktionsadaptern, Builds, GUI-, Paket-/Fuzz-/Sanitizerprüfungen. Kein Windows-/Unity-/D3D-Livetest; tatsächlicher Bildfehler wird hier nicht als live behoben ausgegeben.
