# Prüfung 7.3.5 / mab

Aktuelle Gruppen: mod, trainer, native, managed, motion-managed, motion-native, renderer-adapter, replay-visuals, shader-assets, ui, milestone, layout. Vollständige Ergebnisse: tests/results-7.3.5. 7.3.4 wurde nach Nutzerfeedback verworfen; damalige Host-Prüfungen gelten nicht als Ingame-Erfolg.

## Neue Evidenz und Regressionen

- Tatsächliche Shader-/TagManager-Assets geprüft, ohne fehlende .resS-Daten vorauszusetzen: Standard FORWARD besitzt 22 Vertex-/28 Fragment-Alpha-Varianten und keine Alpha+INSTANCING_ON-Kombination. Relevante Floor-/Walls-/Radar-/Clock-/Prop-Layer und Tags sind bestätigt. SHA-256 und konkrete Anzahlen stehen in shader-evidence.json. Kein Rasterisierungsbeweis.
- Produktions-Fade-Factory: Instancing aus; AlphaBlend-Keyword und vollständiger Transparentzustand (RenderType/Queue/Blend/ZWrite). Produktiver Sicht-Adapter prüft tatsächliche renderer-eigene Materialfarbe Alpha statt lediglich eingereichter Property-Block-Werte. Gemeinsame Prototypen/Bodenmaterialien bleiben unverändert, Freigabe eigener Materialien genau einmal.
- Gleicher Wall-Material-/Layer-Hinweis für Boden und Wand: gemessener Floor-Schutz hat Vorrang. Clock-/Prop-Layer an Vorfahren schützen untergeordnete Renderer. Horizontale, fehlende oder nichtfinite Physiknormalen lösen keinen Wand-Fade aus. Getrennte Wandgrafik/Collider-Wurzeln, Treffer außerhalb von Renderer-Bounds und Treffer ohne Zuordnung bleiben geprüft.
- Kameramaskentest für alle 32 Ebenen. Tatsächliches kompiliertes BuildStage-IL darf Kameramaske nicht erweitern und muss Layer vor Renderer-Styling prüfen. Tatsächliche RadarBlink-Spielroutine bestätigt getrennte Radar-Ebenen.
- Tatsächliches Follow-IL verwendet PoseFocus; Adapter prüft aufgezeichnetes Kameraziel, Radius und Blickrichtung. Alte Orbit-/Zoom-/Seek-/Loop-, Collider-/Trail-/Shader-/Nahgrenzen-Fälle laufen weiter.
- Kompilierte Assembly enthält ReplaySurfaceGeometry/ReplayHorizontalBatch nicht mehr. Originaler Renderer-/Restore-Adapter prüft Materialslots, statische Batch-Bereiche, Lichtdaten und vollständige/idempotente/Fehler-Rücksetzung.
- Physik-Hin-/Rückstrahlen bei maximal 20 Hz, normale Alpha-Rückkehr bei Kameramoduswechsel, unabhängige Deckenoption und Diagnosezähler für Kandidaten/Treffer/Zuordnungen/fadende Renderer.

## Weitere Prüfungen

Reale Game-/Unity-/MelonLoader-/Harmony-API-Builds (Helper und Windows-x64-Trainer), Warnungen als Fehler, Protokoll-Structgrößen und Layoutgenerator. C#-zu-C++-Begleitdateien, strikte Zeit-/Identitäts-/Formatprüfung, native Sanitizer/Fuzz-Fälle und Pakete einschließlich annotierter 7.3.4-Exporte. Python prüft ZIP/CRC/JSON/SHA-256 des aktuellen Exports unabhängig. Produktive GUI-Bedienung und Viewport-/DPI-/HUD-/Minimap-Fälle sowie Finish-/Diagnose-/Versionsanzeige behalten ihre Regressionen.

21 Kern-Dateien für Routing, Seed-Suche, Bewegungserfassung, Run-Validierung, Cursor und Spielpause sind bytegleich zur direkten 7.3.4- und wiederhergestellten 7.3.3-Basis (preserved.json). Zusätzlich bleibt ReplayRendererScope unverändert. Nicht erneut ausgeführte historische Gruppen sind keine aktuellen Prüfläufe.

## Performance und Grenzen

Keine GPU-Rücklesung, Mesh-Kopien oder Geometriebauten. Quellen und Alpha-Materialien werden einmal vorbereitet. Pro Frame ändern sich nur aktive Materialalphas; RGB ist gecacht. Opaque Materialien werden nach Fade wieder gemeinsam verwendet. Diagnosezeichenketten entstehen höchstens einmal pro Sekunde, Physikabfragen höchstens 20 Hz mit festen Trefferpuffern.

Kein laufendes Windows-/Unity-/D3D-Spiel. Die Shaderprüfung belegt gelieferte Varianten, die Adapter CPU-/Physik-/Materialzustände und IL-Verträge. Die tatsächliche Auswahl eines Treiber-Shaders, Rasterisierung und Frametime sind nicht live geprüft. Ein Gegenbeispiel im Spiel hat Vorrang vor Host-Adaptern. Headless-Mono-Hinweise auf fehlende native Unity-Aufrufe gelten nicht als Engine-Test.

Diagnose: `walls` zählt vorbereitete Wandrenderer, `hits` die Physiktreffer beider Sichtstrahlen, `matched` ihre Rendererzuordnungen, `faded` derzeit abgesenkte Materialalphas. Beide Strahlen können dieselbe Wand treffen. Das sind Beobachtungsdaten, kein Beweis sichtbarer GPU-Transparenz. Shadername und Original-Kameramaske stehen im MelonLoader-Log.
