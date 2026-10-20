# Live-Inspector-Verbindung 3 – PracticeSuite 7.11.2 / mab

Zusätzlich zum Hauptprotokoll 29; Commands und Haupttelemetrie behalten ihre 192 / 16952 Byte. Die Inspector-Verbindung ist nur nach explizitem RequestsInspector-Bit 2048 aktiv und wird im Helper unabhängig gegen Clean Speedrun und Replay/Export geprüft. Die Verbindung ist lokal; Prüfsummen ersetzen keine vertrauenswürdige Attestierung.

## Gemeinsamer Speicher

Name: ItStealsInspector.v3.<PID>, Mutex mit Suffix .Lock. Kapazität 5259520 Byte; alle Zugriffe nach Initialisierung per WaitOne/WaitForSingleObject(0). Unity wartet nicht. Bei beschäftigtem Mutex bleibt die letzte Veröffentlichung bestehen. Aktuelle Hauptkanal-Session und PID müssen passen.

| Offset | Inhalt |
| --- | --- |
| 0 / 4 / 8 / 12 | UInt32 Magic 0x504e5349, Version 3, Session; Int32 PID |
| 32 / 36 | Int32 Kataloglänge, UInt32 Publikationsrevision |
| 40 / 44 | Int32 Detaillänge, UInt32 Publikationsrevision |
| 48 / 52 | Int32 Hoverlänge, UInt32 Publikationsrevision |
| 64 / 68 / 72 / 76 | UInt32 Befehlsserial; Int32 Aktion / Objekt-ID; UInt32 Optionen |
| 80 / 84 / 88 / 96 | Float32 normiertes X/Y; UInt32 Szene-Epoch; UInt64 Feldschlüssel |
| 128 / 132 / 136 | UInt32 Pointer-Serial / Szene-Epoch / aktiv (0 oder 1) |
| 140 / 144 / 148 / 152 / 156 | Float32 Pointer X/Y; Int32 Spielclientbreite/-höhe / Aktion |
| 160 | Int32 bevorzugte Objekt-ID, 0 = erstes Ziel |
| 256 | Katalogbereich, 4194304 Byte |
| 4194560 | Detailbereich, 1048576 Byte |
| 5243136 | Hoverbereich, 16384 Byte |

Befehlsserial wird nach allen Feldern geschrieben. Nicht-Null, nur ein offener Befehl. Reconnect vermeidet die zuletzt sichtbare Serial; ACK wird erst nach tatsächlichem Senden angenommen. Detail enthält ACK-Serial, ACK-Aktion und Erfolg; nur ein passender erfolgreicher Fokus-ACK darf das Menü schließen. Timeout bleibt sichtbar und schließt nicht ungewollt. Neue Scene-Epoch muss passen. Optionen: 1 echter Collider-Wire, 2 erfasste Konturen, 4 visuelle Renderer-Auswahl, 8 Trigger einbeziehen, 16 ausführliche Detailabfrage, 32 Physics-Auswahl auslassen. Standard ist 12: Visuals + colliders einschließlich Trigger. Ohne 4 sind nur Collider aktiv; 4+32 beschränkt auf Visuals. Ohne 16 werden nur Grundwerte und ausdrücklich beobachtete Felder gelesen.

Aktionen: 0 Optionen, 1 Katalog-Refresh, 2 Objekt-ID wählen, 3 bestätigte Hover-Auswahl, 4 Feld-Key Watch togglen, 5 Auswahl in vorhandener Freecam fokussieren, 6 Watches leeren, 7/8 Probe-A/B wählen, 9 Auswahl löschen. Float-Werte müssen endlich und innerhalb [0,1] sein. Y läuft unten→oben.

Pointer-Serial wird nach dem restlichen Pointer geschrieben, unabhängig von Befehlen. Der Helper liest alle Felder unter demselben Mutex. Ein geklickter Pointer bleibt eingefroren bis Commit oder Abbruch. Preferred wird vor Pointer-Serial geschrieben und bindet dieselbe Abfrage an eine konkrete Zielebene. Unbekannte Preferred-ID ergibt keinen Nachbarersatz. Pointer-Koordinaten beziehen sich auf den Spielclient, Y unten→oben, nicht auf Screen.width. Breite/Höhe dienen Diagnose/Validierung; Camera.rect und ViewportPointToRay definieren die Abfrage. Aktionswert 3 ist normale Auswahl, 7/8 Probe. Inaktive Pointer und ein 0,5-s-Heartbeat-Timeout entfernen Hover. Normaler Hover ist auf 30 Hz begrenzt.

Für Aktion 3 enthält Objekt-ID das erwartete Ziel, UInt64 key die erwartete Pointer-Serial. Der Helper übernimmt nur dieselbe frische Beobachtung und prüft den Treffer erneut. Geänderte Ziele werden abgewiesen. Route-Probes verwenden dieselbe Query, feste Collider und ihre tatsächlich beobachteten Oberflächenpunkte.

## Publikationen

