# Prüfung – PracticeSuite 7.8.1 / mab

## Diesmal ausgeführte Prüfungen

| Bereich | Ergebnis und Grenze |
| --- | --- |
| Builds | Helper: optimiertes C# 7.2 gegen tatsächliche Game-/Unity-/Loader-/Harmony-Referenzen. Windows-Trainer: GCC 13 / C++20 / O2 / statisch. Beide mit Warnungen als Fehler bestanden. ImGui-Testprogramm ebenfalls gebaut. |
| Originalspiel | IL-Inventar aller 162 Typen. Pickup-Aufruf ist `MouseLook.OnTriggerEnter → getPoint.Play()`. 364 Play/OneShot/Stop/Pause-Stellen. Elf GetPoints-Assetinstanzen geprüft: gleicher Clip, Listener-Bypass, zentrierte 2D-Route, Quell-Effekte aktiv. Spielassembly SHA256: ead3f5f861ad7988ad5fdb6f188bb3f77639927dd496b73066860d6c853f0dd8. |
| Fullbright | Produktiver Scope mit Unity-Vertragsdoubles: TOZ-/Texture-/Color-Auswahl, fehlende Alpha-Varianten, GPU-Instancing, unveränderte Texturen/UVs/Property-Blöcke/Batch-Bereiche, Shader-/Queue-Restore, späte Materialien, neue Shader-Schreiber, Fehler-Rollback und Scene-Wechsel bestanden. 10000 stabile Frames ohne zusätzliche Shader-Suche oder Materialschreibzugriffe. Das ist kein gerenderter GPU-Vergleich. |
| Quellaufnahme | Produktiver Unity-Adapter mit Vertragsdoubles: echte GetPoints-Konfiguration, gemeinsamer DSP-Ursprung, Pass-through-Puffer, keine Doppelung von Normal-/Mixerrouten, Mute-/Gain-/Routingwechsel, dynamische Quellen und Abschlussblöcke bestanden. |
| PCM und Format | Produktiver Helper: exakte Samples und Dauer, Vor-/Nachlaufgrenzen, Quellen vor/nach Listener, mehrere Quellen, Quelllautstärke, Mono-Upmix, Uhrkorrektur ohne Verstärkung, begrenzte Queue/Überlaufmarkierung, 64-Routen-Limit, Flags und Dateiprüfsumme bestanden. Bestehende Experience-/Legacy-/Seek-/Raten-/Korruptionsprüfungen bestanden. |
| Native Reader/Export | Neue C#-Dateien im C++-Reader gelesen; WAV-Samples bei 0.5/1/2x und weichen Speed-Keypoints geprüft. Save → Paket → Import → 3D-Inbox samt Audio bestanden. Bestehende Kamera-, Clip-, Tempo-, Timeline-, Profil-, Settings-, Geometrie- und Routenplaybacktests bestanden. Vorhandene Ausgabedateien werden geschützt; Exporttests wurden dafür in frischen Zielordnern ausgeführt. |
| Pakete | Rückwärtsimport von 7.8.0, 7.7.0 und älteren Paketen einschließlich benannter Marker mit Prüfsummen und Identitätsbindung geprüft. |
| Oberfläche | Echtes ImGui-Testprogramm vollständig bestanden: 480–1920 Pixel, 100/150 % DPI, Replay, Kamerapunkte, HUD, Minimap, Finish-Screen und verpflichtende Validierungsanzeige. Neue Texte und Produktiv-UI kompiliert. |
| Protokoll | Layoutgenerator und produktiver Frame-Kanal mit Hostdoubles: Version 3, Pose/Audio-Steuerwerte, Serial/Ack, Zeilenrichtung, Fehler und Disconnect bestanden. |

Aktuelle Logs und synthetische Daten: **tests/results-7.8.1**. Quellen der Testprogramme liegen in **tests**. Die unveränderten Encoder-Codecs wurden in dieser Korrektur nicht erneut gebaut oder durch sämtliche Formate gerendert. Die ausführlichen Codec-Messungen der 7.8.0-Auslieferung bleiben in **tests/results-7.8.0** erhalten und werden hier nicht als neue Tests ausgegeben.

## Reproduktion

`build.ps1 -GameDir <Spielordner>` nutzt die vorhandenen Spiel-/Loader-Dateien. C++20-Tests mit `-ITrainer -ITrainer/imgui -pthread -Wall -Wextra -Werror`; UI benötigt vier ImGui-Kernquellen und benachbarte managed-data-/experience-Fixtures.

C#-Scope-Test: `tests/fullbright_tests.cs` zusammen mit `Mod/SceneBrightness.cs`; Adaptertest: `tests/replay_audio_capture_tests.cs` mit `Mod/ReplayAudioCapture.cs`. Beide verwenden ausdrücklich Doubles. `tests/replay_experience_tests.cs` dagegen referenziert den produktiven Helper. `tests/replay_experience_tests.cpp` liest dessen Dateien und testet Archiv/Exportzeit. `python generate-layout.py --check` prüft die gemeinsamen Layouts.

## Noch auf dem Zielsystem zu prüfen

**Hier lief keine Windows-/Unity-Spielinstanz.** Daher kein Nachweis der tatsächlichen GPU-Ausgabe, hörbaren Unity-Mischung, DSP-Latenz oder Spiel-FPS. Code-/Assetbefund und bestandene Hosttests ersetzen diesen Zieltest nicht:

1. Neuen Run mit mehreren Kristallen aufnehmen, vollständig im 3D-Replay hören und einen Bereich mit Ton exportieren.
2. Fullbright in Practice, nativem und vereinfachtem Replay mehrfach umschalten; Wände/Boden/Monster, versteckte Decke und anschließende Originalbeleuchtung ansehen.
3. Replay verlassen, neue Szene laden, normales Spielbild und Ton vergleichen. Altes 7.8.0-Paket mit Markern importieren.

Alte Aufnahmen werden nicht nachträglich ergänzt. Nicht beobachtbare Audio-Routen sind als unvollständig gekennzeichnet. Transparente Effekte behalten bei fehlendem Shader ihre Originaldarstellung. Hide-and-Seek-Geometrie und bisheriges Wandfading wurden in diesem Hotfix nicht überarbeitet. Lokale Prüfsummen belegen Datenkonsistenz, keine fälschungssichere Herkunft.
