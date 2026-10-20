> Historischer Stand 7.3.4: Dieser Mesh-/Readback-Ansatz wurde nach fehlgeschlagenem Ingame-Test auf Nutzerwunsch in 7.3.5 zurückgenommen. Die damaligen Host-Prüfungen belegen keine funktionierende Ingame-Darstellung.

# Replay-Daten 7.3.4 / mab

## Dateien und Identität

| Datei | Inhalt |
| --- | --- |
| `run-<ID>.isrun` | Bestehender Map-/Run-Payload 12, einschließlich Eingabe-Metadaten |
| `run-<ID>.isinput` | Separater Eingabe-/Kamera-/Physik-/Colliderstrom, neue Dateien Format 2; Format 1 bleibt lesbar |
| `run-<ID>.validation.json` | Am Run-Ende eingefrorener lokaler Prüfbericht |
| `run-<ID>.meta` | Optional: Name, Kategorie, Notizen; vorhandenes Format 1 |
| `run-<ID>.ismarks` | Editierbare benannte Zeitmarkierungen und A–B-Bereich, Format 1 |

Die .isrun-Datei wird zuletzt veröffentlicht. Ein unvollständig gespeicherter Satz wird damit nicht als neuer Archiv-Eintrag aufgelistet. Im Eingabestrom werden Run-ID, Szene, Layoutseed, Dauer, Game-Hash, Aufnahmeversionen und Anzahlen wiederholt; alle müssen zur ausgewählten Aufnahme passen. Originaldateien werden nicht überschrieben. Aktuelle PID-/Live-Transportinformationen sind keine Run-Identität.

Der Layoutseed bezeichnet die erfasste Spawn-/Kristallauswahl. Der erste beobachtete RNG-Seed im Validierungsbericht ist kein vollständiger Welt-/KI-Zustand. 3D verwendet beobachtete Bewegung und prüft tatsächliche Spawn-/Kristallpositionen nach dem Laden. Eine identische Input-Neusimulation wird nicht vorausgesetzt.

## Eingabestrom

Little Endian. Header: uint32 Magic `0x4e495349`, int32 Format 1 oder 2, int64 Run-ID, int32 Szene, int32 Layoutseed, double Dauer, UTF-8-Game-Hash und dieselben nullable Eingabe-Metadaten wie im Map-Payload. UTF-8-Texte tragen ein int32-Byte-Längenpräfix. Metadaten enthalten int32 Frames/Ticks/Events/Verworfen, uint32 Qualitätsflags und Tool-/Game-/Unity-Versionen.

Danach folgen drei Blöcke, jeweils int32 Anzahl und feste Datensätze. In Format 2 folgt außerdem der unten beschriebene Colliderblock. Ein uint64-FNV-1a-Checksum über alle vorherigen Bytes schließt die Datei ab. Der Paketexport ergänzt SHA-256 über vollständige Dateien.

| Kamerasample, in Schreibreihenfolge | Bytes |
| --- | ---: |
| Sekunden (double) | 8 |
| Frame, Physiktick, Zustand, Tasten, gelesene Achsen (5 × uint32) | 20 |
| Horizontal, Vertical, Mouse X/Y, Joystick X/Y (6 × float) | 24 |
| Spielerposition und Quaternion | 28 |
| Kameraposition, Quaternion und FOV | 32 |
| Lichtquaternion | 16 |
| Lichtposition und RGB | 24 |
| Lichtintensität, Reichweite, Winkel (3 × float), Lichttyp (uint32) | 16 |
| **Gesamt** | **168** |

Physiksample: double Sekunden; uint32 Frame/Tick/Zustand; float Horizontal/Vertical; Vector3 Position; Quaternion Rotation — **56 Bytes**.

Eingabeereignis: double Sekunden; uint32 Frame/Tick/Tasten/Down/Up/Aktionen/Zustand — **36 Bytes**. Das erste Ereignis enthält den anfänglichen Tastenzustand mit Down=Up=0. Danach entsprechen Down/Up den erfassten Zustandswechseln. Eine Radar-Anforderung kann ohne aktive Radaransicht auftreten.

