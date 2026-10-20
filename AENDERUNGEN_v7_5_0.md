# PracticeSuite 7.5.0 / mab

## Video-Exporter

- Drei Arbeitsbereiche: **Camera**, **Overlays**, **Output**. Auswahl, Scrubbing, Fit-/100-%-Vorschau und Ausgabe bleiben in einem Fenster.
- Direkte Kamerasteuerung in der Vorschau: **rechte Maustaste halten** zum Drehen, **Mausrad** zum Zoomen, **mittlere Maustaste halten** zum Verschieben. **Center on player** setzt die Ansicht zurück. Die Verfolgerkamera behält ihre eigene Orientierung.
- **Cinematic track**: bis zu 256 benannte Keypoints mit Zeitpunkt, Position, Rotation und FOV. **Add keypoint**, **Update selected**, Zeit ändern, Punkt entfernen; Punkte in der Leiste anklicken. Der Übergang eines Punkts gilt bis zum nächsten: **Cut**, **Linear**, **Smooth**, **Ease in**, **Ease out**, **Ease in/out**. Ein Schnitt hält die erste Einstellung bis zum nächsten Punkt. Vor und nach der Spur wird der erste/letzte Punkt gehalten.
- Vorschau und Export werten dieselbe Kameraspur anhand der Aufnahmezeit aus. Keine fortlaufende Simulation, kein erneutes Einspeisen von Eingaben. Rendergeschwindigkeit und Scrub-Reihenfolge verändern die Kamerapose nicht.
- Ungespeicherte Änderungen einer Cinematic-Pose müssen als Keypoint übernommen oder verworfen werden, bevor exportiert werden kann.
- Jeder Overlay-Baustein lässt sich auf der Vorschau verschieben und an der unteren rechten Ecke skalieren. Position, Breite und Opazität sind zusätzlich numerisch einstellbar. Run-Informationen bleiben als Replay mit dem ursprünglichen Status erkennbar.
- Drei benennbare Exportprofile. Profile speichern Ausgabe und Overlaystil; Kameraspur und Clip-Bereich bleiben beim jeweiligen Run. Gültige Clip-Projekte werden beim Schließen und vor dem Export separat gespeichert. Originalaufnahmen und ursprüngliche Validierungsberichte bleiben unverändert.

## Qualität und Ausgabe

| Ausgabe | Eigenschaften |
|---|---|
| MP4 / H.264 | Bestehender Windows-Media-Foundation-Pfad; verlustbehaftet, YUV 4:2:0. |
| GIF | Animation mit begrenzter Palette; maximal 25 FPS. |
| MKV / FFV1 | Verlustfreie Übernahme der gerenderten RGB-Pixel; keine NV12-/YUV-Konvertierung. Separater, mitgelieferter Encoder. |
| PNG-Sequenz | Verlustfreie RGBA-Einzelbilder und `sequence.json` mit Bildrate und Quellzeit. Große Dateien; DEFLATE ohne aufwendige Kompression. |

**Transparent overlays only** gibt ausschließlich Overlay-Bausteine als PNG-Sequenz mit geradem Alpha aus. Dies ist ein Compositing-Asset ohne Szenenbild/Replay-Kopfzeile. Normale Replay-Clips bleiben gekennzeichnet. Originaler Spielton ist nicht aufgezeichnet und wird nicht erfunden.

**3 s test export** verwendet die gewählte Ausgabe und ihre tatsächlichen Encoder für bis zu drei Ausgabesekunden ab dem Playhead. Der originale Clip-Bereich bleibt erhalten. Verfügbarer Speicher und Größenhinweise stehen unter Output. Maximal 2560×1440, 60 FPS, 216.000 Bilder pro Clip; bestehende kurze Frame-Warteschlange und temporäre Ausgabe mit atomarer Übernahme. Abbruch entfernt eigene Teildateien; vorhandene Ausgaben bleiben geschützt.

## Input-HUD

- Tastatur, Maus, Controller, akzeptierte Spielzustände und kurze Zustandsmeldungen separat ein-/ausblenden.
- Einzelne logische Tasten, sieben Maustasten und einzelne Spielzustände auswählen; vollständige Tastatur oder Bewegungspreset.
- Panel-Hintergrund, Outline, Schatten, Tasten-Outline, Ruhefüllung, Panel-/Inhaltsopazität und Mausgröße unabhängig anpassen. Direkter Tastendruck, kurz ausblendender Glow. Farben/Schrift folgen der vorhandenen GUI-Gestaltung.
- Live-HUD und Export besitzen getrennte Einstellungen. Der vorhandene HUD-Editor positioniert das Live-HUD; Skalierung jetzt 0,5–2,5×. Im Export sind seine Bausteine einzeln platzierbar.
- Fehlende historische Tastendaten werden als `--` angezeigt. Beobachtete Tasten und akzeptierte Spielzustände bleiben getrennt; kein Echtheitsversprechen aus der Input-Anzeige.

## Update / Kompatibilität

**Helper-DLL und Trainer-EXE gemeinsam ersetzen.** `PracticeSuiteEncoder.exe` neben die Trainer-EXE legen; `Encoder` mit Lizenz und vollständigem zugehörigen FFmpeg-Quellcode mitnehmen. Hauptprotokoll **21**, unveränderte Telemetrie-/Command-Größen; eigener Replay-Frame-Kanal **2**. Einstellungen **17** lesen ältere Einstellungen. Eingabeformate 1–3, Karten-/Replay-Formate und Run-Pakete bleiben lesbar.

Kameraprojekte: `%LOCALAPPDATA%\ItStealsPracticeSuite\Clips\Projects\run-<ID>.isclip`. Profile: `Clips\Profiles`. Ausgaben: `Clips`. Projektdateien haben Format 1 und Zuordnungs-/Konsistenzprüfungen; sie sind reine Schnittmetadaten, keine neuen Validierungsnachweise.

Wand-Fading bleibt auf dem bisherigen Stand. Monster-Texturen, Animationen, neue Beleuchtung und Reportdienst sind nicht Teil dieses Updates.

Prüfung und Grenzen: `PRUEFUNG.md`. Auf diesem Host sind keine Windows-/Unity-Livetests möglich.
