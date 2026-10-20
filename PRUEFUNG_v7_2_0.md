# Prüfung 7.2.0 / mab

22 Prüfgruppen erfolgreich: mod, trainer, managed, native, ui, planner, search, settings, portable, playback, hide-seek-player, replay, layout, movement-managed, movement-native, hud-adapter, experience, audio-adapter, intro-adapter, milestone, motion-managed und motion-native. Vollständige Logs: `tests/results-7.2.0`. Die vorherige Prüfung bleibt als `PRUEFUNG_v7_1_0.md` erhalten.

## Aufzeichnung und Spielcode

Die verwalteten Prüfungen lesen die tatsächlichen Update-/FixedUpdate-Instruktionen der unterstützten Game-DLL und wenden die produktiven Transpiler an. Die ersetzten Eingabefunktionen werden jeweils genau einmal aufgerufen; ihr Rückgabewert bleibt erhalten. Getrennt geprüft: Radar-Anforderung gegenüber tatsächlichem Radarzustand, bereits beim Start gehaltene Tasten, eigene Render-/Physiksequenzen, Blickachsen, FOV sowie tatsächliche Lichtparameter. Kamera, Physik und Ereignisse verwenden die vorhandene Run-Uhr. Eingaben werden ausschließlich im Spiel beobachtet.

Der C#-Encoder erzeugt Daten, die der C++-Decoder unabhängig liest. Geprüft werden feste Datensatzgrößen 168/56/36 Byte, monotone Zeit-/Frame-/Tickfolgen, initiale Tastenzustände und Übergänge, endliche Werte, Quaternionen, Anzahl-/Identitätsgrenzen, beschädigte Prüfsummen, Trunkierungen und 1500 manipulierte Eingabedateien. Ältere Map-/Replay-Payloads bleiben lesbar; neue Metadaten werden mit Run-ID, Modus, Seed, Dauer und Build verbunden. Frame-Schritte nutzen tatsächliche Zeitstempel; Kameralücken werden nicht als lückenlose Aufnahme behandelt.

Ein synthetischer Host-Durchlauf mit 24000 Kamerasamples (100 Sekunden bei angenommenen 240 FPS) erzeugt 4032101 Byte. Anhängen und sequentielles Kodieren werden getrennt gemessen; Werte stehen im Log. Das misst weder Unity-Aufnahmekosten noch reale Spiel-FPS. Puffer sind begrenzt; weitere Proben werden bei Erreichen der Grenze ausdrücklich gezählt statt ältere Eingaben zu überschreiben. Die Eingabedatei wird im Hintergrund geschrieben, vor der abschließenden Veröffentlichung der kleinen `.isrun`-Datei.

## Dateien und vollständige Run-Pakete

Die produktiven Hintergrundarbeiter speichern und lesen Aufnahme, Eingaben und ursprünglichen Validierungsbericht tatsächlich im Test-Dateisystem. Export/Import einschließlich Notizen, Manifest und Originalbericht werden vollständig ausgeführt. Bekannte SHA-256-Vektoren (leer, abc, eine Million a) und CRC32 werden gegen Referenzwerte geprüft. ZIP-Lokal-/Zentralverzeichnis, CRC, Hashliste, Größen, unerwartete/doppelte/Pfad-Dateinamen, fehlende notwendige Eingaben und bestehende Dateikonflikte werden geprüft. Zusätzlich wird ein exportiertes Paket unabhängig mit Python `zipfile`, `json` und `hashlib` gelesen.

Ein besonderer Regressionstest verändert nur PID-/Live-Transportinformationen einer geladenen Aufnahme: Eingaben müssen weiterhin zur unveränderlichen Run-Identität passen; Speichern, Export und 3D-Inbox verwenden die ursprüngliche Archivdatei. Ältere Replays ohne Eingaben bleiben exportierbar. Lokale Hashes belegen Konsistenz, keine vertrauenswürdige Herkunft oder Manipulationsfreiheit.

## Oberfläche und 3D-Steuerung