Tastenbits 0–6: W, A, S, D, linke Shift-Taste, rechte Shift-Taste, R. Zustandsbits 0–8: Spiel fokussiert, Trainer besitzt Eingaben, tatsächlicher Sprint, Radar aktiv, Licht tatsächlich aktiv, Kameraflash aktiv, Controller am Boden, canMove > 0, Freecam aktiv. Achsenbits 0–5: Horizontal, Vertical, Mouse X/Y, Joystick X/Y. Aktionsbits 0/1: vom Spiel gelesener R-/Controller-Radar-Impuls.

Kameras werden unmittelbar vor ihrem Rendering beobachtet; Anfang/Ende ergänzen Zustandsproben. Bei einer deaktivierten Hauptkamera wird ihre Pose in LateUpdate beobachtet. Aufgezeichnet wird die Gameplay-Kamera; Freecam-Nutzung bleibt gekennzeichnet. Eingabeereignisse folgen MouseLook.Update; Physik folgt dem originalen FixedUpdate. Wrapper rufen die ersetzten Unity-Eingabefunktionen jeweils genau einmal auf und geben denselben Wert zurück.

## Grenzen und Wiedergabe

- Maximal 262144 Kamerasamples, 524288 Physiksamples und 131072 Ereignisse; Dateigrenze 128 MiB. Die Hintergrundwarteschlange für neue Replay-Payloads ist auf geschätzte 192 MiB begrenzt. Ein überschrittener Aufnahmepuffer verwirft weitere Proben ausdrücklich, ohne alte Eingaben umzuschreiben.
- Qualitätsflags: Bit 0 Speichergrenze, Bit 1 Beobachtungsfehler/ungültige Probe, Bit 2 Kameralücke über 250 ms. Verworfen zählt Kapazitätsüberschreitungen. Andere Beobachtungslücken werden durch Flags und tatsächliche Zeitabstände erkennbar.
- Zeiten und Sequenzen müssen monoton sein; negative/unendliche Werte, ungültige Rotationen, unmögliche Anzahlen und beschädigte Prüfsummen werden abgewiesen. Kameralücken über 250 ms sowie große Positionssprünge werden nicht glatt überbrückt. Eingaben bleiben diskrete Beobachtungen.
- Monster stammen aus dem bestehenden 10-Hz-/Keyframe-Recorder; explizit zusammenhängende Strecken werden interpoliert. In 3D bleiben sie Marker. Die originale Level-Rendergeometrie wird direkt verwendet, einschließlich Renderer-Batch-Zuordnung und Submesh-Bereichen. Vorübergehende gemeinsame Analyse-Materialien, getrennte Material-Property-Blocks, Lightmap-/Probe- und Schattenzustände werden rücksetzbar verwaltet. Keine Mesh-Kopien und keine per Frame erzeugte Level-Geometrie. Die Gameplay-Koroutinen behalten ihre Iteratorzustände, während sie angehalten sind. Nur die tatsächlichen Wand-/Objekt-/Deckenlicht-Culling-Kataloge werden wieder eingeblendet; inaktive Varianten und gesammelte Kristalle bleiben unberührt. Eine schattenfreie Richtungsbeleuchtung mit konstantem Umgebungslicht liefert Tiefe; ältere/fallbackfähige Hardware kann einfache unbeleuchtete Materialien verwenden. Aufgezeichnete Lichtdaten und 2D-Pulse bleiben erhalten.
- Ab 7.2.1 folgt 3D einer monotonen Uhr mit Ankern bei Start, Seek, Pause und Geschwindigkeitswechsel. Ladezeit zählt nicht zur Wiedergabe. Kamerapose wird in jedem LateUpdate angewendet, ohne Bindung an die 60-Hz-Telemetrie. Welt-Updates, Spielkoroutinen und skalierte Spielzeit pausieren; alle vorübergehenden Zustände werden am Ende zurückgesetzt.
- 3D ist eine Vorschau, kein Video: keine vollständige Animation, kein Ton, keine Radartextur, kein Postprocessing und keine vollständige Weltgeschichte. Die lokale Prüflogik wird durch das Replay nicht erweitert oder als fälschungssicher dargestellt.

