# Prüfung 6.9.7

Alle 16 Gruppen erfolgreich: mod, trainer, managed, native, ui, planner, search, settings, portable, playback, hide-seek-player, replay, layout, movement-managed, movement-native und hud-adapter. Protokolle unter tests/results-6.9.7, Vorschauen aus realer ImGui-Zeichenausgabe mit synthetischen Daten unter tests/previews-6.9.7.

## Builds und Daten

C#-Mod gegen die bereitgestellten echten Spiel-, Unity-, MelonLoader- und Harmony-DLLs; Windows-Trainer als 64-Bit-PE mit MinGW. Beide mit Warnungen als Fehler. Protokoll 15: 128-Byte-Commands, 16276-Byte-Telemetrie. Generierte C#-/C++-Layouts geprüft. Neue Payload 11, unveränderter Kartenkanal 9. C#/C++-Übereinstimmung für kumulative Messdaten, alte Formate, Light Events, Monsterzustände, Checksummen und Validierung. Archive und Binärdateien werden beim Packen auf Integrität und Frische geprüft.

## Bewegungsmessung und Prognose

- Tatsächliche MouseLook.FixedUpdate-Instruktionen: 0,14 Sprint-/0,05 Geh-Einheiten pro Physikschritt, keine Input-Normalisierung. Alle drei ursprünglichen GetAxis-Aufrufe werden einmal weitergereicht. Der Harmony-Finalizer unterdrückt keine Spielausnahme.
- Automatische fünf Klassen: Gerade, Kurve, enge Kurve, Steigung und Start nach Stillstand. Diagonale gerade Schritte ergeben die erwarteten rund 9,90 u/s, Kurven behalten Verluste durch den tatsächlichen Input. Enge Kurven integrieren die volle Schrittstrecke, keine verkürzte Sehne.
- Ungültige/gehinderte Bewegung, NaN, Teleports und ineligible Schritte erhöhen keine gültige Messbasis. Histogramme/Dateigrößen bleiben begrenzt; Modus-/Build-/Physik-/Körperänderungen trennen Profile.
- Alle acht gespeicherten Modelle, Prüfsumme, Neustart, Zusammenführen, atomare Ersetzung und Backup geprüft. Beschädigte Originale bleiben erhalten. Ein zukünftiges Format wird nicht überschrieben.
- Öffnen einer Aufnahme publiziert kein Referenzprofil. Ein eingefrorener kumulativer Stand bleibt lokal bei der Aufnahme. Neue Live-Messung ersetzt nur kompatible Live-Daten.
- Prognose berücksichtigt Startkosten auch über viele kurze erste Segmente, echte Kurven-/Steigungsklassen und gemessene Abdeckung. Ohne enge-Kurven-Daten wird keine sanfte Kurve als vermeintliche Messung eingesetzt. Ungültige/unbestätigte Routen ergeben keine Prognose. 2000 zusätzliche Routen-Fuzzfälle unter ASan/UBSan; 20.000 Prognosen etwa 63 ms auf dem Host mit Sanitizern (kein Live-Spiel-Benchmark).
- Die aufgezeichnete Run-Zeit wird für Details weiter aus Bewegung, Stillstand und Beobachtungslücken rekonstruiert; sie trainiert keine Orts-/Referenzprognose.

## HUD und Performance

Die unveränderte produktive LayeredHud-Klasse wurde mit typisierten Win32-/D3D11-Doubles ausgeführt: zugeschnittene CopySubresourceRegion-Aufrufe, gepolsterter RowPitch, unveränderte Pixel-Alpha, zwei nicht blockierende Slots, veralteter Status-Badge bei belegter GPU, zugehörige Position des gepufferten Frames, negative/verschobene Fensterursprünge, Hide/Show, Eigentümerbindung, NoActivate, weniger Panels, Resize und wiederholtes Aufräumen. Dies ersetzt keinen echten Windows-Treibertest.

Die echte ImGui-Geometrie wird zusätzlich auf vollständige Abdeckung durch höchstens vier nicht überlappende Regionen geprüft. Getrennte Panels, Deckkraft 0,25/1 und der obligatorische Status bleiben sichtbar. In der geprüften 1920×1080-Ansicht mit Minimap und kompaktem HUD in gegenüberliegenden Ecken: 476160 statt 8294400 Bytes pro Frame, rund 94 % weniger Readback-Daten. Diese Zahl gilt für die geprüfte Ansicht, nicht für alle Layouts oder Spiel-FPS.

Seed-Suche: derselbe vollständige Geometrie-/Katalogkontext verwendet vorbereitete Distanzen auch nach Request-/Epoch-/Seed-/Auswahländerung erneut und liefert identische Rankings. Punkte, Mesh, Radius, Höhe, Hash, Navigationsstatus und Modus invalidieren. Zwei Einträge begrenzen den Cache, Verdrängung geprüft. Nur vollständig vorbereitete Tabellen werden gespeichert; Arbeiter lesen unveränderliche Daten.

## Regressionen

168 Tab-/Viewport-/DPI-/Verbindungsfälle, 216 HUD-Konfigurationen, 128 Mode-Regeln und 288 Minimap-Ansichten. WIP-Farbe nur für Szene 5/13 in Minimap/Kartenansicht; Timing-Cache aktualisiert sich bei neuem Live-Modell. Tatsächliche Collider ohne Bounds-Fallback im Mod-Code geprüft.

Gemeinsame Routenpipeline aller acht Modi bleibt geprüft. Originale Hide-&-Seek-Bodendreiecke, Dachauswahl und Weltversatz: Innenkarte 824 Vertices/876 Dreiecke, Outside 4743/4390; jeweils 31 erreichbare Boden-Ziele und unabhängig geprüfte Wegstücke. Hide & Seek 22-Kristall-/Outside 52-Kristall-Suche liefert gleiche Top 10 mit einem und sechs Arbeitern. Weitere RNG-/Ranking-Tests einschließlich einer Million unterschiedlicher Seeds, Abbruch, Backpressure, Decoder-Fuzzing und gespeicherten Karten bleiben erfolgreich.

Replay-Speicherung/Backup, Notizen/Kategorien, orange ABORTED, Light-Flashes/Monsterwege, unabhängige Routenschalter, Koordinaten, Alpha, Status und Einstellungen bleiben geprüft. Native Logik-/Decoder-/Routen-/Adapterprüfungen nutzen ASan/UBSan; LeakSanitizer ist auf dem Host deaktiviert.

## Grenzen

Kein laufendes Windows-/Unity-Spiel verfügbar. Daher keine zugesagte FPS-Steigerung, Treiber-/Fokus-Bestätigung oder perfekt gelöste Hide-&-Seek-Wände. Die verbleibende Geometrieungenauigkeit ist WIP. Die Zeitschätzung bleibt eine Bewegungsprognose mit Modellspanne: Gegner, dynamische Hindernisse, echte Blick-/Eingabewahl und genaue Pickup-Kontakte können die Endzeit ändern. Der lokale Validitätsstatus ist keine manipulationssichere Anticheat-Attestierung.
