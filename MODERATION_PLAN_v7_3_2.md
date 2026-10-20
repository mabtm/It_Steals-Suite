# Moderation / Verifikation: nächste Schritte

Entwurf zu 7.3.2 / mab. Die folgenden Erweiterungen sind Vorschläge; sie sind in dieser Version nicht implementiert.

## Was die aktuelle Suite tatsächlich prüft

RunValidity / SpeedrunManager prüfen beobachteten Start, Random-Seed-Abdeckung, Trainer-Hilfen, verbotene Befehle, fehlende Zustandsbeobachtung, Zeitmaßstab, HUD-Präsenz, ausgewählte Spielerzustände und bekannte Gameplay-Patch-Owner. ExternalToolMonitor beobachtet bekannte Prozessnamen und einen am Spiel hängenden Debugger einmal pro Sekunde. GameBuildInfo hasht die Spiel-Assembly auf Datenträger. Replay-Decoder prüfen Format, Zeitfolgen, Anzahlen, Wertebereiche und Konsistenz zwischen Map/Eingabedaten; Paketprüfung ergänzt CRC/SHA-256 und Identität. Originaldateien werden von der Suite bei Notiz-/Markeränderungen erhalten.

Das beweist keine manipulationsfreie Spielausführung. Prozessnamen können verborgen werden, ein erkanntes Programm beweist keine Änderung dieses Runs. Dateihash entspricht nicht laufendem Speicher. Lokal bearbeitbare Flags/Hashes können neu erzeugt werden. Die Replay-Aufnahme ist Beobachtung, keine vollständige deterministische Simulation des Spiels.

## 1. Unabhängige Nachprüfung der Originaldateien

Ein Prüfer auf dem Rechner des Moderators soll Original-Streams erneut untersuchen, ohne dem gespeicherten VALID-Feld zu vertrauen. Regeln erhalten eine Version und eindeutige Codes. Beobachteter Start/Ende, monotone Kamera-/Physikzeiten, Eingabelücken, Zustandswechsel, Aufnahmequalität und Pickup-Zählung können gegeneinander geprüft werden. Bewegungskonsistenz soll das bestätigte Spielmodell und gemessene Zeitabstände benutzen. Abweichungen brauchen Zeitintervall, Messwert, erwartete Grenze und Sprung zur betreffenden Replay-Stelle.

Regel-Gültigkeit, vollständige Aufzeichnung und Moderatorentscheidung bleiben getrennte Ergebnisse. Ein Stotterer oder fehlender Datensatz ist ein Prüfproblem und kein automatisches Cheater-Urteil.

## 2. Kleine verpflichtende technische Auditspur

RunTracker.Event ist aktuell an ExportReport gekoppelt und auf 128 Ereignisse begrenzt. Ein knapper Kernverlauf sollte für verifikationsfähige Aufnahmen immer verfügbar sein: erster verbotener Befehlsversuch, tatsächlicher Hilfsfunktionswechsel, TimeScale-/Spielerwertänderung, Hook-Abweichung, HUD-/Beobachtungslücke und Scanstatus. Je Ereignis monotone Run-Zeit, Code, Quelle und nötige Vorher-/Nachher-Werte. Zusätzliche verbose Reports bleiben optional. Puffer, Größenlimit und Hintergrundschreibpfad schützen Performance; Überlauf bleibt ausdrücklich sichtbar.

## 3. Vertrauenswürdiger Bezug zu einer veröffentlichten Version

Der Moderator kann Hashes von Suite-Binaries, Game-Assembly, erlaubten Mods und Regelwerk gegen ein separat signiertes Release-Manifest prüfen. Dies bestätigt Dateiversionen, nicht den kompletten Prozessspeicher. Das Manifest wird von einer vertrauenswürdigen Veröffentlichung signiert; der Moderator benötigt nur den öffentlichen Prüfschlüssel. Die bestehenden gemeinsamen DLL-/EXE-Kanalversionen bleiben eine Kompatibilitätsprüfung.

## 4. Optionaler externer Aufnahmenachweis

Für stärkere Nachvollziehbarkeit: verkettete Hashes von Aufnahmeblöcken mit Sequenznummern und optionalen unabhängigen Serverquittungen. Ein externer Start-Challenge-Code kann Run-ID, Aufnahme und sichtbare Videoeinblendung verbinden. Die Verkettung allein ist lokal neu berechenbar; erst unabhängige Anker belegen, welche Daten zu welchem Zeitpunkt vorlagen. Auch ein Empfangsnachweis bestätigt weder physische Tastendrücke noch saubere Ausführung eines möglicherweise veränderten Clients. Unterbrechungen und Offline-Runs müssen klar erkennbar bleiben.

## Priorität

Zuerst unabhängiger Replay-Prüfer und minimale Auditspur. Beide liefern konkrete Moderationshinweise ohne Infrastrukturpflicht. Versionsnachweise ergänzen die Herkunft; externe Quittungen sind eine spätere, ausdrücklich optionale Stufe. Keine automatische Sanktion aufgrund einer einzelnen Prozessliste oder eines isolierten Ausreißers.
