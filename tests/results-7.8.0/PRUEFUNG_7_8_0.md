# Prüfung – PracticeSuite 7.8.0 / mab

Windows-x64-Trainer: MinGW GCC 13 / C++20 / O2 / statisch, Warnungen als Fehler. Helper: optimiertes C# 7.2 gegen die tatsächlichen Game-/Unity-/MelonLoader-/Harmony-Referenzen, Warnungen als Fehler. Spielassembly SHA-256 `ead3f5f861ad7988ad5fdb6f188bb3f77639927dd496b73066860d6c853f0dd8`, Unity 2021.2.19f1. Hauptprotokoll 24, Frame-Kanal 3, Eingabeformat 5, Clip-Projekt 7.

## Aktuelle Prüfungen

| Bereich | Prüfung und Ergebnis |
| --- | --- |
| Originalspiel-Code | IL-Inventar aller 162 Typen und gezielte Dekompilierung der Spieler-/Monsterklassen. Die 364 Play/OneShot/Stop/Pause-Stellen werden ohne weitere Zufallsauswahl durchgeschleift. Harmony-Patch-Erzeugung gegen die tatsächliche Assembly: 364 Audiostellen, 21 Physikstellen, 89 Zustands-Schreibstellen, keine fehlgeschlagene Einrichtung. Bestehende Stun-/Light-Feldpatches lassen sich mit der neuen Beobachtung kombinieren. Das startet keine Unity-Spielschleife. |
| Daten und Audio | Produktive C#-Writer/Reader und C++-Reader: Format-5-Roundtrip, erste Zustandsbeobachtung gegenüber Wechsel, harte Animationsgrenzen, Namen/Referenzen/Flags, alte Format-4-Datei ohne neue Kanäle. Audio-Identität, volle Prüfsumme, beschädigte Daten, exakte Samples bei 1x und nach Seek/0,5x, Lücken, NaN-Sanitisierung, leerer/verworfenener Mitschnitt. |
| Audioexport | Offline-WAV-Samples unabhängig mit ihrer Soll-Quellzeit verglichen: 0,5x, 1x, 2x und weiche Tempo-Keypoints. Derselbe SpeedMap-Code wie Video. Exportabbruch und zusätzliches Audiobitbudget geprüft. |
| Echte Mediendateien | Gleich konfigurierte FFmpeg-8.1.3-/x264-Quellen als Linux-Testbuild. MKV/FFV1, AVI/UtVideo, MOV/ProRes und MP4/x264: 27 Bilder / 60 FPS / 0,45 s, jeweils 48-kHz-Stereo. Zusätzlich alle vier Video-Codecs bei 30/60/120 FPS mit expliziten Frame-IDs, monotonen PTS und drei vollständigen Größenlimits (256000/1000000/5000000 Bytes) erneut geprüft. Unabhängiges ffprobe und vollständige FFmpeg-Decodierung. Float-PCM nach dem Muxen bytegenau identisch; MP4-AAC erwartungsgemäß verlustbehaftet. |
| Größenlimit mit Ton | Zwei-Pass-x264 mit AAC-Bitreserve: kompletter 27-Frame-Clip mit AAC, unter 256000 Bytes, 0,45 s. Die explizite Ausgabe-Endzeit verhindert zusätzliche AAC-Nachlaufzeit. Tatsächliche Größenkontrolle und bis zu drei Nachversuche bleiben erhalten. |
| Archiv | Vollständiger Hintergrundweg: Save → Paketexport → Import → unabhängige 3D-Inbox. Tondatei und Input-Verknüpfung bleiben erhalten. Bestehende Konflikt-/Legacy-/Marker-/Manifesttests für Pakete bestehen weiter. Fehlender Ton sperrt vollständigen Paketexport, aber nicht die 3D-Bildwiedergabe. |
| Speicherfehler | Neue native Experience-/Audio-/Paketprüfung zusätzlich unter AddressSanitizer und UndefinedBehaviorSanitizer, ohne Befund; LeakSanitizer aus. Dateigrößen, Reader-Puffer, Queue und Metadaten besitzen feste Grenzen. |
| Bestehende Systeme | Native Regressionen für Clips/Abbruch/PNG/GIF, Kamera, Tempo, Timeline, Exportprofile, Replay-Pakete, Snapshot/Geometrie, Settings und Routenplayback bestanden. Historische Eingabeformate 1–4 bleiben lesbar. |
| Oberfläche | Echte ImGui-Tests: neuer Analysebereich mit Audio-/Query-Optionen bei 480/1920 Pixel und 100/150 % DPI; Projekt-7-Roundtrip mit Audio und Migration 1–6. Bestehende UI-Interaktionen für Kamera/Follow/Free/Keypoints, HUDs, Minimap, Finish-Screen, Marker und Pflichtanzeigen bestehen. Draw-Daten wurden gerendert und visuell geprüft. Die Vorschau zeigt synthetische Testdaten. |
| Frame-Kanal / Layout | Produktions-Frame-Kanal mit Host-Grafikdoubles: Version 3, Posebytes, Serial/Ack, BGRA-Zeilenrichtung, Fehler/Grenzen/Disconnect. Gemeinsamer Layoutgenerator bestanden. |

