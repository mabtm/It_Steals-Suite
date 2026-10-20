# Projekt-Audit: PracticeSuite 6.0.0

## Umfang und Ausgangspunkt

Die 5.0.1 wurde byteweise mit dem zuletzt bereitgestellten Paket verglichen und in einem eigenen Arbeitsstand fortgeführt. Die funktionierende Ausgangsversion wurde nicht überschrieben. Eigene C#-Quellen, native Anwendung und Header, Build-Dateien, Protokollgenerator, Konfiguration, Tests und Dokumentation wurden durchgesehen. Die vendorte Dear-ImGui-Version 1.91.9b samt Lizenz bleibt unverändert.

Die tatsächliche Assembly-CSharp.dll, Unity-Managed-Referenzen, MelonLoader 0.7.3.0 und Harmony 2.10.2.0 wurden für Compilation und IL-Analyse verwendet. Die Assembly enthält 22 untersuchte Seed-Aufrufstellen in 15 Methoden. Windows- und Unity-Laufzeiteffekte wurden nicht simuliert als bestanden ausgegeben.

## Architektur

Unity-/Spielobjekte werden vom Helper auf dem Unity-Thread gelesen und geändert. Die native Anwendung liest keine einzelnen Pointer aus fremdem Prozessspeicher. Ein benannter Mutex schützt vollständige Shared-Memory-Snapshots und die getrennte Befehlsstruktur; beide Seiten warten mit Timeout 0 und überspringen einen beschäftigten Austausch.

Die UI verwendet ausschließlich eine lokale, validierte Snapshot-Kopie. Renderzustand, Fensterbindung, Steuerintents und Segmenttracker gehören zum nativen Hauptthread. Persistenzwriter erhalten unveränderliche Textaufträge; sie greifen nicht auf Unity-Objekte oder UI-Präferenzen zu. Kurze Queue-Locks schützen nur die Übergabe. File-I/O liegt außerhalb dieser Locks.

Practice-Routen, Kartenoptionen und Verification-Darstellung sind eigene Komponenten. Der bestehende Renderer und die getestete Eingabestrategie wurden erhalten. Das Protokollschema bleibt die gemeinsame Quelle für sizeof-/offsetof-Prüfungen und C#-Offsets.

## Befunde und Umsetzung

| Bereich | Problem bzw. Grenze | V6-Verhalten |
|---|---|---|
| Game-Lifecycle | Start ohne Spiel / Spielende beendeten den Trainer | Warten im Tray, Handles/Hooks freigeben, erneutes Finden nur ohne aktive Verbindung |
| Helper-Neustart | Alte Modus-/Teleport-/Load-Intents könnten erneut gesendet werden | Sessionwechsel löscht Steuerzustände; Header- und Payload-PID müssen übereinstimmen |
| Input | Bekannter 5.0.1-Fix ist für Win32-Klickzustellung entscheidend | Layered/Transparent/NoActivate für passive Anzeigen; kein deaktiviertes Fullscreen-HUD |
| Fokus/Rendering | Wechsel der Eingaberolle betrifft HWND und Graphics-Backend | Surface-Wechsel bleibt außerhalb ImGui-Frames; ausgehendes Menü wird verborgen |
| Modusregeln | Veraltete Telemetrie darf keine zweite HUD-Ansicht aktivieren | Native Moduspolicy bleibt maßgeblich; 128 Render-Einstiegspunkt-Fälle |
| Seed | Vollständige Aufrufanzahl allein identifiziert nicht den Game-Build | Bekannter Datei-SHA256 plus Originalquellen und Hook-Ownership erforderlich |
| Seed-Evidence | Bei fehlender Abdeckung ging eine beobachtete Zahl verloren | Zahl behalten, Einordnung UNKNOWN; SET bleibt für tatsächliche Ersetzung latched |
| Start-Erkennung | Fallback-Anbindung beweist kein beobachtetes MouseLook.Start | Expliziter Start-Evidence-Parameter, Warnung und PB-Ausschluss bei Fallback |
| Integrität | Laufende Feature-Anzeige allein verliert vergangene Hilfen | Aktuelle Features und während des Runs verwendete Hilfen separat |
| Run-Timing | NaN, rückwärts laufende Zeit und Beobachtungslücken | Monotoner Timer, Grenzen, Warnungen; keine unsicheren neuen PBs |
| Death | Pauschales Überschreiben von Death kann stock Gameplay blockieren | Death-Hook übernimmt nur einen tatsächlich getrackten aktiven Versuch / Reload |
| Zähler/History | Gleiche UTC-Auflösung kann identische IDs erzeugen | Eigene monotone ID, tatsächlicher UTC-Zeitpunkt unverändert; doppelte Hooks geschützt |
| Stats | Teilweise/defekte Daten und zukünftige Formate | Prüfsumme, Pflichtwerte, Bounds, .bak-Recovery, beschädigte Datei sichern, Future-Guard |
| Reports | Crash-Journal könnte vorhandenen abgeschlossenen Report ersetzen | Vorhandene Reports bleiben unverändert; Fehler wird gemeldet |
| Config-I/O | Synchrone Writes im Render-Loop erzeugen vermeidbare Wartezeit | Coalescing-Worker; atomare Ersetzung, Backup, Fehler-/Retry-Publikation |
| Map-Sampling | Unbenötigte Objekt-/Geometrieabfragen | Kategorieanforderungen für Walls/Crystals/Monster; Path separat |
| Geometrie | Wiederholte Listen und Trigonometrie | Wiederverwendete Merge-/Clip-Listen, Grundformen gecacht |
| Minimap-Winkel | Unity-Yaw und Karten-Up konnten um 90° abweichen | Yaw 0 = +Z, Yaw 90 = +X; einmaliger Transform pro Frame |
| Interpolation | Kurze Teleports / gleiche neu geladene Szene wirken wie Bewegung | Teleport-Serial und Scene-Epoch resetten Pose, Geschwindigkeit und Trail |
| Fullbright | Ein ungültiger Renderer kann weiteres Aufräumen verhindern | Temporärmaterial zuerst besitzen; einzelne Restore-Fehler isolieren, Kopien freigeben |
| Collider | Alte statische Hitbox-Referenzen bleiben nach Szenenwechsel | Clear bei Szenenwechsel, Disable und Shutdown |
| UI | Practice-Kernfunktion zwischen erweiterten Optionen schwer auffindbar | Segmenttraining zuerst, seltene Szenenwerkzeuge einklappbar; gemeinsame HUD-Komponenten |