## ZIP-Paket

Standard-ZIP mit gespeicherten Einträgen ohne zusätzliche Kompression, CRC32 und strenger lokaler/zentraler Directory-Prüfung. Maximal acht Einträge; nur bekannte Dateinamen für dieselbe Run-ID. Pfadbestandteile, doppelte Namen, unerwartete Dateien, zu große Dateien und unbestätigte Checksum-Zeilen werden abgewiesen. `manifest.json` hält die Run-Metadaten fest, `SHA256SUMS.txt` deckt alle übrigen Einträge ab.

Import ist erst nach vollständiger Prüfung erlaubt; die .isrun-Datei wird zuletzt geschrieben. Notizen werden separat validiert. Ohne Eingabedaten bleiben ältere Aufnahmen exportierbar und als bisherige 2D-Replays verwendbar. Ein fehlender optionaler älterer Prüfbericht wird im Manifest als fehlend ausgewiesen. Der optionale Ereignisverlauf des Prüfberichts folgt weiterhin der Reports-Einstellung.

Die Hashes sind unsignierte Konsistenznachweise. Sie bestätigen weder die Herkunft noch physische Tastendrücke. Änderungen am Paketmanifest oder an Datensatzlayouts benötigen eine neue Formatversion.


## Colliderblock in Eingabeformat 2

Nach den unveränderten drei Eingabeblöcken: uint8 Recorded; uint32 Flags; int32 Dropped, TotalSamples, TrackCount (**17 Bytes**). Höchstens 256 Tracks und zusammen 262144 Samples. Flags: 1 Kapazitätsgrenze, 2 Beobachtungsfehler, 4 nicht unterstützte Form. Diese Qualitätsangaben sind von den unveränderten Eingabe-Metadaten im Map-Payload getrennt.

Je Track: int32 Gruppe (1 Spieler, 2 Monster, 3 Kristall), Besitzer (0 Spieler; Monster-ID bzw. Map-Punktindex), Form (1 Box, 2 Kugel, 3 Kapsel, 4 CharacterController), UTF-8-Name bis 128 Byte und int32 Sampleanzahl.

Je Sample: double Sekunden, uint32 Zustand (1 tatsächlich enabled/active, 2 Trigger), Vector3 Center, Vector3 Size, float Radius/Height, int32 Direction, zwölf float Matrixwerte in drei Zeilen à vier Spalten (**96 Bytes**). Die implizite letzte Matrixzeile ist (0,0,0,1). Boxen verwenden lokale Maße und die ursprüngliche lokale-zu-Welt-Matrix. Kugeln verwenden Weltmittelpunkt und mit Unity-Skalierung beobachteten Weltradius. Kapseln/Controller verwenden Weltmittelpunkt, Rotationsmatrix und die tatsächlichen skalierten Höhen-/Radiusregeln. Maßänderungen werden pro Sample festgehalten.

Quellen stammen aus den bestehenden Spieler-, Kristall- und Monster-Katalogen; es gibt keinen zusätzlichen Szenenscan in den Aufnahmecallbacks. Spieler werden bei Physikbeobachtungen erfasst. Andere Quellen werden rundlaufend mit maximal 16 Beobachtungen je Kamera-Aufruf und höchstens 10 Hz pro Quelle erfasst. Unveränderte Werte erhalten spätestens bei der nächsten vorgesehenen Beobachtung nach 0,5 s eine erneute Probe. Anfang/Ende werden ergänzt. Dateien werden weiter ausschließlich im Hintergrund geschrieben. Veraltete Beobachtungen über 0,6 s werden in der Anzeige ausgeblendet. Collider-Zwischenzustände werden nicht interpoliert oder erfunden.

