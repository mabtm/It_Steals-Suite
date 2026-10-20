# Prüfung 7.0.2 / mab

20 Gruppen erfolgreich: mod, trainer, managed, native, ui, planner, search, settings, portable, playback, hide-seek-player, replay, layout, movement-managed, movement-native, hud-adapter, experience, audio-adapter, intro-adapter und milestone. Vollständige Logs unter tests/results-7.0.2; gerenderte UI-Ausgaben unter tests/previews-7.0.2. Die früheren Prüfungen und Vorschauen bleiben erhalten.

## Neuer Finish-Screen

FinishVerificationCases in ui_tests.cpp führt den produktiven Bildschirm zu 18 Zeitpunkten aus. Die sieben Prüfungen gehen nacheinander von QUEUED über SCANNING zu VALID/INVALID. Die Gesamtdauer von 3,32 sichtbaren Sekunden, Abschlusszustand, weiße Beschriftungen und grüne/rote Statusfarben werden geprüft; Draw-Dateien einschließlich Font-Atlas werden mit dem mitgelieferten Renderer gerendert und visuell gesichtet. Die Vorschauanimation zeigt genau diese Zeichenausgaben mit synthetischen Daten. Ohne Animationen stehen sämtliche Prüfungen sofort fest.

Die reine Zeitleiste prüft Vorlauf, jede Reihenfolgegrenze, abgeschlossenen Zustand, einmaliges Abschlusssignal, Pause während ausgeblendeter Anzeige, rückwärts laufende Zeit und identische Renderzeitpunkte. Wiederholtes Rendern zum gleichen Zeitpunkt darf den fertigen Zustand nicht zurücksetzen. Der produktive Hauptloop übergibt den tatsächlich sichtbaren/nicht laufenden Anzeigezustand.

Technische Angaben kommen aus dem eingefrorenen Snapshot: tatsächlich gesetzte Hilfen-/Warnmasken, Seed-/RNG-Status, Startbeobachtung, gestoppte Zeit, Aufnahmesamples, Build-Hash, Spiel-/Unity-Version und lokaler Tool-/HUD-/Befehlsstatus. Kein zusätzlicher Prozess-/Speicherscan läuft für die Animation. Begrenzte, auch nicht terminierte Teststrings werden ohne Lesen über Feldgrenzen verarbeitet. Ein später fertiggestellter Replay-Status darf nur zur selben Run-ID, Abschlusszeit und Sitzung ergänzt werden; Zeit und Integritätswertung bleiben unverändert. Alle 32 Warnbits werden weiterhin geprüft; fehlende erforderliche Nachweise werden nicht grün.

## Ergebnisaktionen und Replay-Identität

| Fall | Tatsächlich ausgeführter Nachweis |
|---|---|
| Analyse nach Hinweisablauf | Eine Stunde später bleibt der feste Knopf nutzbar und öffnet exakt die eingefrorene ID. Speedrun mode, breite Replay-Ansicht und Tab-Auswahl bleiben korrekt. |
| Analyse während Animation | Echter Maus-Klick vor Ende der Scan-Sequenz in 480×360 und 1920×1080, jeweils 100/150 % DPI. X und Aktionszeile bleiben außerhalb des scrollenden Mittelteils. |
| Automatische Endszene | Nach Scene-/Epoch-Wechsel öffnet die Aktion die Aufnahme des ursprünglichen Spielmodus, ohne einen neueren Versuch einzusetzen. Der kurze F8-Hinweis endet weiterhin beim Szenenwechsel. |
| Archiv statt Helper | Drei Aufnahmen werden durch den echten Archiv-Worker geschrieben, dekodiert und eingelesen. Bei neuer Helper-Sitzung mit abweichender letzter Aufnahme muss die mittlere der drei IDs geöffnet und tatsächlich geladen werden. |
| Fehlende/falsche Daten | Aktiver Run, ausstehender Befehl, fehlende ID, andere Szene/Sitzung, abweichende Zeit oder bekannter anderer Layout-Seed öffnen keinen Ersatz-Run. Fehlende Aufnahme deaktiviert die sichtbare Aktion mit Grund. Passendes Archiv bleibt auch ohne Helper zugänglich. |
| Weitere Aktionen | Tastaturaktivierung öffnet Replays und erhält Speedrun mode. Copy seed kopiert exakt den erfassten Layout-Seed in die Test-Clipboard-Schnittstelle, ohne Aufnahme oder Modus zu wechseln. |
| Helper-Metadaten | Reale RunReviewRecorder-/RunStore-Pipeline meldet aktuelle, ältere und fehlende ID sowie zur Szene unpassende Aufnahmen. Ohne Aufnahme bleibt die aktuelle ID ohne ReviewReady erhalten; Last wird nicht ersatzweise verwendet. |

