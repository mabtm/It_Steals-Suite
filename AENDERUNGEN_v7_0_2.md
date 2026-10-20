# PracticeSuite 7.0.2 / mab

Der Finish-Screen hat eine vollständige Prüfsequenz: sieben einzeln abgearbeitete Zeilen, animierte Fortschrittssegmente und wechselnde technische Angaben aus dem eingefrorenen Run. Nach 3,32 sichtbaren Sekunden erscheinen grüne [VALID]- bzw. rote [INVALID]-Tags mit weißen Beschriftungen. Fehlende erforderliche Nachweise sind ungültig. Die Animation zeigt vorhandene Run-Beobachtungen und löst keinen zusätzlichen Prozess-/Speicherscan aus.

Finale Zeit, Modus, tatsächliche vorherige PB-Differenz, Seed und Copy seed bleiben enthalten. Ein einzelner leiser Abschlusston verwendet die vorhandenen Audio-Einstellungen und Fokusregeln. Ohne Animationen werden alle Prüfungen sofort gezeigt. Ausblenden pausiert den Ablauf. Doppelte Renderzeitpunkte können einen abgeschlossenen Scan nicht wieder auf einen Zwischenzustand setzen.

Analyse Run und Replays sitzen jetzt direkt im Ergebnisfenster. Kopfzeile/X und Aktionen bleiben bei kleiner Fläche fest stehen; der Mittelteil scrollt. Die technischen Angaben verwenden eine eigene native Schrift, unabhängig von der HUD-Skala. Das Fenster bleibt bis zur bewussten Aktion erhalten, auch nach der automatischen Endszene.

Analyse Run ist unabhängig von der 15-Sekunden-Einladung verfügbar, sofern die genaue Aufnahme existiert. Die Zuordnung prüft Run-ID und Modus; das Archiv zusätzlich Zeit, Abschlussart und bekannten Layout-Seed. Verbindungs-/Sitzungswechsel können über die passende Archivaufnahme aufgefangen werden. Fehlende oder inzwischen verworfene Aufnahmen ergeben einen deaktivierten Knopf mit Grund. Die Analyse öffnet niemals ersatzweise den neuesten Versuch. Speedrun mode bleibt eingeschaltet; während aktiver Runs bleibt Replay gesperrt.

Der Helper veröffentlicht die Replay-Metadaten nun für die exakte aktuelle Run-ID. Wenn dieser Versuch keine Aufnahme hat, meldet er diese ID ohne Bereitschaft, statt die vorherige Aufnahme anzuhängen. Ältere vorhandene Aufnahmen werden ebenfalls gezielt gefunden. Protokoll 16, Settings 15 und Karten-/Replay-Format 11 bleiben gleich.

Intro aus 7.0.1 bleibt erhalten. Routing, Seed-Suche, Movement-Lernen, Monster-/Flash-Aufzeichnung, Freecam und Collider bleiben unverändert. Hide & Seek / Outside bleiben WIP für Karten und Seed-Suche. DLL und EXE gemeinsam ersetzen; Einstellungen und alte Aufnahmen bleiben erhalten. Ausgeführte Prüfungen und Grenzen: PRUEFUNG.md.