Level-Collider werden in der Analyse einmal gesammelt und getrennt gekennzeichnet. Sie sind keine historische Weltzustandsaufnahme. Gezeichnet werden höchstens 256 nahe unterstützte Level-Primitive und die nahen aufgezeichneten Formen; die Geschwindigkeitsspur ist auf 360 Abschnitte begrenzt.

## Eigene Markierungen

`.ismarks`: Little Endian, uint32 Magic 0x4b4d5349, int32 Format 1, int64 Run-ID, double tatsächliche Dauer/A/B, uint32 Flags (A gesetzt, B gesetzt, Loop aktiv), int32 Markeranzahl. Je Marker uint32 eindeutige ID, double Sekunden, UTF-8-Name mit int32 Byte-Länge; abschließend FNV-1a uint64. Höchstens 256 Marker, Namen bis 128 UTF-8-Byte, Dateigrenze 64 KiB. Steuerzeichen, doppelte IDs, Zeiten außerhalb der Aufnahme und aktive ungültige Loops werden abgewiesen.

Diese Datei ist editierbar, mit atomarer Speicherung und Backup; `.isrun`, `.isinput` und ursprüngliche Prüfung bleiben unverändert. Eigene Marker werden orange und ausdrücklich als Annotationen dargestellt. Loop-Bereiche brauchen mindestens 0,02 s Abstand. 2D und 3D teilen dieselben Annotationen; 3D-Schleifen laufen auf der monotonen Helper-Uhr.

## Kompatibilität 7.3.4

Hauptprotokoll **19**, Commands **192 Byte**, Telemetrie **16516 Byte**; DLL und EXE zusammen ersetzen. Map-Payload 12 und Settings 15 bleiben erhalten. Eingabeformat 1 bleibt vollständig lesbar; neue Aufnahmen verwenden 2. Alte Aufnahmebytes und Aufnahmeversionswerte werden bei Laden/Export nicht umgeschrieben. Für frühere Replays werden fehlende Collider ausdrücklich gemeldet.

Pakete ohne Annotationen behalten Format 1; Importe der Exporter 7.2.0, 7.2.1, 7.3.0, 7.3.1, 7.3.2, 7.3.3 und 7.3.4 sind unterstützt. Pakete mit `.ismarks` verwenden **Paketformat 2** und `annotationsIncluded: true`; alle Dateien bleiben in der SHA-256-Liste. Die maximale Anzahl beträgt weiterhin acht Einträge. Dateien für die 3D-Inbox werden unverändert aus der geprüften Originalaufnahme kopiert; GUI-Notizen werden nicht als Spielereignisse eingespielt.

Steueroptionen im Protokoll: 1 Play, 2 freie Kamera, 4 Verfolgerkamera, 8/16/32 Spieler-/Monster-/Kristall-Collider, 64 Level-Collider, 128 Geschwindigkeitsspur, 256 Loop, 512 Collider durch Wände, 2048 Decke ausblenden, 4096 verdeckende Wände transparent. Die persistente Optionsmaske ist 7167 (1024 bleibt ausgeschlossen). 1024 ist ein reiner Seek-Befehlsimpuls; er wird nicht als Anzeigeoption gespeichert. Rate-/Kamera-/Darstellungsänderungen ohne diesen Impuls übernehmen die aktuelle monotone Spielposition, wodurch wiederholte Regleränderungen die Wiedergabe nicht abbremsen.

## Darstellungsänderungen 7.3.1

Kein Wire-/Dateiformatwechsel: Hauptprotokoll 18, Eingabeformat 2 (Leser akzeptiert 1/2), Map-Payload 12, Map-Kanal 9 und Settings 15 bleiben erhalten. Annotierte Paketformat-2-Exporte von 7.3.0 werden einschließlich Originaleingaben unverändert importiert. Alte Originalaufnahmen werden nicht umgeschrieben.

Colliderquellen erfassen zusätzlich den Spieler-Komponentenunterbaum und einen gegebenenfalls separaten Agentenkörper des Monsters; Instanz-IDs verhindern doppelte Tracks. Sichtbarkeit wird weiterhin aus den aufgenommenen enabled/active-Zuständen bestimmt. UI-Zähler trennen Spieler/Monster/Kristalle; fehlende Gruppen werden ausdrücklich nicht als verfügbare Daten angeboten.

