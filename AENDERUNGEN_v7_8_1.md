# PracticeSuite 7.8.1 / mab

## Fullbright

Die Ambient-/GI-Aufhellung aus 7.7/7.8 wurde durch eine reversible Unlit-Texturansicht ersetzt. Das Nutzerfoto zeigte stark aufgehellten Boden und weiterhin dunkle Wände: Eine weiße Ambient-Probe ersetzt bei den verwendeten Materialien keine gleichmäßige Texturdarstellung.

`SceneBrightness` verwaltet weiterhin einen eigenen Scope für Practice und Replay. Bestehende Materialinstanzen wechseln ihren Shader, nicht ihre Geometrie. TOZ-Welt-/Lokalprojektion verwendet den passenden vorhandenen Unlit-Shader; sonst Unlit/Texture bzw. Unlit/Color. Texturen, UV-Werte, Property-Blöcke, Materialslots und statische Batch-Bereiche bleiben bestehen. Für abgespeckte Replay-Materialien wird die fehlende Unlit-Instancing-Variante berücksichtigt. Beim Abschalten werden Originalshader, Renderqueue, Instancing und Nebel zurückgesetzt. Neuere fremde Shaderänderungen werden erhalten. Nicht vorhandene transparente Unlit-Varianten führen nicht zu plötzlich undurchsichtigen Effekten. Originalbeleuchtung bleibt im normalen Replay-Profil erhalten.

Gleichbleibende Wiedergabeframes verursachen keine Shader-Suche, Materialerzeugung oder erneute Materialbelegung. Dynamische Practice-Objekte werden über den bestehenden langsamen Scan ergänzt.

## Kristall-Pickup und Aufnahme

Der Spielcode ruft in `MouseLook.OnTriggerEnter` bereits `getPoint.Play()` auf. Dieser Aufruf gehörte schon zu den erfassten 364 Audiostellen. In sämtlichen elf untersuchten GetPoints-Szeneninstanzen zeigt die tatsächliche Assetkonfiguration: Clip getPoint, zentriert/2D, kein Mixerziel, Listener-Bypass an, Quell-Effekte an. Der bisherige Listener-Filter hörte diesen separat vorbeigeführten Ton nicht.

Eine zusätzliche passive Quellaufnahme erfasst solche zentrierten 2D-Bypass-Quellen nach ihren Quellfiltern. Jede Quelle hat einen eigenen begrenzten SPSC-Puffer. Das Hintergrundschreiben addiert die PCM-Daten erst auf bereits vorhandene Listener-Samples, mit derselben DSP-Uhr und der beobachteten Quelllautstärke. Die Reihenfolge der Quell-/Listener-Callbacks ist unabhängig. Uhrkorrekturen addieren überlappende Quellabschnitte nicht mehrfach. Normal- und Mixerquellen werden nicht doppelt aufgenommen. Mute-/Lautstärke-/Routingwechsel werden berücksichtigt. DSP-Callbacks verändern den Spielpuffer nicht und machen weder Datei-I/O noch Speicherallokationen oder Locks.

64 zusätzliche Routen maximal, acht vorab angelegte Blöcke je Route. Fehlende Blöcke, Limits und nicht beobachtbare Routen bleiben markiert. Letzte Callback-Blöcke erhalten bis zu 200 ms zum Eintreffen; die gespeicherte Dauer bleibt exakt an die Run-Dauer gebunden. Bypasste 3D/HRTF-Quellen sowie Quellen mit deaktivierter Quellverarbeitung werden nicht mit geschätzten Klängen rekonstruiert. Die GUI erklärt solche unvollständigen Audiodaten nun verständlich.

**Neue Aufzeichnung erforderlich:** In bestehenden 7.8.0-Tondateien fehlen die vorbeigeleiteten Samples weiterhin. Der letzte Pickup kann wie jeder Ton am gewählten Clip-/Run-Ende abgeschnitten werden; die Aufnahme verlängert den Run nicht künstlich.

## Kompatibilität

7.8.0-Replay-Pakete werden ausdrücklich weiter akzeptiert, auch mit Markern. Dateiformate und Hauptprotokoll bleiben unverändert: Protokoll 24, Frame-Kanal 3, Input 5, Projekt 7, Audio 1. Der Encoder ist unverändert. Quellen, Binärdateien und Prüfprotokolle gehören zum vollständigen Paket. Spiel-/Loader-Dateien werden nicht mitgeliefert.
