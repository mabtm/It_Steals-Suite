# Prüfung 6.9.8

18 erfolgreiche Gruppen: mod, trainer, managed, native, ui, planner, search, settings, portable, playback, hide-seek-player, replay, layout, movement-managed, movement-native, hud-adapter, experience und audio-adapter. Logs unter tests/results-6.9.8, Vorschauen unter tests/previews-6.9.8.

## Native Oberfläche und Bedienung

Die produktiven Controls werden in der echten ImGui-Ausgabe ausgeführt. 168 bestehende Tab-/Viewport-/DPI-/Verbindungsfälle, 216 HUD-Konfigurationen, 128 Mode-Regeln und 288 Minimap-Ansichten bleiben erfolgreich. Zusätzlich echte Maus- und Tastaturaktivierung: Schalter ändern genau einmal ihren Zustand; deaktivierte Controls und bloßes Hover erzeugen keine Aktion/Töne. Eine Kopierbestätigung erhält ID und Größe des Buttons. Cue-Priorität, stumme Run-/Menüzustände und veraltete Tonanforderungen sind geprüft.

Geöffnete/geschlossene Detailbereiche einschließlich reduzierter Bewegung, Zwischenphasen der Start-/Abschlussgrafik und die neuen echten Menüseiten werden gerendert. Alle belegten Zeichenbereiche bleiben in höchstens vier nicht überlappenden HUD-Ausschnitten enthalten. Kein neues Vollbild-Readback für die Effekte. Der Pflichtstatus, Koordinaten, Deckkraft und die großen Replay-Karten bleiben geprüft.

Die Animationstabelle enthält fest 512 Einträge ohne Wachstum pro Widget. 100.000 Lookups benötigen auf dem Host mit Sanitizern ungefähr 6,5 ms; dies ist kein Live-Spiel-Benchmark. Bewegung ist abschaltbar. Start/Exit benutzt einen unabhängigen Zeitgeber; der Modus kann auch ohne weitere sichtbare Renderframes enden.

## Originale Töne und privater Audio-Worker

Die PCM-Töne werden aus eigenen kurzen, glatt ein-/ausgeblendeten Wellen erzeugt. Getrennte Lautstärke durch Multiplikation ausschließlich dieser Samples; kein Schreiben an System-/Spielmixer. Samples werden auf endliche Werte, Pegel, leise Standardlautstärke, Stille bei Volume 0 und einheitliche Gain-Skalierung geprüft. UI-Töne nur bei geöffnetem, fokussiertem Menü ohne aktiven Run. Kein Hover-Ton und keine wartende Geräuschfolge nach Alt+Tab.

Die produktive InterfaceAudio-Klasse wird mit typisierten Win32-/WinMM-Doubles und einem tatsächlichen Hintergrundthread ausgeführt: eventgesteuertes Warten, Geräteöffnung, Buffer-Lebensdauer bis zur Freigabe, Mute/Abbruch laufender Töne, begrenzte Klickfolgen, Fehler beim Öffnen/Prepare/Write und wiederholtes Shutdown. Geräteaufrufe halten keine UI-Sperre. Bei stummer/geschlossener Oberfläche wird das Gerät freigegeben. Ein echtes Windows-Ausgabegerät und die hörbare Abstimmung konnten auf diesem Host nicht geprüft werden.

## Speicherung und Regressionen

Settings Format 13: neue Optionen/Rundlauf, valide Lautstärke, Migration alter Standardfarben und Erhalt eigener Farben. Der echte Settings-Worker bleibt auf Zusammenfassen von Writes, Backups, beschädigte Originale, unbekannte zukünftige Formate und Schreibfehler geprüft. Replays bestätigen erfolgreiche Writes über eine veröffentlichte Revision; fehlgeschlagene Saves erhöhen diese nicht. Erfasste Run-Daten/Validierung und das Replay-Dateiformat werden nicht verändert.

Alle bisherigen SDK-, Decode-/Fuzz-, Seed-/Ranking-, NavMesh-/Asset-, Movement- und LayeredHud-Adapterprüfungen bleiben erfolgreich. Die Route-/Bewegungserfassung wurde zusätzlich gegen das gespeicherte 6.9.7-Paket verglichen und ist unverändert. Ein/sechs Worker, Hide-&-Seek-Innen-/Outside-Bodendreiecke, Top 10, animierte Replay-Routen, Light Events und ABORTED bleiben geprüft. Die verbleibenden Hide-&-Seek-Geometriefehler bleiben WIP.

C#-Mod gegen die bereitgestellten Spiel-/Unity-/MelonLoader-/Harmony-DLLs; 64-Bit-Windows-Trainer mit MinGW, Warnungen als Fehler. Protokoll 15 / 128-Byte-Commands / 16276-Byte-Telemetrie; Kartenkanal 9 und Payload 11 unverändert. Native Logik-/Adapterprüfungen mit ASan/UBSan, LeakSanitizer auf dem Host deaktiviert. Keine echten Spiel-/Loader-DLLs im Paket.

## Grenzen

Kein laufendes Windows-/Unity-Spiel verfügbar. Deshalb keine zugesagte Verbesserung realer Spiel-FPS, keine live bestätigten D3D-/Fokus-/Audio-Treiber. Die neuen Grafik- und Audiooptionen sind einzeln abschaltbar. Die Zeitschätzung bleibt eine Prognose; der lokale VALID-Status ist keine manipulationssichere Anticheat-Attestierung.