Der Analyse-Overlay prüft unterstützte Shader in der Reihenfolge Hidden/Internal-Colored, Sprites/Default, UI/Default, Unlit/Color. Die ersten drei lesen Vertexfarben für weiche Farbverläufe. Der solide Unlit-Fallback zeichnet je Gruppe die tatsächliche Materialfarbe und die Spur gebündelt in höchstens 32 Farbbändern; er bietet keine Alpha-Fades. Der verwendete Pfad und die Track-/Sampleanzahlen stehen im Replay3D-Log. Durch-Wand-Ansicht löscht nach fertiger Szenenzeichnung nur die Tiefe der separaten Replay-Kamera, ohne die Szene erneut zu zeichnen. Der nächste Kameraframe beginnt normal. Bei abgeschalteter Option bleibt die Szenentiefe erhalten.

Die Spur verwendet höchstens 360 Kamera-Bandabschnitte mit je zwei Dreiecken für Kontur und Farbkern (höchstens 4320 eingereichte Vertices, im soliden Unlit-Fallback durch beide Windungen höchstens 8640). Vektoren/Abschnitte sind vorab reserviert. Breite wird aus Kamera-FOV, Tiefe und Viewport berechnet und auf 0,02–0,18 u Halbstärke begrenzt. Gemessene 0–10 u/s erhalten einen kontinuierlichen Rot/Amber/Türkis-Verlauf; höhere Werte werden nur farblich bei Türkis geklemmt, nicht im Messwert.

Der Verfolger-Orbit speichert Yaw/Pitch nur für die Replay-Sitzung und folgt der Spielerposition. Seine anfängliche Richtung und explizites Backspace-Ausrichten stammen aus der aufgezeichneten Kamera. Menü/Fokus-Grenzen schützen Maussteuerung wie bei der vorhandenen freien Kamera. Seek, Pause und Loops erhalten den Winkel; das Sitzungsende setzt ihn zurück. Keine Kamera-Kollisionsabfragen, keine Änderungen aufgezeichneter Posen oder Spieltransforms.

## Darstellungsänderungen 7.3.2 / Hauptprotokoll 19 (historisch; Wandzuordnung in 7.3.4 ersetzt)

Commands bleiben 192 Byte, Telemetrie 16516 Byte; der benannte Hauptkanal wechselt auf v19 und verhindert gemischte DLL-/EXE-Versionen. Datei- und Paketformate bleiben gleich. Annotierte 7.3.0-/7.3.1-Pakete werden mit unveränderten Originalbytes importiert.

Die Verfolgerkamera speichert Radius/Yaw/Pitch lokal für die Sitzung. Ein Mausradschritt multipliziert den Radius mit exp(-0,12 * Schritt), begrenzt auf 0,75–20 u; Pitch bleibt in -80 bis +80 Grad. Mouse X/Y bei rechter Maustaste drehen direkt; Fokus-/Menüschutz bleibt erhalten. Replay3dFollowDistance in der Telemetrie ist nun der tatsächliche Radius. Replay3dFollowHeight bleibt als reserviertes 0-Feld erhalten; die beiden gleichnamigen Befehlsfelder steuern die Kamera nicht mehr. Rate-/Sicht-/Seek-Befehle setzen den lokalen Zoom nicht zurück.

Sichtoptionen verwenden die bestehenden Oberflächenrollen der Analysepalette. Jeder Renderer behält seine Materialslotzahl und originalen Batch-Bereiche. Für Alpha werden 20 gemeinsam verwendete transparente Materialvarianten und ein unsichtbares Material einmal erstellt; Standard-Fade bzw. unterstützte Alpha-Shader werden verwendet. Nur Wand-/Tür-Slots erhalten einen eigenen interpolierten Alpha-Property-Block, Zielwert 0,18; Decken-Slots werden getrennt ausblendbar. Verlassen/Fehler räumt diese Auswahl vor der vollständigen Original-Rücksetzung auf.

