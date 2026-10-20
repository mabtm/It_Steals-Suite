# PracticeSuite 7.0.3 / mab

Der Finish-Screen präsentiert 42 technische Felder in einer 5,04 Sekunden langen Sequenz. Auf großen Flächen steht eine eigene Detailspalte neben den Checks. Auf kleineren Flächen ist die Detailansicht über einen festen Technical scan-Knopf am unteren Rand erreichbar. Analyse Run, Replays und X behalten ihre festen Positionen.

Auto-follow folgt der gerade animierten Prüfung. Anklicken einer Prüfzeile hält diese Auswahl fest, Follow scan setzt sie zurück. Nach Abschluss wird zuerst eine fehlerhafte Prüfung gezeigt; bei vollständig gültigen Checks der Build. Ein Evidence stream blendet die zuletzt präsentierten Felder ein. Copy report kopiert sämtliche Werte, vollständigen Datei-SHA256, Run-ID und lokale Ergebnisse.

Die zusätzlichen Daten stammen aus dem eingefrorenen Snapshot: aktive/verwendete Hilfen, RNG, UTC-Ticks, Szene/Epoch, Replay-Identität/Samples, Build-/Helper-Version, komplette Hash-Hälften, lokale Monitor-Codes und ursprüngliche Validierungsbegründung, PID/Sitzung, erfasster Eingabezustand und IPC-Maße. Die UTC-Zeitspanne ist als Wanduhrmessung erklärt und beeinflusst die Gültigkeit nicht. Captured-Felder sind Momentaufnahmen. Datei-Hash und lokale Beobachtung sind keine Attestierung des laufenden Speichers.

Alle Zeichenketten werden begrenzt gelesen und pro Ergebnis zwischengespeichert. Berichtserstellung erfolgt nur auf Kopieraktion. Kein zusätzlicher Prozess-/Speicherscan, keine neue Game-Telemetrie und keine Änderung der Aufzeichnungs-/Gültigkeitsregeln. Intro, Routing, Seed-Suche, Movement, Flash-Aufzeichnung, Freecam und Collider bleiben unverändert. Hide & Seek / Outside bleiben WIP.

DLL und EXE zusammen ersetzen. Protokoll 16, Settings 15, Karten-/Replay-Format 11; bestehende Einstellungen und Replays bleiben erhalten. Nachweise und Grenzen: PRUEFUNG.md.