Little endian, keine Pointer. Header: UInt32 Magic, Int32 Version/Kind (1 Katalog, 2 Detail, 3 Hover), Int32 PID, UInt32 Session, Int32 Szene, UInt32 Epoch/Revision, Int64 UTC-Ticks, Float64 Unity-Realtime, Int32 Unity-Frame. Danach Body und abschließender UInt64 FNV-1a über sämtliche vorherigen Bytes.

Text: Int32 UTF-8-Bytelänge und Bytes ohne NUL. Bool: Byte 0/1. Vec3: drei Float32. Counts, endliche Werte, Textgrößen, UTF-8, Zeitordnung und Identität werden im Trainer geprüft, nicht nur die Prüfsumme. Laufzeit-IDs können negativ sein.

Katalogbody: Szenenname, Bool partial, Int32 Zahl; je Objekt Int32 ID / Parent-ID / Szene / Layer, UInt32 Flags (activeSelf 1, activeInHierarchy 2, Collider 4, Renderer 8), Text Name. Maximal 20000 eindeutige IDs, Paket zusätzlich bytebegrenzt. Die IDs müssen nicht alle Parent-IDs enthalten; Szenenüberlagerungen oder Teilkataloge sind möglich.

Detailbody und genaue Schreibreihenfolge stehen in LiveInspector.PublishDetail sowie InspectorModel.DecodeDetail; beide werden mit C#-Fixtures gegeneinander geprüft. Inhalt: Aktivität, ACK-Ergebnis, ausgewählte ID / Status / Name / Hierarchie / Pickingquelle, Scan-/Kosten-/Limitdaten, Kamera, Werte, beobachtete Änderungen, Raycast-Treffer, Probe-Endpunkte und Collider mit realen Erfassungsdaten/Konturteilen. Maximal 512 Werte, 8 Watches, 128 Änderungen, 32 Treffer, 32 Collider, 512 Teile, 4096 Konturpunkte; je Kontur 2–64 Punkte. Ausgelassene Einträge und Teilkonturen bleiben kenntlich. Wertschlüssel ist FNV64 von componentID + '/' + vollständigem Feld-/Engine-Quellnamen.

Ein Katalog erhält eine Veröffentlichung nach dem Refresh; selektierte Details höchstens 5 Hz, zusätzliche direkte Befehlsbestätigung bei Bedarf. Watches sind unabhängig pro beobachtetem Spiel-Frame; ihr Ring ist zusätzlich auf 180000 Byte begrenzt. Übergänge innerhalb eines Frames und kausale Aufrufstellen sind nicht Teil des Protokolls.

Hoverbody: UInt32 Pointer-Serial; Bool aktiv; Int32 Objekt-ID / Art (0 kein Treffer, 1 Physics, 2 Bounds estimate, 3 Mesh surface, 4 Sprite surface); Float32 X / Y / Trefferdistanz; Bool Outline; Float32 links / oben / rechts / unten (normiert, oben→unten); Text Name (768) / Typ (256) / Status (1024); Int32 Kamera-ID; Text Kamera (512); Int32 Clientbreite/-höhe; Float32 Camera.rect X/Y/Breite/Höhe. Danach Bool partial, Bool scanning, Int32 Zielanzahl (0–32); je Ziel Int32 ID / Art, Float32 Distanz, Text Name (256) / Typ (128). IDs sind eindeutig und ungleich 0; Art ist 1–4, Distanz endlich in [0,10000]. Ein ausgewähltes Ziel muss mit Art/Distanz in der Liste vorkommen. Bei fehlender bevorzugter Ebene darf ein aktives Hover ohne Auswahl die übrigen Ziele listen. Inaktive Hover enthalten keine Ziele. Gleicher Header/Checksum wie die anderen Publikationen. Kein Treffer ist ein explizites Ergebnis, keine Auswahl. Der Rahmen projiziert Bounds einschließlich Near-Plane-Schnittkanten und ersetzt keine echte Collider-Drahtgeometrie. Physikpuffer: 512 Treffer; Sättigung ist Fehler. Spielerkörper wird an der Spielkamera übersprungen; optional Trigger, Reichweite 10000 u. Visuelle Abfragen enthalten mehrere Ebenen und verwenden kein Textur-Alpha; Grenzen und Cache in LIVE_INSPECTOR.md. Route-Probe-A/B bleiben ausschließlich physisch und verwenden unverändert die gemeinsame Bodenlogik.

## Diagnose-JSON

Format 1, UTF-8. Enthält beobachteten Zeitpunkt, Szene/Session, Objektpfad/IDs, Kamera, version/author/build, Seed-Art mit null bei unbekanntem Seed, Rohwerte und Herkunft, tatsächliche Geometriegründe, Erfassungsprofil, Konturen, Änderungsring, optionale Baseline-Unterschiede/Probe und Diagnose-Text. Inklusive Scope-Hinweis; keine State-Wiederherstellung. Exklusive neue Datei, 4 MiB Größenlimit, keine automatische Übertragung. Siehe InspectorModel.Report/SaveReport.
