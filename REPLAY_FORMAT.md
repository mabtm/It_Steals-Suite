# Ergänzung 7.11.0 / mab: Live Inspector

Das Hauptprotokoll verwendet jetzt 27; Commands (192 Byte), Telemetrie (16952 Byte), Map-/Run-Payload 13, Kartenkanal 10, Input 6, Frame 4, Audio 1 und Clip-Projekt 7 behalten ihre Form. Die schreibgeschützten Inspector-Daten nutzen einen getrennten Kanal 1, beschrieben in INSPECTOR_FORMAT.md. RunAids erhält LiveInspector=512 im bestehenden UInt32-Feld. Die vorherigen Replay-/Clip-Formate sind weiterhin lesbar; 7.10.1-Manifeste werden ausdrücklich akzeptiert.

# Ergänzung 7.10.0 / mab: physische Karten- und Pickup-Daten

Neue Map-/Run-Payloads verwenden **13**, Kartenkanal **10**, Hauptprotokoll **26**. Eingabeformat 6, Frame-Kanal 4, Audio 1 und Clip-Projekt 7 bleiben unverändert. Die folgenden historischen Abschnitte bleiben relevant für alte Dateien.

Nach den v12-Motion-Metadaten und vor der bisherigen FNV64-Prüfsumme stehen:

- Bool `PhysicalCaptured`, Int32 `PhysicalOmitted`, Int32 Teilezahl (0–20000).
- Je Teil UInt32 Flags (1 = Wanddarstellung, 2 = statisches/kinematisches Spielerhindernis, 4 = konservativ tessellierte Kurve), Int32 Punktezahl (2–64), danach Float32 x/y/z pro Punkt. Gesamt höchstens 60000 Punkte. 2D-X/Z-Konturen sind konvex und gegen den Uhrzeigersinn in X/Z orientiert, y ist kein vertikales Hindernisprofil. Fehlende Teile werden gezählt, nicht als geschlossene AABB ersetzt.
- Int32 Pickup-Zahl (0 oder genau Point-Kataloggröße), danach je Bool `Verified`, Float32 Center x/y/z und Float32 Radius (0–10, bei Verified >0). Ein verifizierter Radius ist ein konservativer stehender/waagerechter Kamera-Kernbereich des echten Pickup-Triggers einschließlich Sphere-Trigger und Sicherheitsrand. Er ist nicht der Wandkollisionsradius und keine Aussage über beliebige Kamera-/Crouch-Zustände.

Reader prüfen Flags, endliche Werte, Größen und konvexe Konturen; die äußere Prüfsumme umfasst alle neuen Felder. Cacheidentität umfasst auch diese Daten. Ältere Map-/Run-Payloads bleiben lesbar und behalten die Mittelpunktberechnung. Paketimporte akzeptieren auch den bisherigen 7.9.0-Manifeststand. Gültigkeits-/Eingabe- und Aufnahmequalitätsdaten werden nicht verändert.

# Replayformat – aktueller Stand 7.9.0 / mab

## Eingabeformat 6 und Frame-Kanal 4 (7.9.0)

Eingabeformat 6 enthält unverändert alle Format-5-Blöcke. Direkt vor der abschließenden FNV64 folgen byte CoverageRecorded (0/1), int32 Kanalzahl (genau 6), dann je Kanal:

| Feld in Schreibreihenfolge | Typ |
| --- | --- |
| Started | byte, 0/1 |
| Observations / Errors / Sources | 3 × int32, nicht negativ; Sources höchstens 8192 |
| First / Through / MaxGap | 3 × double, endlich und innerhalb der Run-Dauer |
| GapCount / StoredGapCount | 2 × int32; StoredGapCount höchstens 16 und höchstens GapCount |
| Gespeicherte Lücken | je double Begin / End, streng positive Länge, chronologisch ohne Überlappung |

Kanäle: 0 Movement, 1 Camera, 2 Inputs, 3 Colliders, 4 Visuals, 5 Animator/Blendshape-Metadaten. Ein erfolgreicher Poll verlängert die Beobachtung auch ohne gespeicherte Wertänderung. Tolerierte Beobachtungsabstände in Sekunden: 0,12 / 0,25 / 0,25 / 0,12 / 0,12 / 0,25. Through ist die echte letzte erfolgreiche Beobachtung, kein künstlicher Endpunkt. Abschluss ergänzt eine gegebenenfalls fehlende Endspanne. Audio behält seine eigenen verifizierten PCM-/Gap-/Routingdaten. Formate 1–5 haben keine solchen Heartbeats; Vollständigkeit bleibt dann Unknown. Strukturelle Inkonsistenzen, ungültige Flags und Zeitbereiche werden abgewiesen.

Die Grafik-Frames selbst behalten das bestehende Format. Transformposen werden bis 60/120 Hz beobachtet und mit maximal 16 Zwischenproben gegen Positions-/Skalierungs-/Quaternion-Interpolation verdichtet. Toleranzen je beobachtetem Transform 0,00075 u / 0,0001 / 0,15 Grad. Konstante Werte benötigen keine periodischen Kopien; nicht konstante Abschnitte sind höchstens 0,1 s lang. Grenzen gelten für tatsächliche Beobachtungen, nicht für unbekannte Zwischenzustände.

Frame-Kanal: `Local\ItSteals.Clip.v4.<PID>`, Mutex gleicher Name + `.Lock`. Global 1024 Byte, danach drei unabhängige Slots à 16 MiB. Globale Felder: uint32 Magic 0x504c4349, int32 Version=4, int32 Slots=3, int32 SlotBytes=16777216. Slotbasis: 1024 + Index × SlotBytes. Alle folgenden Offsets gelten relativ zur Slotbasis:

