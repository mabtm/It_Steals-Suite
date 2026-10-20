# Codeprüfung 7.11.0 / mab

## Zugriffsmodell

InspectorFields beschränkt Reflexion auf Instanzfelder aus der tatsächlichen Spielassembly. Geerbte Felder werden aufgenommen; Static, Literal, Pointer und ByRef werden ausgelassen. Allgemeine Properties, Methoden, Collection-Enumeratoren und Script-ToStrings werden nicht aufgerufen. Primitive, benannte Enums, Vektoren, Quaternion/Color, kurze Strings, Arraylänge und Unity-Objektidentität werden explizit formatiert. Fehler einzelner Reads bleiben als Unavailable sichtbar. Bekannte Unity-Komponenten lesen gezielte Engine-Properties; sharedMaterials/sharedMesh vermeiden unbeabsichtigte Materialinstanzen.

LiveInspector läuft ausschließlich auf dem Unity-Thread nach explizitem Hauptkanal-Opt-in. Clean Speedrun, Playback/Export und Verbindungsverlust sperren unabhängig vom GUI. Szene/Epoch und Hauptkanal-Session binden alle Befehle und Ergebnisse. Geschlossen bleiben weder Objektlisten-Scans noch Watch-/Detailabfragen aktiv; Objekt-/Feldreferenzen werden gelöst. RunAids.LiveInspector ist 512; AuditBeforeSuppression und Configure erfassen die tatsächliche Aktivität. Der bestehende Eligible-Test verwirft jedes Aid und enthält nun auch diesen Fall.

## Daten und Performance

Die Objektliste verwendet einen auf mehrere Frames verteilten Transform-Stack, keine wiederholte FindObjectsOfType-Weltsuche. Ein Slice höchstens 256 Schritte bzw. ungefähr 0,7 ms zwischen einzelnen Unity-Aufrufen; einzelne native Aufrufe können das Budget überschreiten. 20000 Objekte und 4 MiB begrenzen die Ausgabe. Die Liste beschreibt den Refresh-Zeitraum, die ausgewählten Werte sind laufend beobachtet.

Watches höchstens acht einmal pro beobachtetem Frame. Änderungen im 128er Ring mit 180000-Byte-Budget; die Detailansicht publiziert normalerweise höchstens 5 Hz. C# schreibt in getrennt revidierte Katalog-/Detailbereiche unter nicht blockierendem Mutex. Der Trainer kopiert nur neue Bereiche; Prüfung/Dekodierung und Routenproben laufen im einzelnen Inspector-Worker. Der Worker startet erst nach vollständig konstruierten Daten; Sitzungswechsel und Generationszähler verwerfen alte Arbeiten. Neue Endpunkte löschen die alte Probe. CPU-Zeiten in der Ansicht sind explizit letzte Detail-Baukosten bzw. kumulierte Katalog-Arbeit, keine behaupteten Game-FPS.

Native Parser prüfen alle Zähler, Bytebudgets, Flags, Bool-Werte, endliche Koordinaten, UTF-8, FNV64 und Identitäten. Keine rohen Pointer über die Verbindung. UI-Caches halten shared_ptr statt bloßer Adressen, damit wiederverwendete Allokatoradressen keine alte Indexliste oder Snapshot-Differenz als aktuell erscheinen lassen. Virtuelle Objektzeilen, begrenzte Konturen und feste temporäre Polygonpuffer begrenzen Zeichnungsarbeit. Neue Komponente isoliert Fehler vom Telemetrie-/Run-Recorder-Pfad.

## Geometrie und Kamera

SceneGeometry zeichnet seine Entscheidungen direkt an den vorhandenen Filter-/Aufnahmezweigen auf; keine neue Routingklassifikation. Die Konturen referenzieren dieselben MapFootprint-Objekte, die der bisherige Pass erzeugt. Erfasste Spielerhöhe, Radius, Grounded-Zustand, Frame und Zeit ermöglichen den Abgleich mit aktuellen Colliderdaten. Eine fehlende Kontur wird nicht durch Bounds-Wände ersetzt. Die bestehenden Filter für H&S-Culling-Kataloge, Pickup-/Actor-Ausschluss und physische Kollision bleiben erhalten.

Inspector-Wire nutzt ColliderWireGeometry / ReplayColliderShape. Zusätzlich dürfen explizit ausgewählte lesbare Mesh-Collider begrenzt echte Dreieckskanten darstellen; 60000 Vertices / 180000 Indizes / 8192 Kanten insgesamt. Nicht lesbare oder größere Meshes melden die Grenze; keine Bounding-Box als Collider. Physics-Auswahl nutzt begrenztes NonAlloc-Raycasts; voller Trefferpuffer wird als unvollständig benannt. Optionales Renderer-Bounds-Picking ist ausdrücklich eine ungefähre Auswahlhilfe. Eltern/Kinder helfen bei getrennten Render-/Collider-GameObjects.

Die Routenprobe verwendet NavGraph im bestehenden floorOnly-Modus und FloorProject, Component, Search und Path inklusive physischer Clearance-/Umwegreparatur. Sie übergibt keine Boden-Dreiecksindizes an alte Korridorknoten. Unbekanntes/Unverbundenes liefert einen klaren Status, keine erfundene direkte Linie. Acht-Modi-Detour- und Korridor-Fixtures prüfen diesen Pfad.

Focus verwendet die bestehende Freecam, aktiviert keine inaktiven Objekte und versetzt nicht den Spieler. Für inaktive Renderer dient deren Transform statt potenziell leerer Renderer-Bounds als Fokus. Nur eine vom Inspector gestartete Freecam wird beim Schließen restauriert. Das Menü gibt Eingaben erst nach exakt passendem erfolgreichen ACK ab; Timeout oder fremder ACK lässt es geöffnet. Reconnect-Serien können den letzten Helper-Befehl nicht versehentlich als neue Bestätigung wiederverwenden.

## Exporte und Regressionen

Lokaler JSON-Snapshot mit Quellen, Beobachtungszeit, getrenntem Geometrie-Erfassungszeitpunkt, Build/Seed-Art, optionalen Vergleichs-/Probedaten und tatsächlicher Diagnose. Unbekannte Seeds bleiben null, unerreichbare Längen null. Keine willkürliche Ursache/Save-State-/Attestierungsbehauptung. Exklusive Dateianlage mit CREATE_NEW/O_EXCL schützt vorhandene Dateien; der Pfad wird erst nach abgeschlossenem Schreiben als Erfolg angezeigt.

Hauptprotokoll auf 27, eigene Inspector-Verbindung auf 1; unveränderte Commands-/Telemetriegröße und unveränderte Replay-/Clip-Formate. Paketmanifest 7.10.1 ausdrücklich rückwärts lesbar. Aktuelle Build-, C#/C++-, Sanitizer- und vollständige ImGui-Nachweise sowie reale Grenzen in PRUEFUNG.md. Kein Live-Spielnachweis; H&S/Outside-WIP bleibt.

Mesh-Wire-Präzision: Primitive werden live gelesen. Nicht-konvexe lesbare Mesh-Collider zeigen einen begrenzten Source-Mesh-Snapshot vom Auswahl-Frame; bei Topologieänderungen neu auswählen. Ausgetauschte Mesh-Referenzen werden nicht mit alten Kanten gezeichnet. Cooked-Convex-Hüllen sind nicht verfügbar und werden ohne Ersatz ausgelassen.
