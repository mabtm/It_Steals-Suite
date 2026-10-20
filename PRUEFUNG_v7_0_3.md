# Prüfung 7.0.3 / mab

20 Prüfgruppen erfolgreich: mod, trainer, managed, native, ui, planner, search, settings, portable, playback, hide-seek-player, replay, layout, movement-managed, movement-native, hud-adapter, experience, audio-adapter, intro-adapter und milestone. Vollständige Logs: tests/results-7.0.3. Native GUI-Vorschauen: tests/previews-7.0.3. Frühere Berichte bleiben als PRUEFUNG_v7_0_2.md / PRUEFUNG_v7_0_1.md erhalten.

## Neue technische Ansicht

Die produktive ImGui-Oberfläche wird zu 21 Zeitpunkten der 5,04 Sekunden langen Sequenz ausgeführt und mit ihrem tatsächlichen Font-Atlas gerendert. Visuell geprüft: laufender Scan, abgeschlossene Build-Ansicht mit beiden vollständigen Hash-Hälften, ungültiger Abschluss und kompakte Detailansicht bei 480×360 / 150 % DPI. Synthetische Run-Daten, Host-Schrift DejaVu Sans; keine Windows-/Unity-Liveaufnahme. Windows verwendet die ausgewählte verfügbare Systemschrift.

FinishVerificationCases führt echte Mausauswahl einer Prüfzeile und Tastaturaktivierung von Copy report / Follow scan aus. Die Auswahl bleibt fest, Automatik kehrt zurück; vollständiger Datei-Hash, exakte Run-ID und 42 Felder erreichen die Clipboard-Schnittstelle. Ergebnis, Speedrun mode und Replay-Befehle bleiben dabei unverändert. Nach Ende aller Prüfungen wird der Build ausgewählt; bei Fehlern der erste fehlgeschlagene Check.

Der kompakte Technical scan-Knopf öffnet bei 480×360 und 100/150 % DPI eine begrenzte Detailansicht. Kopieren bleibt erreichbar; Schließen der Details bestätigt nicht das Ergebnis. Bestehende Analyse-/Replay-/X-Aktionen und ihre festen Positionen werden weiterhin bei 480/1920 und 100/150 % DPI bedient. Die breite Ansicht berücksichtigt belegte HUD-Flächen.

milestone_tests.cpp prüft 42 Felder, unveränderte Check-Prädikate, monotone 0–42-Eintragsdarstellung, korrekte UTC-Ticks-Umrechnung, fehlende Belege, begrenzte nicht terminierte Snapshot-Strings, vollständigen Bericht, Auto-Auswahl, Pinning, Reset und Cache-Aktualisierung. UTC-Zeitspanne ist ausdrücklich kein Spieltimer und beeinflusst die Gültigkeit nicht. Captured-Felder sind Momentaufnahmen. Der Datei-Hash identifiziert die Datei auf dem Datenträger; die Ansicht attestiert keinen laufenden Speicher.

## Builds und erhaltene Regressionen

Windows-Helper gegen bereitgestellte Game-/Unity-/MelonLoader-/Harmony-Signaturen, 64-Bit-Trainer mit MinGW C++20. Warnungen als Fehler. Protokoll 16, Commands 136 Byte, Telemetrie 16292 Byte, Settings 15 und Karten-/Replay-Format 11 bleiben unverändert. Acht native Schriften; die technische 16-px-Schrift ist von der optionalen HUD-Skala unabhängig.

144 Tab-/Viewport-/DPI-/Verbindungsfälle, 216 HUD-Konfigurationen, 128 Mode-Regeln und 288 Minimap-Ansichten bleiben erfolgreich. Ergebnis bleibt nach automatischer Endszene und einer Stunde erhalten; X stellt die vorherige Ansicht/Eingabe wieder her. Analyse öffnet die exakte Run-ID auch nach Ablauf des Hinweises und über das Archiv, während aktiver Runs bleibt sie gesperrt. Intro wartet auf Bereitschaft und erfolgreiche Erstpräsentation, pausiert verborgen und unterstützt reduzierte Bewegung.

Seed-/Navigation-/Ranking-Pipeline, tatsächliche Boden-Fixtures, Outside, Route-/Movement-Lernen, Monster-/Flash-Playback, ABORTED-Markierung, Aufzeichnungsregel und Hintergrund-Persistenz bestehen ebenfalls. Native Logik-/Adapterprüfungen verwenden ASan/UBSan; LeakSanitizer ist auf diesem Host deaktiviert. SDK-Tests unter Mono verwenden Unity-Test-Doubles; bekannte Native-Stub-/Runtime-Hinweise bleiben im Log sichtbar.

13 zentrale Routing-/Movement-/Light-/Freecam-/Collider-Dateien sind bytegenau zum gesicherten 7.0.2-Paket unverändert (tests/results-7.0.3/core-preservation.json). Die sieben Check-Prädikate und FinalValid werden zusätzlich unverändert verglichen. Die technische Ansicht erzeugt weder zusätzliche Game-Telemetrie noch einen neuen Prozess-/Speicherscan. Zeichenketten werden pro Ergebnis aufgebaut und zwischengespeichert, der Bericht nur auf Kopieraktion formatiert. Bericht und Darstellung ändern keine Run-Daten oder Archivdateien.

## Grenzen und Installation

Kein laufendes Windows-/Unity-Spiel auf diesem Host. Builds und GUI-/Logik-/Dateipfade sind geprüft; reale Fokus-/D3D-/Audio-Treiber, Zustellung im Spiel und Spiel-FPS nicht live getestet. Lokale Run-Beobachtungen sind keine manipulationssichere Anticheat-Attestierung. Hide & Seek / Outside bleiben für Karten und Seed-Suche WIP. Die übrige GUI wurde in diesem Update nicht umgestaltet.

DLL und EXE gemeinsam ersetzen. Einstellungen und vorhandene Replays bleiben erhalten. Keine originalen Spiel-/Loader-DLLs enthalten. SHA256SUMS.txt / SHA256.txt enthalten die ausgelieferten Datei-Hashes.
