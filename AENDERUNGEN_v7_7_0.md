# PracticeSuite 7.7.0 / mab

## Clip-Export

- Explizite, fortlaufende Frame-Zeitstempel, feste Ausgabe-FPS und feste Framezahl für alle externen Videoformate. Die Export-Zeitabbildung wird beim Start aus der eingefrorenen Kameraspur neu aufgebaut. Renderdauer und Videodauer bleiben unabhängig; langsames Rendern lässt keine Frames fallen.
- **Normal speed (1x)** setzt 1x und übergeht Speed-Keypoints, ohne Kamera-/Layerpunkte zu löschen. **Apply speed keypoints** schaltet die Tempospur wieder ein. Der Output-Bereich zeigt Laufzeit und Bildzahl. Eine vorhandene 0,1x-Spur kann weiterhin absichtlich Zeitlupe erzeugen.
- **30 / 60 / 120 FPS** für Video/PNG. 120 FPS interpoliert vorhandene Beobachtungen und erzeugt keine fehlenden historischen Details.
- Wiederverwendete Bildpuffer, begrenzte Hintergrundwarteschlange, größere Pipe-Schreibblöcke; Vorschau-Kopie auf der GPU statt erneutem CPU-Upload. Puffer werden nach Jobende freigegeben.
- Größenlimit in dezimalen **MB / GB**: Szene einmal verlustfrei als UtVideo zwischenspeichern, Zwei-Pass-x264 aus dieser Aufnahme, tatsächliche Dateigröße prüfen. Bei Überschreitung bis zu drei Versuche mit kleinerer Bitrate. Kein Abschneiden des Clips. Ein unmögliches Limit führt zu einer verständlichen Fehlermeldung statt zu einem zu großen oder unvollständigen Ergebnis.
- Temporärer Speicher ist zusätzlich nötig und wird bei Erfolg, Abbruch und Fehlern entfernt. Bestehende Exportdateien werden erhalten. Eine JSON-Begleitdatei beschreibt Quelle, geplante FPS/Framezahl/Laufzeit, tatsächliche Bytes, Exportdauer und Tempospur. Sie ist ein Exportprotokoll, kein unabhängiger Decoder-Prüfbericht; ihr Platz zählt nicht zum Video-Limit.

| Preset | Format / Encoder | Zweck |
| --- | --- | --- |
| Share | MP4 / x264, 1080p60, medium | Gute Qualität und verbreitete Wiedergabe |
| Best Quality | AVI / UtVideo, 1080p60 | Verlustfreie RGB-/Alpha-Pixel, leichteres Decodieren; 30/60/120 wählbar |
| Archive | MKV / FFV1, 1080p60 | Verlustfreie Archivdatei mit Slice-Prüfsummen |
| Editing Master | MOV / ProRes HQ, 1080p60 | Schnittformat, 10-bit 4:2:2; nicht verlustfrei, keine zusätzliche Quellfarbtiefe |
| Size Limit | MP4 / x264, slow, zunächst 25 MB | Vollständiger Clip innerhalb einer festen Dateigröße |
| Quick Preview | MP4 / x264, 720p30, veryfast | Schnelle Vorschau |
| Transparent Overlay | RGBA-PNG, 1080p60 | Freigestellte HUD-/Input-Ebenen |
| GIF Loop | GIF, 640×360, 20 FPS | Kleine animierte Vorschau mit begrenzter Palette |
| Windows MP4 | MP4 / Windows Media Foundation | Alternative mit systemabhängiger Hardware-Unterstützung |
| Custom | Eigene Ausgabe | Freie Kombination zulässiger Einstellungen |

Presets verändern keine Kamerapunkte oder den gewählten Quellbereich. Lossless und eine beliebig kleine maximale Dateigröße lassen sich nicht zugleich garantieren: Das Größenlimit ist deshalb ein eigener verlustbehafteter MP4-Export. UtVideo benötigt einen passenden Decoder. FFV1 kann bei großen Auflösungen/120 FPS trotz korrekter Datei durch Decoderlast ruckeln. Videos bleiben ohne Audio; die Aufnahmen enthalten keinen historischen Audiostream.

## Fullbright

Practice-Fullbright und Replay-Fullbright verwenden dieselbe neue Beleuchtungsverwaltung: neutrales Umgebungslicht, deaktivierte dunkle Lightmaps und Nebel. Originalshader, Texturen, Alpha, Property-Blocks und statische Batch-Bereiche werden nicht ersetzt. Vorherige Beleuchtung wird beim Abschalten, Beenden und nach Export-Frames wiederhergestellt. Neue Renderer werden in Practice höchstens einmal pro Sekunde ergänzt; Replay verwendet die erfasste Renderer-Liste.

**3D preview** und **Clip Studio → Camera** besitzen Fullbright. Ein ausgewählter Kamerapunkt kann in **Scene layers** Fullbright ab seinem exakten Zeitpunkt aktivieren/deaktivieren. Replay-Fullbright verändert weder Originalaufnahme noch Run-Validierung. Practice-Fullbright bleibt ein Cheat/Aid und macht einen Clean Run ungültig.

## Update / Daten

Alle drei Dateien ersetzen: **ItStealsHelper.dll**, **ItStealsTrainer.exe**, **PracticeSuiteEncoder.exe**. Helper in Mods, beide EXE nebeneinander; Encoder-Ordner mit Lizenz/Quellcode behalten. Protokoll **23**; Frame-Kanal weiterhin 2, Eingabedaten weiterhin 4. Clip-Projekte jetzt **ISCLIP 6**, Formate 1–5 werden gelesen. Bestehende Replays/Einstellungen bleiben erhalten.

Der erweiterte separate FFmpeg-/x264-Encoder steht unter GPL 2 oder später. Vollständige zugehörige Quellarchive, Lizenzen, Konfiguration und Build-Skript sind in beiden Paketen enthalten. Keine Originalspielassets werden verteilt.

## Prüfung und Grenzen

Windows-x64-Trainer und Helper gegen die tatsächlichen Game-/Unity-APIs gebaut, Warnungen als Fehler. Native Sanitizer-/Managed-/ImGui-Tests bestanden. 12 unabhängig decodierte Codec-Fälle bei 30/60/120 FPS: korrekte Bildzahl, Reihenfolge, Laufzeit und PTS, keine doppelten Bilder; beide Lossless-Codecs pixelgenau einschließlich Alpha. Drei vollständige Zwei-Pass-Clips unter 0,256 / 1 / 5 MB geprüft. Details und Logs: **PRUEFUNG.md**, **tests/results-7.7.0**.

Die Codec-Lauftests verwenden den aus denselben FFmpeg-/x264-Quellen und Komponenten gebauten Linux-Encoder und einen unabhängigen Decoder. Windows-Prozesssteuerung, D3D11 und Fullbright wurden hier nicht im laufenden Spiel ausgeführt. Die vom Nutzer beobachtete alte MKV-Datei liegt nicht vor; ihre konkrete Zeitlupenursache bleibt unbestätigt. Wand-Fading und Animationsaufzeichnung werden in dieser Version nicht weiter verändert.