Die echte ImGui-Oberfläche führt weiterhin 144 Tab-/Viewport-/DPI-/Verbindungsfälle, 216 HUD-Konfigurationen, 128 Overlay-Regeln und 288 Minimap-Fälle aus. Neue Prüfungen bedienen die tatsächlichen Frame-Schritte und die Ereignisliste, prüfen 480-/1920-px-Viewports bei 100/150 % DPI, Befehlszuordnung zur Run-ID und Vorrang von Stop 3D. Der native Replay-Tab liest den produktiven Eingabearbeiter; seine Eingabeanzeige und der eindeutig als REPLAY markierte passive 3D-Status werden gerendert. Testbilder: `tests/previews-7.2.0` mit synthetischen Daten und Host-Ersatzschrift. Sie zeigen keine Unity-3D-Liveaufnahme.

Verwaltete Tests prüfen die Zuordnung zum originalen Spawn-/Kristalllayout mit dem vorhandenen Positionsvergleich sowie die Gameplay-/Pickup-Sperren während der 3D-Reservierung. Der LIFO-Rollback wird mit einem simulierten Fehler geprüft: alle übrigen Rücksetzungen werden trotzdem genau einmal ausgeführt. Der Windows-x64-Trainer und die Helper-DLL sind mit strengen Warnungen gegen die tatsächlichen Game-/Unity-/Loader-Signaturen gebaut.

Die 3D-Vorschau verwendet Original-Meshreferenzen mit erhaltenen Transform-Hierarchien, gemeinsame einfache Materialien und höchstens Grundlicht plus aufgenommenes Licht. Keine Originaltexturen, Schatten, HDR, MSAA oder Nebel. Geometrieaufbau erfolgt schrittweise (höchstens 64 bearbeitete Mesh-Einträge und ein überprüftes Zielbudget von 2 ms pro Tick; einzelne Unity-Aufrufe können dieses Budget überschreiten). Zusätzliche Live-Map-/Routenscans pausieren währenddessen. Keine Spielskripte oder Collider werden in die Darstellung kopiert. Originale Kameras, Lichter, Nebel, Spielersteuerung, Audiopause und Zeitskalierung werden zur Rücksetzung registriert; Stop, Szenenwechsel, Trennung und Fehler führen den Rollback aus.

**3D bleibt WIP und benötigt einen Windows-/Unity-Livetest.** Auf diesem Host sind native Unity-Szenen-/Renderer-/Input-Aufrufe und D3D-Ausgabe nicht ausführbar. Shader-Verfügbarkeit, Sichtbarkeit aller Level-Meshes, tatsächliche Lade-/Frametimes und die vollständige In-Game-Wiederherstellung konnten daher nicht live bestätigt werden. Monster sind zeitlich interpolierte Positionsmarker aus der bestehenden Aufnahme, keine nachgebildeten Animationen. Radarbildschirm, Audio, Postprocessing und vollständige dynamische Weltzustände fehlen. Es wird keine deterministische Input-Neusimulation behauptet.

## Regressionen und erhaltene Daten

Native Daten-/Routing-/Such-/Archivprüfungen und die Adapter verwenden AddressSanitizer/UndefinedBehaviorSanitizer, die GUI führt das produktive ImGui aus. Die verwalteten Tests verwenden Mono mit passenden Game-Referenzen; bekannte Hinweise zu nicht ausführbaren Unity-Internals werden dokumentiert und sind keine Spielausführung.

Elf zentrale Geometrie-/Routing-/Flash-/Movement-/Validierungsdateien sind bytegenau zur gesicherten 7.1.0-Basis erhalten (`tests/results-7.2.0/preserved.json`). Vorhandene Seed-Rangfolge, Bewegungslernen und lokale Gültigkeitsprädikate bleiben erhalten. Einstellungen bleiben Format 15. Hauptprotokoll 17 erfordert ein gemeinsames DLL-/EXE-Update; alte Replay-Dateien bleiben lesbar. Hide & Seek / Outside bleiben für Minimap und Seed-Suche WIP. Die 7.1.0-Downloadarchive bleiben unverändert.