Die separaten bisherigen PersistentFeedbackCases prüfen weiterhin automatischen Endszene-Wechsel und eine Stunde Laufzeit, echte Maus-/Tastaturbedienung des X, vorherige Menü-/Eingabeansicht, 480/1920 und 100/150 % DPI sowie aktive-Run-Sperre. Das Intro wartet weiterhin auf Bereitschaft und erfolgreiche Erstpräsentation, pausiert verborgen und unterstützt reduzierte Bewegung sowie den expliziten Aus-Schalter.

## Builds und Regressionen

Helper-DLL gegen bereitgestellte Game-/Unity-/MelonLoader-/Harmony-Signaturen, 64-Bit-Windows-Trainer mit MinGW C++20; Warnungen als Fehler. Acht native Schriften im ImGui-Atlas; eigene 16-px-Ergebnisdetails bei 100 % DPI, unabhängig vom optionalen HUD-Maßstab. Wiederholter Atlaswechsel und 24 echte Settings-Unterseiten bleiben geprüft. Protokoll 16, Commands 136 Byte, Telemetrie 16292 Byte, Settings 15 und Karten-/Replay-Format 11 sind unverändert.

144 Tab-/Viewport-/DPI-/Verbindungsfälle, 216 HUD-Konfigurationen, 128 Mode-Regeln und 288 Minimap-Ansichten sowie die bestehende Seed-Suche, Navigation, Ranking, Movement-Profile, Replay-Routen, Monster-/Flash-Toggles und orange ABORTED bestehen weiterhin. Reale Recorder-Aufzeichnungsregeln (10/10,001 Sekunden, Kristallschwelle, eingefrorene Regel, temporärer Neustart-Verwurf), Speicher-/Settings-/Replay-Hintergrundarbeiter, beschädigte Dateien/Backups und Dateiidentitäten bleiben geprüft. Native Logik und Windows-/Audio-/Intro-Adapter verwenden ASan/UBSan; LeakSanitizer ist auf diesem Host deaktiviert.

13 zentrale Routing-/Movement-/Light-/Freecam-/Collider-Dateien sind zusätzlich bytegenau identisch zum gesicherten 7.0.1-Paket. Vergleich: tests/results-7.0.2/core-preservation.json. Dies ergänzt die Regressionen und ersetzt keinen Spieltest.

## Grenzen

Kein laufendes Windows-/Unity-Spiel auf diesem Host. DLL/EXE sind gebaut; reale D3D-/Fokus-/Audio-Treiber und tatsächliche Spiel-FPS sind hier nicht live geprüft. Die native GUI-Ausgabe verwendet synthetische Run-Daten und die Host-Schrift DejaVu Sans; Windows lädt die gewählte verfügbare Systemschrift. Die SDK-Tests unter Mono verwenden Unity-Test-Doubles; die bekannten Native-Stub-/Runtime-Hinweise bleiben im Log sichtbar.

Die animierte Liste präsentiert lokale Run-Beobachtungen und ist keine neue oder manipulationssichere Anticheat-Prüfung. Hide & Seek / Outside bleiben für Karten und Seed-Suche WIP. Aufzeichnungsregel und bestehende Replays bleiben erhalten; außerhalb der Suite sind Dateien nicht gegen Löschen/Ändern geschützt. Intro, Navigation, Lernmodell, Flash-Aufzeichnung, Freecam und Collider wurden für dieses Update nicht umgebaut.

Installation: Helper-DLL und Trainer-EXE zusammen austauschen. Keine originalen Spiel-/Loader-DLLs im Paket. Hashes der ausgelieferten Dateien stehen in SHA256SUMS.txt und SHA256.txt.