## Aktualisierungsraten und Ressourcen

| Arbeit | Takt / Auslöser |
|---|---|
| Native sichtbare Frames | 30 / 60 / 120 FPS, Win32-Waitable-Timer; kein Busy-Wait |
| Unsichtbares Overlay | Kein Rendering; gewöhnlicher Austausch 2 Hz |
| Aktives Segment, auch ohne Overlay | Snapshot-Anforderung/Austausch bis 60 Hz, keine Kartenanforderung nur wegen Segment |
| Helper-Befehle | Höchstens 60 Hz, Mutex ohne Warten |
| Sichtbare Live-Daten | Höchstens 60 Hz; vom Spiel-FPS abhängig |
| Verborgene Daten | Gewöhnlich 2 Hz, außer aktive Details/Hitboxen/Segmentanforderung |
| Heartbeat | 500 ms; unverbundene Hilfen nach etwa 3 s zurücksetzen |
| Game-FPS-Anzeige | Tatsächliche Helper-Update-Frames über etwa 0,5 s |
| RNG-Hook-Ownership | Etwa alle 2 s sowie vor Run-Start/Ende |
| Karten-Geometrie / Collider-Entdeckung | 1 Hz / etwa alle 2 s, nur bei Bedarf; F5 invalidiert |
| Monsterpfad / optionaler Trail | Maximal 10 Hz; Trail-Ring 128 Samples |
| Display-Präferenzen | Vergleich etwa alle 1,5 s, gleiche Inhalte schreiben nicht |
| Aktiver Run-Journal-Checkpoint | Alle 5 s; Start/Ende/Seed-/Hilfenänderungen sofort einreihen |
| Run-History / tägliche Zähler | 64 / maximal 366 gespeicherte Tage |
| Reports / Events | Opt-in; maximal 64 wartende Jobs, 128 Events pro Run |
| Game-Dateiidentität | Ein SHA256-Durchlauf im Hintergrund |

Diese Werte sind Code-Takte, keine gemessenen CPU-/RAM-/FPS-Einsparungen. Der Lookup nach einem fehlenden Spiel erfolgt etwa einmal pro Sekunde; mit gehaltenem Prozesshandle wird nicht fortlaufend die Prozessliste durchsucht. Teure Debugtexte entstehen erst beim geöffneten Diagnose-Tab.

## Practice und Minimap

Positions-Segmente liefern einen unmittelbaren Trainingsnutzen, ohne Unity-Objektgraphen als vermeintlichen Save-State zu serialisieren. Ein Versuch beginnt erst nach erfolgreichem Teleport-Feedback. Szene, Seed-Kontext, Modus, Gesundheit und Datenlücken werden berücksichtigt; unsichere Versuche enden abgebrochen.

