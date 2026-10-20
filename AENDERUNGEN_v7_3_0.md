# PracticeSuite 7.3.0 / mab

## 3D-Ansicht

- Bestehende Level-Renderer, Transform-Hierarchien und statische Batch-Bereiche bleiben erhalten.
- Schattenfreie Richtungsbeleuchtung, Umgebungslicht und gemeinsame abgestufte Flächenfarben liefern Tiefe. Unbeleuchteter Fallback bei fehlender Shader-Unterstützung.
- Materialslots, Property-Blocks, Lightmaps, Licht-/Reflexions-Probes, Umgebung und ursprüngliche Lichtzustände werden zurückgesetzt. Keine Level-Mesh-Kopien, keine Echtzeitschatten und keine Nachbearbeitung.
- Aufgezeichnete Kamera, Verfolgerkamera mit Abstand/Höhe und Wandabstandstest, freie Kamera.
- Farbige kurze Geschwindigkeitsspur aus tatsächlich beobachteter Bewegung, mit begrenzter Segmentzahl und ohne Verbindungen über Datenlücken.

## Bedienung

- Große WASD-/Shift-/R-Anzeige unten mittig, akzeptierter Sprint-/Radarzustand und Blickindikator.
- Tab öffnet die direkt eingeblendete 3D-Bedienleiste und gibt die Maus frei. F3 öffnet das vollständige Menü. Escape schließt zuerst die Leiste bzw. beendet die Ansicht.
- Gemeinsame 2D-/3D-Timeline mit Pickups, Flashes und benannten Annotationen; Ziehen/Mausrad, Frame-Schritte, direkte Sekunden-Eingabe mit Go/Enter und stufenlos 0,1–8×.
- Space/Pfeile/Home/End/+/− bei Spiel-Fokus bedienen die Wiedergabe; M erstellt eine Markierung. Texteingaben und aktive Runs sind geschützt.
- A–B-Schleifen laufen auf der monotonen Helper-Uhr, auch ohne geöffnetes Menü. Kamera-/Darstellungs-/Ratenänderungen führen ohne expliziten Seek keinen Zeitsprung aus.
- Marker lassen sich benennen, zeitlich ändern und entfernen. Sie werden separat gespeichert und exportiert; Originalereignisse/Prüfung bleiben unverändert.

## Echte Collider

- Neue Runs erhalten eine begrenzte Aufnahmespur tatsächlicher unterstützter Spieler-/Monster-/Kristall-Collider: Box, Kugel, Kapsel und CharacterController, einschließlich geänderter Maße, Transformationen und enabled/active/trigger-Zustände.
- Bestehende Quellen/Kataloge werden wiederverwendet. Spieler: Physikbeobachtung; andere Quellen bis 10 Hz mit maximal 16 Beobachtungen pro Kamera-Aufruf. Keine Datei-IO und keine zusätzlichen Szenensuchen im Aufnahmepfad.
- Formen werden getrennt und optional durch Wände angezeigt. Vorhandene Accurate-Collider-Zeichenlogik wird geteilt. Mesh/Terrain werden nicht durch ungefähre Boxen ersetzt.
- Unterstützte solide Level-Collider werden einmal gesammelt und als geladene Szene gekennzeichnet. Historische bewegte Collider fehlen bei alten Aufnahmen und werden dort nicht erfunden.

## Kompatibilität

- Hauptprotokoll 18: DLL und EXE gemeinsam ersetzen. Commands 192 Byte, Telemetrie 16516 Byte.
- Map-Payload 12 und Settings 15 bleiben erhalten. Neue Eingabedateien Format 2; bisheriges Format 1 bleibt lesbar.
- .ismarks ist eine getrennte editierbare Begleitdatei. Pakete mit Markierungen verwenden Format 2; unveränderte Pakete ohne Markierungen weiterhin 1. Exporter 7.2.0/7.2.1 werden weiterhin importiert.
- Bestehende Route-, Flash-, Seed-Suche-, Bewegungs- und Validierungssysteme bleiben erhalten. Die Monster-/Flash-Aufnahme erhielt ausschließlich einen zusätzlichen Collider-Quellenanschluss.

Details und tatsächliche Prüfumfänge: PRUEFUNG.md / tests/results-7.3.0. Keine Live-FPS- oder Unity-/Treiber-Prüfbehauptung auf diesem Host.
