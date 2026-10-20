# PracticeSuite 7.8.0 / mab

## Originalton

- Beobachteter Unity-Spielmix statt geratener Soundereignisse oder erneuter Zufallsauswahl. Keine Mikrofon-/Desktopaufnahme und keine kopierten Spielassets.
- Pass-through-DSP-Aufnahme ohne IO/Locks/Allokationen im Audiocallback; 32 vorab reservierte Blöcke, Hintergrundschreiber, 256-MiB-Dateigrenze. Lücken werden zeitlich mit Stille gefüllt und markiert. Der letzte DSP-Block darf nach Run-Ende ankommen. Verworfenes Kurztraining verwirft auch seine Tondatei.
- `.isaudio` ist über Run-ID, Szene, Seed, Gamehash, Formatdaten und FNV-Prüfsumme mit `.isinput` verknüpft. Pakete enthalten zusätzlich die bestehende SHA-256-Dateiliste. Fehlender/beschädigter Ton sperrt den 3D-Bildkanal nicht; vollständige Paketexporte geben einen verständlichen Fehler aus.
- Eigener Replay-Listener/-Source, rücksetzbare ursprüngliche Audiokonfiguration, Hintergrund-Lesepuffer und absolute Sprünge. Der originale Hörpunkt bleibt auch bei Free-/Follow-Kamera erhalten.
- Offline-Audioexport mit gleicher SpeedMap wie Video; Float-PCM bei MKV/AVI/MOV, AAC bei beiden MP4-Varianten. Lautstärke ist im Projekt gespeichert. GIF/PNG bleiben stumm. Zwei-Pass-Größenbudget reserviert auch AAC-Bitrate. Video wird beim Ton-Muxen unverändert kopiert.

## Animation und Spielbeobachtung

- Zusätzlich zu den bisherigen Posen: Animator-Zustand/Phase, Übergang, Tempo, Layergewicht, dominanter Clipname und tatsächliche Blendshape-Gewichte. Auch animierte Weltobjekte werden gebunden.
- Knochenposen weiterhin bis 30 Hz, Übergangs-/Morphdaten bis 60 Hz und unmittelbare Aufzeichnung erkannter harter Zustandswechsel. Keine Behauptung fehlender 120-Hz-Details. Harte Wechsel werden nicht vorweg interpoliert.
- Alle 364 ursprünglichen Play/OneShot/Stop/Pause-Aufrufstellen erfasst. Beobachtung nach dem ursprünglichen Aufruf; kein zusätzlicher RNG-Aufruf und kein erneutes Abspielen zur Ereigniserkennung.
- Tatsächliche Raycast-/Linecast-Ergebnisse der Monster mit Quellstelle, Layer-Maske, Ursprung, Ziel und Zeitpunkt, Geometrie bis 10 Hz. Kein zusätzlicher Physiktest. Boolesche Zustandswechsel separat ohne diese Drosselung; erster Zustand als Beobachtung gekennzeichnet.
- Classic `HasBeenSpotted`, Hide-and-Seek `SeeingIt` und `FoundPlayer` besitzen unterschiedliche Bedeutungen. Anzeige und Hilfetexte vermeiden die Gleichsetzung mit einem erfolgreichen Sichtkontakt. Ausgewählte Abfragen sind keine vollständige KI-Simulation.
- Kompakter, aufklappbarer Analysebereich mit Ereignis-/Actorfilter, zeitgenauem Sprung und virtualisierter Liste. Timeline-Punkte werden gecacht; 3D-Abfragen sind abschaltbar und auf 128 Segmente pro dargestelltem Zeitpunkt begrenzt.

## Kompatibilität und Grenzen

Hauptprotokoll 24, Commands 192 Byte, Telemetrie 16952 Byte; Frame-Kanal 3; Eingabeformat 5, Leser akzeptieren 1–5; Clip-Projekt 7, Leser akzeptieren 1–7. Settings und Kartenformate unverändert. Ältere Aufnahmen bleiben unverändert, neue Kanäle fehlen dort ausdrücklich. Die bestehenden Hide-and-Seek-WIP-Markierungen bleiben bestehen. Wand-Fading unverändert.

Die Prüfungen ersetzen keinen Windows-/Unity-Livetest. Die Integritätswerte bestätigen Datenkonsistenz, keine manipulationssichere Herkunft oder physische Eingaben. Details und ausgeführte Tests stehen in PRUEFUNG.md.
