# PracticeSuite 7.11.0 / mab

Der Live Inspector ergänzt einen schreibgeschützten Blick auf die laufende Engine. Er ersetzt weder den Unity-Editor noch eine vollständige Spielzustandsaufnahme.

- Objekte direkt per tatsächlichem Physics-Ray auswählen; Solid-/Trigger-Modus, überlappende Treffer und klar beschriftete Renderer-Bounds-Auswahl. Inaktive und colliderfreie Objekte über den durchsuchbaren Katalog auswählen; Eltern/Kinder öffnen.
- Transform, Aktivität, Layer/Tag, primitive Collider-Dimensionen, CharacterController, reale Layer-/Pair-Collision, Rigidbody, Renderer, Shared Materials/Shader/Mesh und private/öffentliche Spielskriptfelder. Keine beliebigen Getter, Methodenaufrufe, Script-ToStrings oder Schreibfunktionen.
- Echte Collider im vorhandenen Drahtgitterrenderer; begrenzte lesbare Mesh-Kanten. Daneben die Konturen aus der vorhandenen Geometrieerfassung, deren reale Aufnahmeentscheidung und deren Spielerprofil/Frame/Zeit. Die Routingregeln aus 7.10.1 bleiben erhalten.
- Acht Watches, begrenztes Änderungsprotokoll und beobachtete Frame-/Zeitangaben. Baseline/Live-Vergleich mit strenger Objekt-, Szenen- und Sitzungsbindung.
- Zwei-Punkt-Routenprobe mit denselben Bodendreiecken und statischen Clearance-Regeln wie die übrige Suite; Roh-/Bodenpunkte getrennt, unbekannte/fehlende Wege ausdrücklich gekennzeichnet. Hintergrundberechnung mit Abbruch bei neuer Szene oder geänderten Endpunkten.
- Lokale JSON-Diagnose mit Build-/Seed-Art, Auswahl, Kamera, Collider-Entscheidungen, beobachteten Änderungen, Baseline und optionaler Routenprobe; exklusive Dateianlage ohne Überschreiben. Unbekannter Seed bleibt null.
- Vorhandene Freecam auf die Auswahl richten, ohne die Spielfigur zu versetzen. Menü wechselt erst nach passender erfolgreicher Bestätigung zurück ins Spiel. Bereits vom Benutzer gestartete Freecam bleibt beim Schließen erhalten.
- Vollständiger Arbeitsbereich mit fünf Tabs, Desktop-Katalog und kompakter Tools-/Objektauswahl; unterstützt die vorhandenen Farben, Schriften und Skalierung. F3/Esc bricht die Spielauswahl ab, F2/F4 handhaben sie ebenfalls.
- Doppelte Sperre im Trainer und Helper für Clean Speedrun und reserviertes 3D-Replay/Export. Eigener RunAids-Bit 512; Aktivierung bleibt in der Run-Validierung erkennbar.
- Begrenzter Katalog, nicht blockierende Verbindung, Feld-Metadaten-Cache, begrenzte Watch-/Mesh-/Konturdaten. Keine Objektlisten- oder Feldabfragen bei geschlossenem Inspector. Ausgewählte GameObject-Referenzen werden beim Schließen freigegeben. Die kleine Erfassungsbegründung entsteht im ohnehin vorhandenen statischen Geometriepass.

Helper und Trainer gemeinsam ersetzen und Spiel neu starten. Hauptprotokoll 27; Commands weiter 192 Byte / Telemetrie 16952 Byte. Inspector-Kanal 1 ist getrennt. Map-Kanal 10, Map/Run 13, Input 6, Frame 4, Audio 1, Clip-Projekt 7 und Encoder unverändert. Alte Replay-Pakete einschließlich 7.10.1 bleiben lesbar. H&S/Outside bleiben WIP. Keine neue Export-/Grafik-/Routenfunktion außerhalb des Inspectors.

Host-Builds, neue C#/C++-Inspector-Tests, ASan/UBSan und vollständige bestehende ImGui-/Routen-/Replay-/Clip-Regressionen wurden geprüft. Kein Live-Windows-/Unity-Nachweis; konkrete Grenzen und Reproduktion stehen in PRUEFUNG.md.

Mesh-Wire-Präzision: Primitive werden live gelesen. Nicht-konvexe lesbare Mesh-Collider zeigen einen begrenzten Source-Mesh-Snapshot vom Auswahl-Frame; bei Topologieänderungen neu auswählen. Ausgetauschte Mesh-Referenzen werden nicht mit alten Kanten gezeichnet. Cooked-Convex-Hüllen sind nicht verfügbar und werden ohne Ersatz ausgelassen.