| Offset | Inhalt |
| ---: | --- |
| 8 / 12 / 16 | uint32 RequestSerial / AckSerial, int32 Status (0 pending, 1 fertig, 2 Fehler, -1 abgebrochen/idle) |
| 20 / 24 | int32 Width / Height |
| 32 / 40 / 48 / 52 | double SourceSeconds, int64 RunId, uint32 Options, int32 CustomPoseFlag |
| 56 / 96 | Request-/Antwortpose: float3 Position, float4 Quaternion, float Fov |
| 136 / 140 | float PreviewAudioSpeed / Volume |
| 144 | int64 OutputFrameIndex |
| 152 / 156 / 160 / 164 | int32 ReadbackMode (0 synchron, 1 asynchron); float RenderMs / ReadbackLatencyMs / TransferMs |
| 256 | Nullterminierter UTF-8-Fehler, höchstens 510 Byte |
| 1024 | Width × Height × 4 top-down BGRA-Bytes |

Mutex schützt Anfrage und Veröffentlichung. Pixel/Pose werden vor Ack geschrieben. Jeder Slot behält sein Renderziel bis zur GPU-Fertigstellung; danach werden native GPU-Daten sofort kopiert, selbst wenn die Veröffentlichung noch warten muss. Rückgaben werden an Serial, Run, Frame-Index, Zeit und Größe gebunden. Ein gemeinsames Drei-Frame-Creditfenster über beide GPU-Auslesestufen verhindert mit der Clipdauer wachsende Speicherbelegung. Bilder werden strikt nach OutputFrameIndex an den Encoder übergeben; die Quellzeit kommt aus der unveränderten Tempoabbildung. Keine Frames werden wegen Echtzeit-Verzögerungen übersprungen.

Hauptprotokoll 25 verhindert gemischte Trainer-/Helper-Versionen. ISCLIP 7 / Audioformat 1 / Map-Kanal 9 / Map-Payload 12 bleiben unverändert. Alte Replay-Pakete inklusive 7.8.1 werden weiter unverändert gelesen und kopiert.

## Erweiterung in Eingabeformat 5

Bis einschließlich Grafikblock bleibt das Format-4-Layout erhalten. Vor der abschließenden Gesamt-FNV64 folgen:

- byte ExperienceRecorded, int32 Dropped;
- AudioInfo: byte Recorded, int32 Rate/Channels/Flags/Gaps, int64 Frames/Bytes, uint64 FNV64;
- int32 NameCount und UTF-8-Namen mit int32 Byte-Länge;
- int32 ProbeCount, je double Seconds, int32 Actor/Site/Mask, uint32 Flags, float3 Origin/End, float Limit;
- int32 EventCount, je double Seconds, int32 Actor/Kind/Name, float Value, float3 Position;
- int32 AnimationTrackCount, je int32 Group/Actor/Layer/Kind/Slot, UTF-8 Path/Name, int32 SampleCount, je double Seconds, int32 State/Next/Clip, uint32 Flags, float Phase/NextPhase/Transition/Speed/Weight.

Name-Referenzen beginnen bei 1; Clip=0 ist erlaubt. Event-Kind 1: boolescher Zustandswechsel, 2: Audio-Play/OneShot, 3: Stop/Pause, 4: erster beobachteter boolescher Zustand. Animator-Kind 1: Metadaten; Kind 2: tatsächliches Blendshape-Gewicht. Animationsflag 1: Übergang; 2: harter Wechsel. Keine erneute Controller-Simulation.

Probe-Flags: 1 ursprüngliche Abfrage traf etwas, 2 Linecast, 4 dargestellte Länge gekappt, 8 bestätigter bekannter Ziel-Transform. Unbegrenzte Distanz wird als -1 gespeichert. No-hit-Rays werden auf höchstens 64 Einheiten für die Darstellung begrenzt. Linecasts ohne out-Hit speichern das angefragte Endziel, keinen erfundenen Kollisionspunkt. Geometrie ist bis 10 Hz abgetastet; Zustandswechsel separat. Keine zusätzliche Physik-/RNG-Ausführung.

Grenzen: 2048 Namen à 512 UTF-8-Byte, 131072 Queries, 65536 Events, 512 Animationstracks, 65536 Animationssamples, insgesamt höchstens 16 MiB neue Metadaten. Monotone Zeiten innerhalb der Run-Dauer, endliche Zahlen, eindeutige Tracks/Namen, bekannte Flags und passende Referenzen werden geprüft. Fehlender Format-5-Block in Formaten 1–4 bedeutet keine nachträglich erzeugten Daten. Basis-Inputbudget 56 MiB; Grafik-/Colliderlimits unverändert.

## 7.8.1: unveränderte Formate

Zusätzliche zentrierte 2D-Listener-Bypass-Quellen werden vor der abschließenden Prüfsumme auf die tatsächlichen PCM-Samples addiert. Keine zusätzlichen Audiodateien oder Event-Synthese. Überschneidungen nach DSP-Uhrkorrektur werden pro Quelle entdoppelt. Nicht zuverlässig beobachtbare Routen setzen Audioflag 4 auch bei ansonsten vorhandener Aufnahme. Paketimporte akzeptieren weiterhin 7.8.0 einschließlich Marker.

## 7.8.1: unveränderte Formate