Timing basiert auf den tatsächlich beobachteten Snapshots. Dies ist keine sub-frame Engine-Trigger-Messung. Statistik vergleicht Versuche desselben Punktepaares; Random-Punkte sind bewusst an die geladene Welt gebunden. Separate Punkte-/Statistikslots existieren für die sechs spielbaren Modi.

Die Minimap bietet Follow/Overview, Orientierung, Zoom, Radius, Position und Markerfilter. Die Transform- und Clipping-Tests prüfen heading, Skalierung, Bounds und Überdeckung. Der Spieler wird zuletzt gezeichnet, damit Practice-Marker und Trails ihn nicht verdecken. Filter wirken auf Darstellung und Game-Datenanforderung.

## Bedeutung der Verification-Daten

NO bedeutet beobachteten SET-Seed oder verwendete Helper-Hilfen. UNKNOWN bedeutet fehlende vollständige externe Verifizierbarkeit, auch wenn keine Hilfen beobachtet wurden. Weder eine Anzeige noch ein JSON kann andere externe Manipulationen oder eine veränderte laufende Assembly vollständig ausschließen.

Die Build-Identität ist eine einmalige Prüfung der Datei auf der Platte. Ein generischer Application.version-Wert ersetzt den Dateihash nicht. Metadaten werden am Run-Start gelatched; ein zu diesem Zeitpunkt unbekannter Hash wird im begonnenen Run nicht rückwirkend als erkannt ausgegeben.

RANDOM meint den beobachteten unveränderten RNG-Pfad dieser unterstützten Assembly. Es ist keine Zusicherung eines einzelnen dauerhaften Welt-Seeds. Ein SET-Seed ist der Practice-Base-Seed des Helpers. Änderungen der Quellensicherheit führen zur Herabstufung; eine begonnene Unsicherheit wird nicht im selben Run einfach zu RANDOM aufgewertet.

Reports enthalten Modus/Level am Start, UTC-Zeiten, beobachtete Dauer, Seed-Evidence, kumulierte Hilfen, Warnungen und Status-Events. Kein Report wird ohne Opt-in angelegt. Aktives Journal speichert begrenzte Events für Crash-Recovery; History hält nur Zusammenfassungen, um die Stats-Datei klein zu halten. Report-Fehler und Queue-Überlauf werden sichtbar.

## Migration und Wartbarkeit

Vorhandene Feldnummern und Offsets bleiben unverändert; V6 hängt Felder an und nutzt reservierte Command-Bytes. Neuer Kanal v5 verhindert Austausch mit älteren, unterschiedlich großen Payloads. Game-Pointer sind keine Grundlage des UI-Datenzugriffs.

Config-Defaults und Zahlengrenzen sind zentral. Fehlende Felder alter Formate behalten Defaults, zusätzliche HUD-Felder erweitern bestehende Indizes. Stats werden kulturenunabhängig und mit voller Double-Präzision geschrieben. Ungültige Einzel-History-Einträge werden ignoriert, globale Beschädigung führt zu expliziter Recovery.

Eigene Worker, Maps, Mutex-/Process-Handles, Fenster-Hooks, D3D-/Composition-Objekte, FreeCam-Zustand, Hitbox-Referenzen und temporäre Materialien haben explizite Freigabewege. Unity-Aufräumen toleriert bereits zerstörte Objekte. Vorhandene Features wurden nicht durch einen Komplett-Rewrite ersetzt.

## Offene praktische Grenzen

Der vollständige Build, Regressionen, DLL-IL-Analyse und echte ImGui-Draw-Daten sind geprüft. Hier gibt es keinen laufenden Windows-/Unity-Spielprozess. Tray-Verhalten, echter Fokus und Cursor, Alt+Tab, Monitore/DPI, Device-Loss, Harmony-JIT und vollständige Game-Zustandsübergänge bleiben Zielsystemprüfungen.

Ein Disk-Hang kann auch einen Writer am Shutdown blockieren; beim C#-Store wird nach drei Sekunden gewarnt. Die native Flush-/Join-Phase wartet auf den lokalen Writer. Das wird nicht als generelle Garantie gegen blockierte Betriebssystem-I/O verkauft.

Es wurden keine Full-Save-States, deterministischen Replays, Auto-Verifikationsfreigaben, externen Leaderboard-Integrationen oder unzuverlässig abgeleiteten Movement-Bewertungen eingebaut. Solche Funktionen benötigen weitere verifizierbare Game-Daten und konkrete Regeln.

PRUEFUNG.md beschreibt die ausgeführte technische Validierung und die Windows-/Spiel-Abnahme.