Verdeckungsprüfung: höchstens 20 Hz, je ein RaycastNonAlloc in beide Richtungen mit je 64 Trefferplätzen. Nur bestehende solide Collider mit passender Renderer-Hierarchie und Trefferpunkt im Rendererbereich werden zugeordnet. Der Rückweg berücksichtigt eine Kamera innerhalb eines Colliders. Trigger, Spieler, Monster und Kristalle sind keine Wandquellen. Keine Wand-Bounding-Box als Ersatz für einen fehlenden Physiktreffer. Bei zusammengesetzten Renderern erfolgt Transparenz auf der vorhandenen Materialabschnitt-/Rendererebene; Türenzustände bleiben wie bisher Teil der geladenen Szene, keine aufgezeichnete historische Weltrekonstruktion.

Die Spieler-Wire-Geometrie verwendet unveränderte Box-/Kugel-/Kapsel-/Controller-Zentren und Maße. Striche entstehen als kameraseitige Dreiecke mit Nahgrenzen-Clipping und Pixelbreite 2,5/4,5 für Kern/Kontur, skaliert mit der Ansicht. Kameradaten werden je Renderpass gecacht; maximal 4096 Liniensegmente je Spielerpass. Der feste Cull-Modus von Unlit/Color wird durch beide Dreieckswindungen berücksichtigt; interne/Vertexfarben-Shader benötigen keine Verdopplung. Andere Collider bleiben schmale Linien; die Geschwindigkeitsspur behält ihre 360-Abschnitt-Grenze.

## Darstellungsänderungen 7.3.3 (historisch; in 7.3.4 ersetzt)

Wand-Renderer werden vor dem Palettenwechsel über Walls-Layer, Wall-Tags, Objekt- und Materialnamen zugeordnet. Bestätigte Hauptwand-Grafik und Hauptwand-Collider dürfen getrennte Hierarchien haben. Bounds-Überschneidung allein erzeugt keinen Fade: ein tatsächlicher Hin-/Rückstrahl-Treffer innerhalb des Renderers bleibt erforderlich. Breite, bestätigte Wände erhalten Vorrang vor dem bisherigen horizontalen Bounds-Fallback; explizite Boden-/Decken-/Tür-Materialrollen bleiben bestehen. Die Originalszene wird weiter vollständig zurückgesetzt. Hauptprotokoll 19 und alle Dateiformate unverändert; auch annotierte 7.3.2-Pakete bleiben lesbar.

## Darstellungsänderungen 7.3.4

Die bisherige Collider-/Tag-/Materialzuordnung für Wandtransparenz ist ersetzt: Sichtbarkeit wird gegen die eigenen Mesh-Dreiecke des Renderers/Materialslots geprüft. CPU-unlesbare Meshes werden einmalig über GPU-Puffer gelesen; Float32/Float16-Positionen, Stream/Stride/Offset, UInt16/UInt32-Indizes sowie baseVertex werden berücksichtigt. Statische Batches verwenden subMeshStartIndex und Renderer.localToWorldMatrix. Gemischte horizontale/Wand-Flächen werden in eigene, nur für das Replay existierende Abschnitte getrennt. Original-Batchdaten bleiben erhalten. Eine vorbereitete räumliche Baumstruktur begrenzt Sichtprüfungen auf maximal 20 Hz, ohne Szenensuchen oder GPU-Rücklesen im laufenden Replay. Hauptprotokoll 19 und alle Aufnahme-/Paketformate unverändert; annotierte 7.3.3-Pakete weiter importierbar.

Boden-/Deckenstücke aus gemischten Renderern werden nach den vier bestehenden Farbvarianten gebündelt: höchstens acht gemeinsame horizontale Zeichenabschnitte pro Szene. Wandabschnitte behalten unabhängige Fade-Steuerung. Bestätigte Props werden ohne unnötigen Mesh-Readback direkt geschützt.