Zusätzliche zentrierte 2D-Listener-Bypass-Quellen werden vor der abschließenden Prüfsumme auf die tatsächlichen PCM-Samples addiert. Keine zusätzlichen Audiodateien oder Event-Synthese. Überschneidungen nach DSP-Uhrkorrektur werden pro Quelle entdoppelt. Nicht zuverlässig beobachtbare Routen setzen Audioflag 4 auch bei ansonsten vorhandener Aufnahme. Paketimporte akzeptieren weiterhin 7.8.0 einschließlich Marker.

## Audiobegleitdatei `.isaudio`

Little Endian, 256-Byte-Header: uint32 Magic 0x55415349, int32 Version 1, int64 Run-ID, int32 Scene/Seed/Rate/Channels, int64 Frames, int32 Flags/Gaps, int32 Gamehash-Byteanzahl und UTF-8-Hash (maximal 65 Byte), Nullpadding bis 256. Danach interleaved Float32-PCM; zuletzt uint64 FNV-1a über sämtliche vorherigen Bytes. Höchstens 256 MiB. Flags: 1 Lücke/Überlauf, 2 Dateilimit, 4 nicht verfügbar/keine Samples oder unvollständig beobachtbare Audio-Route oder unvollständig beobachtbare Audio-Route, 8 Kanal-/Zahlenfehler, 16 IO-Fehler. Nicht aufgenommene AudioInfo hat Frames/Bytes=0. Aufgenommene Dauer darf Run-Dauer nur um Sample-Rundung überschreiten.

Die `.isinput` bindet alle AudioInfo-Werte und die Audioprüfsumme. Prüfung läuft auf Hintergrundthreads vor Veröffentlichung oder Export. Eine `.isrun` wird erst nach ihren Begleitdateien veröffentlicht. Eine verwaiste Audio-Paketdatei wird abgelehnt. Pakete erlauben bis neun Dateien; Audiokapazität wird zusätzlich berücksichtigt. Bestehende Dateien werden nicht ersetzt.

## Clip / Steuerung

Hauptprotokoll 24 ohne Größenänderung. Replay-Optionsbits 65536 = Ton, 131072 = aufgezeichnete Queries, Bits 24–30 = Lautstärke in Prozent. Frame-Kanal `ItSteals.Clip.v3` bleibt 16 MiB; Offset 136 float AudioPreviewRate (0 = aus), 140 float AudioVolume. Diese Vorschau läuft nur kurz weiter, solange neue Vorschauframes ankommen; Export bleibt deterministisch offline. Frame-Capture selber erzeugt keine Tonaufnahme.

ISCLIP 7 ergänzt hinter `speedKeys quality preset maxFileBytes` die Zeile `audio audioVolume` (bool 0/1, float 0–1). Alte Projekte erhalten stummen Export und Lautstärke 1. Audioexport: Stereo Float32-WAV mit 48 kHz als temporärer Quellkanal, exakte Ausgabe-Framezahl/FPS bestimmt dessen Dauer; Samplezeit über dieselbe SpeedMap wie Video. Bei geänderter Geschwindigkeit ändert sich die Tonhöhe. WAV-Temporärdateien über 4 GiB werden mit Fehlermeldung abgewiesen. MP4 AAC 128 kbit/s; MKV/AVI/MOV Float-PCM. Für Mono/Surround-Quellen erfolgt eine Stereoabbildung; Originaldateien bleiben unverändert.

FNV und Paket-SHA sind lokale Konsistenzprüfungen und kein Herkunftsnachweis.

## Frühere Formate und historische Erweiterungen

# Replay-Daten 7.7.0 / mab

## Dateien und Identität

| Datei | Inhalt |
| --- | --- |
| `run-<ID>.isrun` | Bestehender Map-/Run-Payload 12, einschließlich Eingabe-Metadaten |
| `run-<ID>.isinput` | Separater Eingabe-/Kamera-/Physik-/Colliderstrom, neue Dateien Format 4; Formate 1/2/3 bleiben lesbar |
| `run-<ID>.validation.json` | Am Run-Ende eingefrorener lokaler Prüfbericht |
| `run-<ID>.meta` | Optional: Name, Kategorie, Notizen; vorhandenes Format 1 |
| `run-<ID>.ismarks` | Editierbare benannte Zeitmarkierungen, A–B-Loop und unabhängige Clip-Grenzen, Format 2 |

Die .isrun-Datei wird zuletzt veröffentlicht. Ein unvollständig gespeicherter Satz wird damit nicht als neuer Archiv-Eintrag aufgelistet. Im Eingabestrom werden Run-ID, Szene, Layoutseed, Dauer, Game-Hash, Aufnahmeversionen und Anzahlen wiederholt; alle müssen zur ausgewählten Aufnahme passen. Originaldateien werden nicht überschrieben. Aktuelle PID-/Live-Transportinformationen sind keine Run-Identität.

Der Layoutseed bezeichnet die erfasste Spawn-/Kristallauswahl. Der erste beobachtete RNG-Seed im Validierungsbericht ist kein vollständiger Welt-/KI-Zustand. 3D verwendet beobachtete Bewegung und prüft tatsächliche Spawn-/Kristallpositionen nach dem Laden. Eine identische Input-Neusimulation wird nicht vorausgesetzt.

## Eingabestrom

Little Endian. Header: uint32 Magic `0x4e495349`, int32 Format 1, 2, 3 oder 4, int64 Run-ID, int32 Szene, int32 Layoutseed, double Dauer, UTF-8-Game-Hash und dieselben nullable Eingabe-Metadaten wie im Map-Payload. UTF-8-Texte tragen ein int32-Byte-Längenpräfix. Metadaten enthalten int32 Frames/Ticks/Events/Verworfen, uint32 Qualitätsflags und Tool-/Game-/Unity-Versionen.

