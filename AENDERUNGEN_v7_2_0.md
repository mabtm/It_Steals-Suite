# PracticeSuite 7.2.0 / mab

- Neue Eingabeaufnahme neben der bestehenden Routenaufnahme: WASD, beide Shift-Tasten, R, tatsächlich gelesene Bewegungs-, Maus- und Joystickachsen. Radar-Anforderung und tatsächlicher Radar-/Sprintzustand bleiben getrennt.
- Spieler-/Kamerapositionen, vollständige Rotationen, FOV und tatsächliche Lichtposition, Rotation, Typ, Farbe, Reichweite, Winkel und Intensität. Rendernahe Kameraproben, eigene Physiksequenzen und Ereigniszeiten teilen dieselbe Run-Uhr.
- Unabhängige, begrenzte `.isinput`-Datei. Sequentielle Hintergrundspeicherung mit niedriger Threadpriorität; keine Dateioperationen in Eingabehooks und keine großen Datenströme im Map-/Telemetriekanal.
- Replay-Eingabeanzeige, Blickwinkel, Frame-Schritte und anwählbare Eingabeereignisse. Fehlende Daten und Grenzen sind sichtbar. Ältere Aufnahmen behalten ihre bisherigen Funktionen.
- ZIP-Export/-Import kompletter Run-Pakete mit Originalaufnahme, Eingabedaten, Notizen, ursprünglichem Validierungsbericht, Versionen und SHA-256-Liste. CRC, Größen, Run-Zuordnung, Kamera-/Ereigniszeiten und Metadaten werden geprüft. Vorhandene Aufnahmen werden bei Konflikten bewahrt.
- Aktuelle Prozess-/Transportdaten dürfen von der gespeicherten Originaldatei abweichen: Zuordnung prüft die unveränderlichen Aufnahmedaten. Exporte und gespeicherte Kopien verwenden die vorhandene Originaldatei.
- Neue **3D preview (WIP)** im Originallevel: tatsächliche Kamera und Lichtdaten, freier Beobachter, synchronisierte Steuerung und passiver Wiedergabehinweis. Renderobjekte tragen keine Spielskripte oder Collider. Spawn und Kristallpositionen werden vor Beginn geprüft. Marker zeigen vorhandene Monster-/Kristalldaten; vollständige Animationen und Weltgeschichte fehlen.
- Aktive Runs sperren 3D-Wiedergabe. Aufbau ist begrenzt, Datei-Laden läuft im Hintergrund, Abbruch/Trennung/Szenenwechsel/Fehler führen zur Wiederherstellung. Escape wird beim Verlassen der Vorschau für den Übergangsframe abgefangen.
- Auf Performance ausgerichtete Analyseansicht: geteilte Original-Meshes mit erhaltenen Transform-Hierarchien, einfache gemeinsame Materialien, Grundlicht plus aufgezeichnetes Licht, keine Texturen/Schatten/HDR/MSAA/Nebel. Zusätzliche Kartenscans pausieren während der Wiedergabe; originale Kamera-/Lichtzustände werden danach wiederhergestellt.
- Replay-Bedienung scrollt unabhängig von der großen Karte. Diagnosebericht enthält 3D-Zustand und Fehlermeldung.
- Version/Autor überall **7.2.0 / mab**. Hauptprotokoll **17**, Commands **164 Byte**, Telemetrie **16472 Byte**, Map-/Replay-Payload **12**, Eingabedatei **1**. Einstellungen bleiben **15**; bisherige Daten werden nicht migriert/gelöscht.

Navigation, Seed-Rangfolge, bisherige Flash-Erkennung, Bewegungslernen und lokale Gültigkeitsregeln bleiben erhalten. Hide & Seek / Outside bleiben für Minimap und Seed-Suche WIP. Lokale Aufnahmen und unsignierte Hashes unterstützen Moderation; sie beweisen keine physischen Tastendrücke oder Manipulationsfreiheit.

Prüfnachweise und Grenzen: PRUEFUNG.md. Datenformat: REPLAY_FORMAT.md.