Aktuelle Logs und Daten: **tests/results-7.8.0**. Ältere Ergebnisordner dokumentieren frühere Versionen und sind keine neu durchgeführten Messungen. Die ausgelieferten Binärdateien und zugehörigen Quellen werden über das Paketmanifest gebunden.

## Reproduktion

`build.ps1 -GameDir <Spielordner>` verwendet die lokal installierten Spiel-/Loader-Dateien. `Encoder/build-encoder.sh` baut den Windows-Encoder aus den gebündelten hash-geprüften Quellen und aktualisiert seine Trainer-Prüfsumme; Trainer danach neu bauen. Keine Spiel-/Loader-DLLs werden mitgeliefert.

C++20-Tests: `-ITrainer -ITrainer/imgui -Itests -pthread -Wall -Wextra -Werror`. Für Sanitizer zusätzlich `-fsanitize=address,undefined -fno-sanitize-recover=all`, ASAN_OPTIONS=detect_leaks=0. Export-/Archivprüfungen benötigen frische eigene Testordner, da vorhandene Dateien geschützt werden.

`tests/replay_experience_tests.cs` verwendet den produktiven Helper und alte Input-/Map-Fixtures. `tests/replay_experience_tests.cpp` liest diese neuen Dateien und prüft Samplezeit und Archivweg. `tests/ui_tests.cpp` benötigt die vier ImGui-Kernquellen, benachbarte managed-data- und experience-Fixtures; `tests/render_ui.py <draw>` rendert die gespeicherte GUI. `python generate-layout.py --check` prüft das Protokoll.

## Grenzen und Zieltest

**Hier lief keine Windows-/Unity-Spielinstanz.** Die tatsächliche Listener-Aufnahme, DSP-Latenz, Klang, Hörgerätwechsel, D3D-Bildausgabe, Media-Foundation-Ausgabe sowie reale Spiel-FPS sind auf dem Zielsystem noch zu prüfen. Die Harmony-Prüfung erzeugt/komponiert Patches; sie führt keine Monster-KI, Physik oder Unity-Audiofunktion aus. Entsprechende nicht auflösbare Unity-ICall-Hinweise im Host sind keine erfolgreiche Engine-Ausführung. Der ausgelieferte Windows-Encoder ist querkompiliert; Codec-Lauftests verwenden dieselben Quellen als Linux-Build.

Neue Mitschnitte sind erforderlich. Alte Replays erhalten keine erfundenen Ton-/Animationsdaten. Der Mix behält den ursprünglichen Hörpunkt, Tempoänderungen verändern Tonhöhe. 2D-Vorschau/GIF/PNG bleiben stumm; Videoexport kann Ton enthalten. Surround wird für Video nach Stereo abgebildet. Größen-/Aufnahmelimits und Lücken bleiben sichtbar. Keine neue Messung von Spiel-FPS und keine Zusage, dass sämtliche ursprünglichen Animationseffekte rekonstruiert werden. Shaderzeit, Partikel und noch fehlende historische Daten bleiben begrenzt.

Kurzer Zieltest: neuen Run mit Schritten, Taschenlampe/Kamera und Monsterreaktion aufzeichnen; Run-Ende, Tod und Reset prüfen. Im 3D-Viewer Ton einschalten, vor/zurück springen, pausieren und Kamera wechseln; beim Verlassen ursprüngliche Audiokonfiguration prüfen. Einen Clip mit 1x→0,5x→1x-Keypoints samt Ton exportieren, Vorschau/Dauer vergleichen und Archivpaket auf einer zweiten Installation importieren. Ein altes Replay/Projekt laden. Fading und Hide-and-Seek-Geometrie bleiben beim bisherigen Stand.

Integritätswerte belegen lokale Datenkonsistenz und keine manipulationssichere Herkunft.