Danach folgen drei Blöcke, jeweils int32 Anzahl und feste Datensätze. Ab Format 2 folgt außerdem der unten beschriebene Colliderblock. Format 3 ergänzt danach den Geräteblock; Format 4 anschließend den Grafikblock. Ein uint64-FNV-1a-Checksum über alle vorherigen Bytes schließt die Datei ab. Der Paketexport ergänzt SHA-256 über vollständige Dateien.

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

Tastenbits 0–6: W, A, S, D, linke Shift-Taste, rechte Shift-Taste, R. Zustandsbits 0–8: Spiel fokussiert, Trainer besitzt Eingaben, tatsächlicher Sprint, Radar aktiv, Licht tatsächlich aktiv, Kameraflash aktiv, Controller am Boden, canMove > 0, Freecam aktiv. In Format 3 ergänzt Zustand Bit 9 den tatsächlichen Crouch-Zustand und Bit 10 die tatsächliche Uhransicht. Bei älteren Formaten sind diese Zustände unbekannt. Achsenbits 0–5: Horizontal, Vertical, Mouse X/Y, Joystick X/Y. Aktionsbits 0/1: vom Spiel gelesener R-/Controller-Radar-Impuls.

Kameras werden unmittelbar vor ihrem Rendering beobachtet; Anfang/Ende ergänzen Zustandsproben. Bei einer deaktivierten Hauptkamera wird ihre Pose in LateUpdate beobachtet. Aufgezeichnet wird die Gameplay-Kamera; Freecam-Nutzung bleibt gekennzeichnet. Eingabeereignisse folgen MouseLook.Update; Physik folgt dem originalen FixedUpdate. Wrapper rufen die ersetzten Unity-Eingabefunktionen jeweils genau einmal auf und geben denselben Wert zurück.

## Grenzen und Wiedergabe

- Maximal 262144 Kamerasamples, 524288 Physiksamples, 131072 Ereignisse und 131072 Gerätebeobachtungen; Dateigrenze 128 MiB. Die Hintergrundwarteschlange für neue Replay-Payloads ist auf geschätzte 192 MiB begrenzt. Ein überschrittener Aufnahmepuffer verwirft weitere Proben ausdrücklich, ohne alte Eingaben umzuschreiben.
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

## Geräteblock in Eingabeformat 3

Nach dem Colliderblock: double letzte zuverlässig beobachtete Gerätezeit, int32 verworfene Gerätebeobachtungen und int32 Anzahl. Danach feste Datensätze von **172 Bytes**, zum Schluss die bisherige Datei-Prüfsumme.

| Datensatz, Schreibreihenfolge | Bytes |
| --- | ---: |
| Sekunden (double), Frame/Tick/Zustand/Quellflags (4 × uint32) | 24 |
| 256 logische Windows-Virtual-Key-Zustände (4 × uint64) | 32 |
| Maustasten 0–6 (uint32) | 4 |
| Unity-Joystick 1–8, jeweils B0–B19 (8 × uint32) | 32 |
| Vier XInput-Pads, je uint16 Buttons, byte LT/RT, int16 LX/LY/RX/RY | 48 |
| Gelesene Spielachsen Sprint/CameraFlash (2 × float) | 8 |
| Gelesene Spielachsen Horizontal/Vertical/Mouse X/Y/Joystick X/Y (6 × float) | 24 |
| **Gesamt** | **172** |

Quellflags: Bits 0–3 verbundene XInput-Pads, Bit 8 erfolgreiche Tastaturbeobachtung, Bits 9/10 in diesem Spiel-Frame gelesene Sprint-/Flash-Achse, Bits 16–23 von Unity gemeldete Joysticks. Maus/Controller/Tastatur werden nur bei fokussiertem Spiel ohne Trainer-Menübesitz abgefragt. Akzeptierte Spielzustände und Achsen bleiben getrennte Beobachtungen. Unity-Joysticknummern müssen nicht den XInput-Nummern entsprechen. Unbekannte Gerätebelegungen werden nicht in Xbox-Semantik umgedeutet.

Unveränderte Beobachtungen verlängern nur die zuverlässige Abdeckung, statt denselben Gerätezustand pro Frame zu duplizieren. Nach dem ersten kapazitätsbedingten Verlust bleibt die Abdeckung eingefroren, auch wenn die Eingaben später zu einem früheren Zustand zurückkehren. Die Anzeige erfindet jenseits der Abdeckung keine historischen Eingaben. Gerätewerte werden diskret gehalten; Sticks und Trigger werden nicht als Tastendrücke interpretiert. Alle Zeiten und Zähler müssen monoton, alle Achsen endlich und begrenzt sein. Der Gesamtpuffer für Kamera/Physik/Ereignisse/Geräte ist auf rund 80 MiB begrenzt; zusammen mit den bestehenden Collidergrenzen bleibt die Datei unter 128 MiB.

Neue Eingabenachrichten und das Live-HUD verwenden denselben Beobachter. Die Haupttelemetrie enthält dessen letzten Zustand sowie höchstens acht tatsächliche Spielzustandswechsel. Die Anzeige zeigt höchstens drei Meldungen für 2,5 Sekunden. Es werden keine System-Hooks installiert. Diese Daten bleiben lokale, unsignierte Beobachtungen; insbesondere sind sie kein Beweis für physische Hardware oder unveränderten Prozessspeicher.

## Eigene Markierungen

`.ismarks`: Little Endian, uint32 Magic 0x4b4d5349, int32 Format 1 oder 2, int64 Run-ID, double tatsächliche Dauer/A/B, uint32 Flags (A gesetzt, B gesetzt, Loop aktiv), int32 Markeranzahl. Je Marker uint32 eindeutige ID, double Sekunden, UTF-8-Name mit int32 Byte-Länge; in Format 2 anschließend uint32 Clip-Flags (Start/Ende gesetzt) und double ClipStart/ClipEnd, abschließend FNV-1a uint64. Höchstens 256 Marker, Namen bis 128 UTF-8-Byte, Dateigrenze 64 KiB. Steuerzeichen, doppelte IDs, Zeiten außerhalb der Aufnahme und aktive ungültige Loops werden abgewiesen.

Diese Datei ist editierbar, mit atomarer Speicherung und Backup; `.isrun`, `.isinput` und ursprüngliche Prüfung bleiben unverändert. Eigene Marker werden orange und ausdrücklich als Annotationen dargestellt. Loop-Bereiche brauchen mindestens 0,02 s Abstand. 2D und 3D teilen dieselben Annotationen; 3D-Schleifen laufen auf der monotonen Helper-Uhr.

## Kompatibilität 7.4.0

Hauptprotokoll **20**, Commands **192 Byte**, Telemetrie **16952 Byte**; DLL und EXE zusammen ersetzen. Map-Payload 12, Map-Kanal 9; Settings 16 liest frühere Einstellungen. Eingabeleser akzeptieren 1/2/3, neue Aufnahmen verwenden 3. Annotationen werden als 2 gespeichert; 1 bleibt lesbar. Alte Aufnahmebytes und Aufnahmeversionswerte werden bei Laden/Export nicht umgeschrieben. Fehlende historische Geräte/Collider werden ausdrücklich angezeigt.

Pakete ohne Annotationen behalten Format 1; mit `.ismarks` Paketformat 2 und `annotationsIncluded: true`. Exporter 7.2.0, 7.2.1 und 7.3.0 bis 7.3.5 bleiben unterstützt. Alle Dateien stehen weiter in der SHA-256-Liste, höchstens acht Einträge. Für die 3D-Inbox werden Originalaufnahmen unverändert kopiert. Eigene Marker, Notizen und Clip-Grenzen sind keine Spielereignisse.

Steueroptionen im Hauptprotokoll: 1 Play, 2 freie Kamera, 4 Verfolgerkamera, 8/16/32 Spieler-/Monster-/Kristall-Collider, 64 Level-Collider, 128 Geschwindigkeitsspur, 256 Loop, 512 Collider durch Wände, 2048 Decke ausblenden, 4096 verdeckende Wände transparent. Persistente Maske 7167; 1024 ist ausschließlich ein Seek-Impuls.

## Clip-Ausgaben

MP4/GIF sind eigene Präsentationsdateien unter `%LOCALAPPDATA%\ItStealsPracticeSuite\Clips`, keine Replay-Originale. Der Export liest feste Quellzeiten `Start + Bildnummer × Tempo / FPS`; die Bildanzahl ist aufgerundet, das Ende wird nicht doppelt als zusätzliche Frameprobe geschrieben. Die Wiedergabezeit ergibt sich aus Bildnummer/FPS. MP4 erhält 100-ns-Zeitstempel, GIF ganzzahlige Hundertstelsekunden. Ausgabegrenzen: gerade 320–2560 × 180–1440, höchstens 60 FPS bzw. GIF 25 FPS, Tempo 0,1–8, höchstens 216000 Bilder. Die GUI bietet ausgewählte 16:9-Auflösungen und Bildraten.

MP4 verwendet Media Foundation/H.264 und BT.709-NV12 mit begrenztem Wertebereich. GIF verwendet eine feste Palette aus 192 RGB-Farben und 64 Graustufen mit gestreamtem LZW. Dieselbe quantisierte BGRA-Komposition erzeugt die GIF-Vorschau. MP4 ist verlustbehaftet; seine Vorschau zeigt das Quellbild vor dem Encoder. Beide Formate sind stumm und tragen REPLAY, ursprünglichen Run-Status und Suite-Version/Autor unabhängig von den optionalen Overlays.

Die Encoderwarteschlange hält höchstens zwei wartende Bilder. Es werden keine Bilder aufgrund der Echtzeitgeschwindigkeit übersprungen. Ein temporärer Dateiname wird reserviert; erst die vollständige finalisierte Datei wird ohne Ersetzen eines bestehenden Ziels veröffentlicht. Abbruch entfernt nur die eigene temporäre Datei.

3D-Bilder nutzen einen getrennten, pro Spielprozess benannten 16-MiB-Kanal mit Mutex und genau einer offenen Anfrage. Jeder Request enthält Sequenz, Run-ID, Quellzeit, Maße und Darstellungsoptionen; die Antwort trägt dieselbe Sequenz und top-down BGRA ab Byte 1024. Größen, Run-Identität, Kameralücken und aktiver Run werden geprüft. Die Hauptprotokoll-Commands bleiben dadurch unverändert groß. Unity-Render-/Readback-Aufrufe laufen auf dem Spielthread; Kodierung im Trainer-Hintergrund. Kamera, Orbit, Zeit, Renderziel und Sichtzustände werden danach zurückgesetzt. Aufwendige Bildaufnahme und GPU-Rücklesung gibt es ausschließlich während angeforderter 3D-Vorschau bzw. Export, nicht im normalen Spielpfad.

Die nachfolgenden Abschnitte sind historische Format-/Darstellungsänderungen.

## Darstellungsänderungen 7.3.1

Kein Wire-/Dateiformatwechsel: Hauptprotokoll 18, Eingabeformat 2 (Leser akzeptiert 1/2), Map-Payload 12, Map-Kanal 9 und Settings 15 bleiben erhalten. Annotierte Paketformat-2-Exporte von 7.3.0 werden einschließlich Originaleingaben unverändert importiert. Alte Originalaufnahmen werden nicht umgeschrieben.

Colliderquellen erfassen zusätzlich den Spieler-Komponentenunterbaum und einen gegebenenfalls separaten Agentenkörper des Monsters; Instanz-IDs verhindern doppelte Tracks. Sichtbarkeit wird weiterhin aus den aufgenommenen enabled/active-Zuständen bestimmt. UI-Zähler trennen Spieler/Monster/Kristalle; fehlende Gruppen werden ausdrücklich nicht als verfügbare Daten angeboten.

Der Analyse-Overlay prüft unterstützte Shader in der Reihenfolge Hidden/Internal-Colored, Sprites/Default, UI/Default, Unlit/Color. Die ersten drei lesen Vertexfarben für weiche Farbverläufe. Der solide Unlit-Fallback zeichnet je Gruppe die tatsächliche Materialfarbe und die Spur gebündelt in höchstens 32 Farbbändern; er bietet keine Alpha-Fades. Der verwendete Pfad und die Track-/Sampleanzahlen stehen im Replay3D-Log. Durch-Wand-Ansicht löscht nach fertiger Szenenzeichnung nur die Tiefe der separaten Replay-Kamera, ohne die Szene erneut zu zeichnen. Der nächste Kameraframe beginnt normal. Bei abgeschalteter Option bleibt die Szenentiefe erhalten.

Die Spur verwendet höchstens 360 Kamera-Bandabschnitte mit je zwei Dreiecken für Kontur und Farbkern (höchstens 4320 eingereichte Vertices, im soliden Unlit-Fallback durch beide Windungen höchstens 8640). Vektoren/Abschnitte sind vorab reserviert. Breite wird aus Kamera-FOV, Tiefe und Viewport berechnet und auf 0,02–0,18 u Halbstärke begrenzt. Gemessene 0–10 u/s erhalten einen kontinuierlichen Rot/Amber/Türkis-Verlauf; höhere Werte werden nur farblich bei Türkis geklemmt, nicht im Messwert.

Der Verfolger-Orbit speichert Yaw/Pitch nur für die Replay-Sitzung und folgt der Spielerposition. Seine anfängliche Richtung und explizites Backspace-Ausrichten stammen aus der aufgezeichneten Kamera. Menü/Fokus-Grenzen schützen Maussteuerung wie bei der vorhandenen freien Kamera. Seek, Pause und Loops erhalten den Winkel; das Sitzungsende setzt ihn zurück. Keine Kamera-Kollisionsabfragen, keine Änderungen aufgezeichneter Posen oder Spieltransforms.

## Darstellungsänderungen 7.3.2 / Hauptprotokoll 19

Commands bleiben 192 Byte, Telemetrie 16516 Byte; der benannte Hauptkanal wechselt auf v19 und verhindert gemischte DLL-/EXE-Versionen. Datei- und Paketformate bleiben gleich. Annotierte 7.3.0-/7.3.1-Pakete werden mit unveränderten Originalbytes importiert.

Die Verfolgerkamera speichert Radius/Yaw/Pitch lokal für die Sitzung. Ein Mausradschritt multipliziert den Radius mit exp(-0,12 * Schritt), begrenzt auf 0,75–20 u; Pitch bleibt in -80 bis +80 Grad. Mouse X/Y bei rechter Maustaste drehen direkt; Fokus-/Menüschutz bleibt erhalten. Replay3dFollowDistance in der Telemetrie ist nun der tatsächliche Radius. Replay3dFollowHeight bleibt als reserviertes 0-Feld erhalten; die beiden gleichnamigen Befehlsfelder steuern die Kamera nicht mehr. Rate-/Sicht-/Seek-Befehle setzen den lokalen Zoom nicht zurück.

Sichtoptionen verwenden die bestehenden Oberflächenrollen der Analysepalette. Jeder Renderer behält seine Materialslotzahl und originalen Batch-Bereiche. Für Alpha werden 20 gemeinsam verwendete transparente Materialvarianten und ein unsichtbares Material einmal erstellt; Standard-Fade bzw. unterstützte Alpha-Shader werden verwendet. Nur Wand-/Tür-Slots erhalten einen eigenen interpolierten Alpha-Property-Block, Zielwert 0,18; Decken-Slots werden getrennt ausblendbar. Verlassen/Fehler räumt diese Auswahl vor der vollständigen Original-Rücksetzung auf.

Verdeckungsprüfung: höchstens 20 Hz, je ein RaycastNonAlloc in beide Richtungen mit je 64 Trefferplätzen. Nur bestehende solide Collider mit passender Renderer-Hierarchie und Trefferpunkt im Rendererbereich werden zugeordnet. Der Rückweg berücksichtigt eine Kamera innerhalb eines Colliders. Trigger, Spieler, Monster und Kristalle sind keine Wandquellen. Keine Wand-Bounding-Box als Ersatz für einen fehlenden Physiktreffer. Bei zusammengesetzten Renderern erfolgt Transparenz auf der vorhandenen Materialabschnitt-/Rendererebene; Türenzustände bleiben wie bisher Teil der geladenen Szene, keine aufgezeichnete historische Weltrekonstruktion.

Die Spieler-Wire-Geometrie verwendet unveränderte Box-/Kugel-/Kapsel-/Controller-Zentren und Maße. Striche entstehen als kameraseitige Dreiecke mit Nahgrenzen-Clipping und Pixelbreite 2,5/4,5 für Kern/Kontur, skaliert mit der Ansicht. Kameradaten werden je Renderpass gecacht; maximal 4096 Liniensegmente je Spielerpass. Der feste Cull-Modus von Unlit/Color wird durch beide Dreieckswindungen berücksichtigt; interne/Vertexfarben-Shader benötigen keine Verdopplung. Andere Collider bleiben schmale Linien; die Geschwindigkeitsspur behält ihre 360-Abschnitt-Grenze.

## Darstellungsänderungen 7.3.3

Wand-Renderer werden vor dem Palettenwechsel über Walls-Layer, Wall-Tags, Objekt- und Materialnamen zugeordnet. Bestätigte Hauptwand-Grafik und Hauptwand-Collider dürfen getrennte Hierarchien haben. Bounds-Überschneidung allein erzeugt keinen Fade: ein tatsächlicher Hin-/Rückstrahl-Treffer innerhalb des Renderers bleibt erforderlich. Breite, bestätigte Wände erhalten Vorrang vor dem bisherigen horizontalen Bounds-Fallback; explizite Boden-/Decken-/Tür-Materialrollen bleiben bestehen. Die Originalszene wird weiter vollständig zurückgesetzt. Hauptprotokoll 19 und alle Dateiformate unverändert; auch annotierte 7.3.2-Pakete bleiben lesbar.

## Darstellungsänderungen 7.3.5

7.3.4 wurde verworfen. Keine GPU-Buffer-Rücklesung, Mesh-Kopien/-Aufteilung oder Ersatzrenderer. Physische Hin-/Rückstrahlen wählen Originalrenderer; normale Werte und ausreichende vertikale Ausrichtung der Treffernormalen sind erforderlich. Boden-/Prop-/Clock-/Decken-Metadaten und gemessene Controller-Bodenhöhe schützen andere Flächen.

Nicht-instanzierte Alpha-Materialien verwenden RenderType Transparent, Renderqueue 3000, SrcAlpha / OneMinusSrcAlpha und ZWrite 0. Alpha-Varianten werden beim Laden für die jeweiligen Renderer vorbereitet und direkt über Materialfarbe verändert. Die vorhandene Original-Restore-Scope bleibt erhalten. Interpolierte aufgezeichnete Kameraposition ist das Fokusziel von Verfolgeransicht und Sichtstrahlen; es wird kein fester Body-Höhenwert addiert. Die Original-Kameramaske bleibt erhalten.

Hauptprotokoll 19 und sämtliche Aufnahme-/Paketformate unverändert. Auch annotierte 7.3.4-Pakete bleiben lesbar. `replay3dMessage` enthält höchstens einmal pro Sekunde aktualisierte Angaben zu Wandkandidaten, Physiktreffern, Zuordnungen und derzeit gefadeten Renderern. Diese Zahlen prüfen Auswahl/CPU-Zustand und ersetzen keinen Pixel-/Treiber-Livetest.


## Clip-Projekte ab 7.5.2: ISCLIP 3

Schnittmetadaten bleiben getrennt vom Original-Replay. Die Textdatei unter `Clips/Projects` speichert pro Keypoint zusätzlich zu Zeit, Übergang, Name und Pose: Kameramodus (0 Recorded, 1 Follow, 2 Free), Layer-Maske, Layer-Werte und Follow-Yaw/Pitch/Radius/Pan. Die Maske enthält nur explizit geänderte Werte; ein gesetzter Wert muss Teil der Maske sein. Bits: 1 Colliders, 2 Route, 4 Monster, 8 Hide ceiling, 16 vorhandenes Wall fading. Änderungen gelten ab dem Punkt und werden pro Bit bis zur nächsten Änderung weitergegeben.

Recorded/Follow werden für die jeweilige Quellzeit dynamisch aus der Aufnahme ausgewertet; Free ist eine feste Pose. Cut schaltet am nächsten Punkt, andere Übergänge blenden die zum selben Zeitpunkt ausgewerteten Kameras. Vor dem ersten Punkt gilt Recorded, nach dem letzten dessen Modus. Die Ausgabeframes folgen weiter Start + Index × Tempo / FPS.

Formate 1/2 bleiben lesbar: alte statische Posen werden Free-Punkte ohne Layer-Overrides; eine vorhandene Spur wird aktiviert. Bestehende v1-Activity-Migration bleibt erhalten. Hauptprotokoll 21, Frame-Kanal 2, Settings 18 und Run-/Eingabeformate bleiben unverändert. Die Projektprüfsumme erkennt unbeabsichtigte Änderungen und bietet keine kryptografische Authentizität.

## Grafikblock ab 7.6.0: Eingabeformat 4

Direkt nach den Geräten und vor der Dateiprüfsumme: byte Recorded (0/1), int32 Dropped, int32 Assetanzahl, UTF-8-Assetschlüssel; int32 Owneranzahl. Owner: int32 Gruppe/ID/Monstertyp, UTF-8-Rootpfad, int32 Trackanzahl. Track: int32 Art/Komponentslot, UTF-8-Pfad, int32 Sampleanzahl. Transformpfade kombinieren Geschwisterindex und escapten Namen; relative Pfade für Actors, Szenenpfade für Weltobjekte. Texturen/Sprites referenzieren installierte Assets per Name, Dimensionen und Sprite-Rechteck; keine Assetbytes.

Gruppe: 0 Welt, 1 Spieler, 2 Monster, 3 Kristall. Art: 1 Pose, 2 Renderer/Material, 3 Sprite, 4 Licht. Jedes Sample beginnt mit double Originalsekunden. Pose: Vector3 Position, Quaternion Rotation, Vector3 Skalierung (**48 Byte**). Renderer/Sprite: uint32 Flags, RGBA (4 float), int32 Sprite-/Textur-ID (**36 Byte**). Licht: uint32 Flags, RGBA, Vector3 Position, Quaternion Rotation, float Intensität/Reichweite/Spotwinkel/innerer Winkel (**72 Byte**). IDs sind 1-basiert, 0 bedeutet keine Referenz. Flags: 1 sichtbar/aktiv, 2/4 Sprite-Flip, 8 Materialfarbe vorhanden, 16 Haupttextur vorhanden. Welt-/Actorroot-Posen und Lichtposen absolut; übrige Actor-Bones lokal.

Grenzen: 512 Owner, 8192 Tracks, 4096 Assetreferenzen, 393216 Samples, Grafikblock höchstens 24 MiB. Texte höchstens 1024 UTF-8-Byte. Diskrete Zustände werden bis zum nächsten Sample gehalten. Posen interpolieren nur kurze, kontinuierliche Abstände; Halteanker verhindern Bewegung durch lange Stillstandszeiten. Reihenfolge, Quaternions, Zahlen, Assetindizes und Duplikate werden in C# und C++ geprüft. Die Hauptdatei bleibt auf 128 MiB begrenzt; Kamera/Inputbudget neu 72 MiB mit Reserve für Collider/Grafik. Die Speicherwarteschlange berücksichtigt nun auch Geräte, Listenreserve und Grafikdaten.

Grafik-Dropped ist von Input-Dropped und Run-Validierung getrennt. Bei Limitüberschreitung keine erfundenen Animationen; letzter Zustand und Reduktionshinweis. Fehlende historische Grafikdaten in Format 1–3 bleiben explizit Legacy. Statische ursprüngliche Modelle dienen dabei nur als Positionsvisualisierung.

## Clip-Projekte und Zeitabbildung ab 7.6.0: ISCLIP 4

Nach bisherigen 13 Optionsflags folgt SimplifiedGraphics (0/1). Jede v3-Keypointzeile wird nach Modus/Layer/Follow um int SpeedOverride (0/1), double PlaybackSpeed (0,1–8), int SpeedTransition (dieselben sechs Kurven) ergänzt. Formate 1/2/3 lesen weiter mit deaktivierten Tempo-Overrides. Checksummen/Run-ID/Dauer/Gamehash und Schutz zukünftiger Formate bleiben bestehen.

Die bisherige Quellformel gilt ohne Speed-Punkte. Mit aktiver Spur ist s(t) die stückweise Quellgeschwindigkeit; Ausgabezeit F(t)=Integral(0..t, 1/s(u) du). Frame n liest die Quellzeit F^-1(F(ClipStart)+n/FPS). Clipdauer F(Ende)-F(Start), Bildanzahl aufgerundet. Cut hält bis zum nächsten Override, weiche Kurven interpolieren dessen Tempo. Speed-Punkte bleiben unabhängig von Kamera-Übergängen. Eine zwischengespeicherte Integrationstabelle mit Gauss-Quadratur und deterministischer Inversion vermeidet Integration über die gesamte Spur pro Frame. PNG-Manifeste mit aktiver Speed-Spur ergänzen variableRate=true und sourceTimes[] je Bild.

Stand 7.6.0: Hauptprotokoll **22**, persistente Replay-Maske **23551** inklusive **16384 Simplified**. 8192 bleibt ein Clip-Capture-Flag für Monstermodelle. Frame-Kanal weiterhin 2; Map 12, Annotationen 2, Settings 18. Der Layoutgenerator prüft jetzt auch die C#-Kanalidentität, damit DLL/EXE dieselbe Versionsnummer und dieselben Namen verwenden.

## 7.7.0: Fullbright / Projekt 6 / Export

Hauptprotokoll **23**, unveränderte Größen 192/16952 Byte. Replay-Fullbright ist Bit **32768**, persistente Replay-Maske **56319**. Clip-Frame-Kanal bleibt v2 und akzeptiert Maske **65535** inklusive Monstermodelle 8192. Kamera-Layer Fullbright ist Bit **32**, alle Layer **63**.

**ISCLIP 5** ergänzt hinter Simplified das boolesche Fullbright-Feld; alte Formate erhalten false. **ISCLIP 6** ergänzt unmittelbar nach der Bool-Flagzeile eine eigene Zeile `speedKeys quality preset maxFileBytes`. speedKeys ist 0/1, quality 0–2, preset 0–9, maxFileBytes uint64 (0 ohne Limit; sonst nur Format 6). Alte Projekte erhalten true / 1 / 9 / 0. Ausgabeformate: 0 Windows H264/MP4, 1 GIF, 2 FFV1/MKV, 3 RGBA-PNG, 4 UtVideo/AVI, 5 ProRes-HQ/MOV, 6 x264/MP4. Video-/PNG-FPS maximal 120, GIF maximal 25. Speed-Overrides der Kamerapunkte bleiben im bestehenden Layout; deaktiviertes speedKeys übergeht sie nur für die Zeitabbildung. Leser akzeptiert 1–6 und erhält unbekannte zukünftige Dateien.

Externes Encoding setzt feste Zeitbasis 1/FPS und PTS=N, feste Bildzahl und CFR. Quellzeit ist a + n/FPS × rate oder die inverse integrierte Speed-Spur. Die Videolänge ist ceil(OutputDuration × FPS)/FPS; der letzte Quellframe liegt vor dem exklusiven Ende. Größe wird nach Containerabschluss geprüft, niemals durch -fs begrenzt. Eine zusätzliche `.json`-Begleitdatei protokolliert die geplante Zeitabbildung und tatsächliche Dateigröße. Sie ist kein Run-Verifikationsnachweis.
