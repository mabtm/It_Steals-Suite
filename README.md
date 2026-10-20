# It Steals PracticeSuite 7.11.5 / mab

Windows-Trainer mit Cheats für Practice, Seed-Suche, Minimap, zwei anpassbaren HUDs und Replay-Analyse mit Eingabe-/Kameraaufzeichnung und 3D-Vorschau. Fertige DLL/EXE, vollständiger Quellcode und Prüfungen sind enthalten.

## Installation

1. Spiel und bisherigen Trainer schließen. Alte Helper-DLL außerhalb von Mods sichern.
2. **bin/ItStealsHelper.dll** in **Mods** kopieren. Nur eine Helper-Version laden.
3. **bin/ItStealsTrainer.exe** ersetzen. **bin/PracticeSuiteEncoder.exe daneben legen** und den Ordner **Encoder** mit Lizenz und zugehörigem Quellcode mitnehmen. Trainer starten; das Spiel darf vorher oder danach starten.
4. **F3** öffnet das Menü. Einstellungen, bisherige Run-Daten und Replays bleiben erhalten.

**Kompatibilität (mitgelieferter Helper 7.11.4):** Hauptprotokoll **29**, Commands **192 Byte**, Telemetrie **16952 Byte**. Der getrennte Kartenkanal verwendet Version 10, Karten-/Replay-Payload 13, Eingabeformate 1/2/3/4/5/6, bestätigte Seed-Batches 3. Alte lesbare Replay-/Kartenformate bleiben unterstützt. MelonLoader ist erforderlich; originale Spiel-/Loader-DLLs sind nicht enthalten.

## Neu in 7.11.5: Inspector-Gestaltung

Neue schwebende Objektkarte mit Symbolleiste, LIVE-/Inaktiv-Anzeige, Theme-Farben und Versions-/Autorangabe. Der Picker zeigt animierte Eckmarkierungen und einen kurzen Randimpuls. Die Details öffnen und schließen mit einer kurzen, unterbrechbaren Bewegung; geänderte Eigenschaften werden kurz hervorgehoben.

**Inspector > Style** regelt Auswahl-Effekte, eigene Sounds, relative Soundlautstärke, Paneldeckkraft und Konturstärke. Farben und Schriften folgen den bestehenden Interface-Einstellungen. Die globalen Sound-/Animationsschalter bleiben maßgeblich. Escape schließt zuerst ein offenes Kartenmenü.

**Update von 7.11.4:** Nur `bin/ItStealsTrainer.exe` ersetzen. Der mitgelieferte **Helper 7.11.4 bleibt unverändert und kompatibel**; die unterschiedlichen Versionsanzeigen sind bei dieser UI-Ausgabe beabsichtigt. Encoder und Replayformate bleiben gleich. Bei einer älteren Installation die mitgelieferte Helper-DLL übernehmen. Details: [AENDERUNGEN_v7_11_5.md](AENDERUNGEN_v7_11_5.md).

## Neu in 7.11.4: zuverlässige Routen und große Layouts

Routen bleiben beim Aktualisieren derselben Seed-/Spawn-/Kristallauswahl eingeschaltet. Ein unbekanntes Teilstück blendet nicht mehr die ganze Karte aus: geprüfte Abschnitte bleiben sichtbar, fehlende Verbindungen werden nicht überbrückt. **Show full route**, bleibende Zeichenlinie und weißer Richtungsimpuls bleiben erhalten.

Hide and Seek Outside verwendet für neue Unity-Abfragen den Agententyp der tatsächlich geladenen NavMeshSurface statt des ersten Monsters. Ein fehlgeschlagener oder schlechterer AI-Mitschnitt ersetzt keine besser verbundenen Bodendaten. Eine gemeinsame Tabelle verbindet vorhandene geprüfte Boden-/Korridorwege und nutzt sie für Vorschau und Seed-Suche; dieselbe Methode gilt in allen Modi. Unbekannte Verbindungen bleiben ausdrücklich unbekannt.

Für große Layouts wird diese Navigation einmal vorbereitet und wiederverwendet. Die vollständige bisherige Multistart-Suche und 2-opt bleiben erhalten; schnellere Nachbarabfragen und zusätzliches Versetzen einzelner Besuche verbessern die Reihenfolge. 40/52 Kristalle bleiben **heuristisch optimiert**, keine bewiesene globale Optimalroute. **Checked** bezeichnet geprüfte Wegstücke, **Exact** die exakte Reihenfolge für die modellierten Distanzen bis 15 Kristalle. Kontaktoptimierung bleibt gesondert heuristisch. Zeitangaben verwenden weiterhin gelernte Live-Bewegung oder das vorhandene Spielmodell; sie erscheinen für vollständig geprüfte Wege wieder regulär.

**Update von 7.11.3: Spiel und Trainer schließen, Helper-DLL und Trainer-EXE gemeinsam ersetzen, danach die Szene frisch laden.** Encoder und Main29 / Inspector3 / Dateiformate unverändert. Hide and Seek / Outside bleiben bei Geometrie/Seed-Suche als WIP markiert. Gemessene Beschleunigung und Grenzen siehe **PRUEFUNG.md**; Änderungen: **AENDERUNGEN_v7_11_4.md**. Folgende Versionsabschnitte sind historisch.

## Hotfix 7.11.3: Seed-Route dauerhaft sichtbar

Unter **Seed Map → Preview & replay → Optimal route** wächst die Linie jetzt vom Spawn bis zum Ziel und bleibt danach vollständig sichtbar. **Show full route** zeigt sofort den ganzen geprüften Weg, **Restart drawing** startet neu. **Pause animation / Resume animation** hält Zeichnung und weißen Richtungsimpuls an. Die Zeichenanimation dauert fünf Sekunden und ist unabhängig von der geschätzten Run-Zeit. Bei reduzierter Bewegung erscheint die Route sofort ohne Impuls. Neue Seeds starten weiterhin mit ausgeblendeter Optimalroute.

**Update von 7.11.2:** Nur bin/ItStealsTrainer.exe ersetzen, Trainer neu starten. **Helper bleibt 7.11.2**, Encoder, Hauptprotokoll29, Inspector3 und Dateiformate unverändert. Bestehende Replay-Animationen sind unverändert; 7.11.2-Replaypakete bleiben importierbar. Details: **AENDERUNGEN_v7_11_3.md**. Die folgenden Versionsabschnitte sind historisch.

## Neu in 7.11.2: Ebenenauswahl und reparierte Details

Die ImGui-Meldung nach **Details** ist behoben: nicht zusammenhängende Object-/Transform-Felder werden zu einer eindeutigen Gruppe zusammengeführt. Die Objektkarte öffnet Details wieder am Anfang.

**F7 → Maus über ein Objekt → Mausrad** wechselt zwischen den Treffern unter dem Zeiger. **Tab** öffnet die eingefrorene **TARGET LAYERS**-Liste; dort Ebene wählen und mit **Select highlighted**, Doppelklick oder **Enter** übernehmen. So lassen sich Wand und Graffiti getrennt untersuchen. Auch Grafiken ohne Collider werden im standardmäßigen Modus **Visuals + colliders** erfasst. Die letzte Trefferliste bleibt auf der Objektkarte und im Inspector als Auswahl erhalten.

Lesbare Meshes und einfache Sprites verwenden ihre tatsächlichen Dreiecke; nicht lesbare, animierte oder zu große Geometrie erhält ausdrücklich **Bounds estimate**. Transparente Texturpixel und Shader-Verformungen werden nicht geprüft. Ein auf Frames verteilter Index und begrenzte Abfragen vermeiden vollständige Szenenabfragen pro Mausbewegung. Während des Aufbaus und bei begrenzter Trefferliste zeigt die Oberfläche den Status.

**Helper und Trainer gemeinsam ersetzen und das Spiel neu starten.** Hauptprotokoll **29**, Inspector-Kanal **3**; Karten-/Replay-/Clipformate und Encoder unverändert. Hide and Seek / Outside bleiben WIP. Bedienung: **LIVE_INSPECTOR.md**, Details: **AENDERUNGEN_v7_11_2.md**, Prüfnachweise: **PRUEFUNG.md**. Die folgenden Versionsabschnitte sind historisch.

## Neu in 7.11.1: direkter Live Inspector

**F7** startet den **Picker** direkt im Spiel. Unter der Maus erscheinen Objektname und Markierung; ein Klick öffnet eine kleine verschiebbare Objektkarte. **Esc / RMB / F7** brechen den Picker ab. **F3** gibt die Spielsteuerung zurück. **Pin card** hält die Karte beim Spielen sichtbar.

Der eigene Tab **Inspector** enthält die Auswahl, **Fullbright**, die optionale Objektliste und aufklappbare **Technical details**. Die Grundansicht zeigt Name, Collider-/Objekttyp, Aktivität, Position und Distanz. Scriptfelder, Materialien und komplette Collider-Daten werden erst auf Anfrage gelesen. Fullbright, erneute Auswahl, Objekt-ID kopieren und Freecam-Fokus sind auch direkt auf der kleinen Karte verfügbar.

Die Auswahl verwendet jetzt die beim Mausklick gespeicherten Spielclient-Koordinaten und die tatsächliche Kamera-Viewport. Hover und Übernehmen teilen dieselbe Abfrage; vor dem Übernehmen werden Ziel, Szene und Beobachtungskennung erneut geprüft. Fehlende, verschwundene oder überfüllte Treffer bleiben erkennbar. Trigger sind standardmäßig enthalten, damit Kristalle auswählbar sind; feste Collider und die ausdrücklich ungefähre Renderer-Bounds-Auswahl sind weiterhin wählbar. Objektkataloge werden nur bei Bedarf aufgebaut.

**Helper und Trainer gemeinsam ersetzen**, Spiel neu starten. Hauptprotokoll **28**, separater Inspector-Kanal **2**; bestehende Karten-, Replay- und Clipformate sowie Encoder unverändert. Hide and Seek / Outside bleiben WIP. Anleitung: **LIVE_INSPECTOR.md**, Änderungen: **AENDERUNGEN_v7_11_1.md**, Prüfgrenzen: **PRUEFUNG.md**. Die folgenden Abschnitte dokumentieren frühere Versionen.

## Neu in 7.11.0: Live Inspector

**Cheats → Live Inspector** oder **Diagnostics → Live Inspector** öffnet den neuen Arbeitsbereich. **Pick object** wählt direkt im Spiel; die Objektliste erfasst auch inaktive Objekte und Objekte ohne Collider. Eigenschaften bleiben schreibgeschützt. Auf kleinen Fenstern liegen Zusatzaktionen unter **Tools (…)**.

- **Properties:** Hierarchie, Transform, echte Collider, Collision-Layer/-Paare, Renderer, gemeinsame Materialien/Shader/Meshes und Felder der originalen Spielskripte. Eltern, Kinder und überlappende Raycast-Treffer sind auswählbar. Die Liste aktualisiert sich auf Wunsch; nur die Auswahl wird laufend gelesen. Beliebige Script-Methoden oder Property-Getter werden nicht ausgeführt.
- **Geometry:** Cyan zeigt den aktiven echten Collider, Orange einen deaktivierten/inaktiven. Violett zeigt die von der Suite tatsächlich erfassten Konturen. Pro Collider stehen die reale Erfassungsentscheidung, Spielerhöhe/-radius und der Erfassungszeitpunkt. Lesbare Mesh-Collider haben ein ausgewiesenes Kantenbudget. Die optionale Renderer-Bounds-Auswahl ist nur eine ausdrücklich ungefähre Auswahlhilfe und erzeugt keine erfundene Collider-Hitbox.
- **Watch / Compare:** Bis zu acht Werte pro Spiel-Frame beobachten und Änderungen mit beobachtetem Frame/Zeitpunkt lesen. Baseline aufnehmen, eine Spielaktion ausführen und die tatsächlichen Unterschiede ansehen. Die Ansicht behauptet keine Ursache für einen Zustandswechsel.
- **Route probe:** Zwei echte Oberflächen anklicken, um Bodenprojektion, Verbindung und geprüften Weg zu untersuchen. Dieselbe Boden- und Spielerradiusprüfung wie in 7.10.1; keine neue Hide-and-Seek-Sonderlogik. Fehlende oder unvollständige Geometrie bleibt erkennbar.
- **Focus in Freecam:** Die vorhandene Kamera auf die Auswahl richten. Nach Bestätigung kehrt die Steuerung ins Spiel zurück: RMB zum Umsehen, WASD/QE bewegen, Shift schneller, F3 zurück zum Inspector. Die Spielfigur wird nicht versetzt. Beim Verlassen wird eine vom Inspector gestartete Freecam wieder beendet.
- **Save diagnostic snapshot:** Lokale JSON-Datei mit ausgewählten Rohwerten, Geometrieentscheidungen, Beobachtungen, optionalem Vergleich, Routenprobe, Seed-Art, Kamera, Build-Hash und Diagnose. **Copy report path** kopiert den Pfad. Regulär unter `%LOCALAPPDATA%\ItStealsPracticeSuite\Diagnostics\Inspector`; bei fehlendem LocalAppData im bestehenden Trainer-Einstellungsordner. Keine Übertragung an einen Dienst.

Der Inspector ist in **Clean Speedrun** und während 3D-Replay/Export gesperrt. Tatsächliche Aktivierung zählt als Run-Hilfe. Listenabfragen sind auf Frames verteilt, Veröffentlichungen der Auswahl auf 5 Hz begrenzt; ausführliches Dekodieren und Routenproben laufen im Trainer-Hintergrund. Objekt-/Sitzungswechsel verwerfen alte Auswahl, Watches und Vergleiche.

**Update von 7.10.1:** Spiel und Trainer schließen, Helper-DLL und Trainer-EXE gemeinsam ersetzen. Hauptprotokoll ist jetzt **27**; die neue Inspector-Verbindung verwendet einen separaten Kanal **1**. Bestehende Einstellungen/Replays und der Encoder bleiben erhalten. Hide and Seek / Outside bleiben WIP. Details: **LIVE_INSPECTOR.md**, **INSPECTOR_FORMAT.md**, **AENDERUNGEN_v7_11_0.md**, **PRUEFUNG.md**.

## Neu in 7.10.1: Routen-Grenzfälle

Pickup-Kontakte funktionieren auch ohne erreichbaren Kristallmittelpunkt oder vollständigen Korridoranker. Sehr dünne Wandsegmente erhalten sichere Umwege. Ergebnisstatus und Zeichnung unterscheiden die tatsächlich benutzten bekannten und unbekannten Teilstücke. Dieselbe Korrektur gilt für alle Modi; Hide and Seek / Outside bleiben WIP.

**Helper und Trainer gemeinsam ersetzen**, danach das Spiel neu starten. Protokoll und Dateiformate bleiben wie in 7.10.0; Encoder unverändert. Die Host-Prüfungen umfassen acht Modi, kontinuierliche Kontaktwege, Score/Zeichnung, 4096 Seed-Auswahlen mit 1/6 Workern, alte Pakete, echte ImGui-Bedienung sowie ASan/UBSan. Kein Live-Spielnachweis. Details: **AENDERUNGEN_v7_10_1.md**, **CODE_REVIEW_v7_10_1.md**, **PRUEFUNG.md**.

## Neu in 7.10.0: Pickup-Routen und physische Kartengeometrie

- **Einsammelpunkte:** Neue Karten erfassen den echten Kristall-Trigger und den eingeschalteten Pickup-Trigger am Kameraobjekt. Das ist eine andere Form als der CharacterController für Bewegung. Die Route wählt einen zusammenhängenden Kontakt pro Kristall, statt immer dessen Mittelpunkt anzulaufen. Acht Randpunkte und sichere Anker halten die Suche schnell. Das Modell nutzt einen konservativen Bereich bei stehender Figur und waagerechter Kamera; Blickneigung, Ducken und Headbob verändern die genaue Kante. Tooltip und Diagnose nennen diese Annahme. Die Reihenfolge bleibt für Pickup-Bereiche heuristisch.
- **Radar und Karten:** Gemeinsame physische Konturen für alle Modi. Gedrehte und geneigte Boxen werden in der tatsächlichen Spielerhöhenzone geschnitten; Props und Wände unterscheiden sich durch Füllung und Kontur. Mesh-Teile schließen Durchgänge nicht mehr mit einem einzigen Bounds-Rechteck. Die NavMesh-Grenze wird bei vorhandenen physischen Konturen nicht als zusätzliche Wand darübergezeichnet. Dreiecksflächen erzeugen keine geglätteten inneren Rasterkanten mehr.
- **Hide and Seek / Outside:** Dieselbe Kapsel-, Boden-, Sichtlinien- und Umwegprüfung wie für die anderen neu erfassten Modi. Der kurze Controller wird mit seiner effektiven Höhe behandelt. Inaktive Objekte werden nur aus den tatsächlichen Culling-Katalogen übernommen. Enger gebackene AI-Ecken werden auf Abstand zur Spielerkapsel korrigiert. Die erste Aufnahme wartet auf Bodenkontakt. Beide Modi bleiben bis zum Live-Abgleich **WIP**.
- **Leistung und Diagnose:** Statische Konturen und Kontaktabstände werden einmal vorbereitet und gemeinsam von den Workern genutzt. Es gibt keine Unity-Abfragen pro Such-Seed. Die reine Radaransicht liest jetzt ebenfalls den detaillierten Kartenkanal und startet dafür keine unnötige RNG-Verifikation. Kopierte Diagnosen enthalten Geometrieabdeckung, Pickup-Bereiche, Kapselprofil und Routenmodell.

**Update von 7.9.0:** Spiel und Trainer schließen, Helper-DLL und Trainer-EXE gemeinsam ersetzen. Danach die Szene neu laden, damit die neuen Konturen und Pickup-Trigger erfasst werden. Einstellungen/Replays behalten; vorhandener Encoder unverändert. Alte Aufnahmen ohne Triggerdaten verwenden weiter die bisherige Mittelpunktberechnung. Details: **AENDERUNGEN_v7_10_0.md**, **CODE_REVIEW_v7_10_0.md**, **PRUEFUNG.md**.

## Neu in 7.9.0: Export-Pipeline und Aufnahmequalität

- **Überlappender Export:** Drei begrenzte Frame-Plätze überlappen Szenenrendering, asynchrone Unity-GPU-Auslese, native HUD-Komposition und Hintergrund-Encoding. Frame-ID und Quellzeit bleiben über alle Schritte gebunden; Rückstau wartet, statt Frames auszulassen. Die native Komposition verwendet ebenfalls nicht blockierende GPU-Rücklesung. Nicht unterstützte oder fehlerhafte Unity-Auslese fällt auf den bisherigen synchronen Weg zurück. Exportberichte nennen die tatsächliche Auslesemethode, Frame-Zahlen und beobachtete Wartezeiten. Eine konkrete Beschleunigung muss auf dem Zielsystem gemessen werden.
- **Replay → Recording check:** Bewegung, Kamera, Inputs, Collider, Animation/Grafik und Ton erhalten einen verständlichen Status. Details stehen im Tooltip; **Copy recording check** kopiert den Bericht. Unter **Recorded gaps** springt ein Klick zur erkannten Lücke und pausiert. Alte Aufnahmen ohne Erfassungsnachweise werden als **Unknown** angezeigt. Die Anzeige ist unabhängig von VALID / UNVALID.
- **Genauere neue Animationen:** Dynamische Posen werden normalerweise bis 60 Hz beobachtet, bei schneller Bewegung und Animator-Übergängen bis 120 Hz. Gespeicherte Zwischenpunkte richten sich nach der beobachteten Positions-/Winkelabweichung; ruhige Posen benötigen kaum zusätzliche Daten. Spritewechsel werden bis 120 Hz, andere dynamische Grafik-/Lichtwerte bis 60 Hz beobachtet. Tatsächliche Bildrate und Dateigrenzen bleiben maßgeblich; alte Aufnahmen gewinnen keine fehlenden Details.

**Update von 7.8.1:** Spiel und Trainer schließen, `bin/ItStealsHelper.dll` im Spielordner `Mods` und `bin/ItStealsTrainer.exe` ersetzen. Nur eine Helper-DLL behalten. Der Encoder bleibt unverändert. Für Neuinstallationen das vollständige Paket verwenden. Neue Eingabedateien verwenden Format **6**, Frame-Kanal **4**; Clip-Projekte bleiben Format **7**. Alte Eingabeformate 1–5 sowie bisherige Replay-Pakete bleiben lesbar. **AENDERUNGEN_v7_9_0.md**, **PRUEFUNG.md** und **REPLAY_FORMAT.md** enthalten Details.

## Hotfix 7.8.1

Fullbright verwendet wieder eine gleichmäßige **Unlit-Texturansicht** im Trainer und 3D-Replay. Wand-/Bodentexturen behalten, wenn verfügbar, den passenden TOZ-Unlit-Projektionsshader. Shader, Renderqueue und Instancing werden beim Abschalten wiederhergestellt; vorhandene Renderer und statische Mesh-Bereiche bleiben erhalten.

Neue Aufnahmen erfassen zusätzlich den **Kristall-Pickup-Ton**: Die originale `getPoint`-Quelle umgeht den Listener-Aufnahmepunkt. Ein passiver Quellabgriff ergänzt diesen Ton auf derselben DSP-Zeitachse. Spiel-Lautstärke, Bypass-Einstellungen und Originalpuffer werden nicht verändert. Alte Replays erhalten keinen nachträglich erfundenen Sound. Nicht zuverlässig beobachtbare Audio-Routen werden als unvollständig gekennzeichnet.

Das **Update-v7.8.1-ZIP** ist für eine vorhandene 7.8.0-Installation: Trainer und Spiel schließen, beide Dateien aus `bin` an die bisherigen Stellen kopieren (Trainer-EXE sowie Helper-DLL im Spielordner `Mods`). Keine zusätzliche Helper-Kopie behalten. Der vorhandene Encoder bleibt unverändert. Für eine Neuinstallation das vollständige Paket und die untenstehende Anleitung verwenden.

Details: **AENDERUNGEN_v7_8_1.md**, **PRUEFUNG.md**, **CODE_REVIEW_v7_8_1.md**.

## Neu in 7.8.0: Ton, Animation und Monsteranalyse

Neue Runs zeichnen den **tatsächlichen Spielmix** über den Unity-AudioListener auf. Der 3D-Viewer bietet **Recorded game audio** und Lautstärke; die 3D-Vorschau im Clip Studio und Videoexport verwenden dieselbe Run-Zeit und dieselben Tempo-Keypoints. Ein Kameraschwenk ändert den aufgenommenen Hörpunkt nicht. Tempoänderungen verändern auch die Tonhöhe. GIF/PNG und die reine 2D-Vorschau bleiben stumm. MKV/AVI/MOV enthalten Float-PCM, MP4 AAC. Größenlimits berücksichtigen den Ton. Alle drei Binärdateien zusammen ersetzen.

**Monster analysis / recorded game data** zeigt tatsächlich beobachtete Spielzustände und Audioaufrufe mit anklickbarer Run-Zeit. **Show recorded game queries** zeichnet ursprüngliche Raycast-/Linecast-Abfragen im 3D-Viewer; die KI wird dafür nicht erneut ausgeführt. Lichtkontakt, Sichtabfrage und tatsächliche Entdeckung sind unterschiedliche Informationen. Die Timeline ergänzt Tonpunkte und Zustandsmarkierungen.

Animationsaufnahmen ergänzen Animator-Zustände, Übergänge und Blendshape-Gewichte. Abrupte Zustandswechsel werden nicht in vorherige Posen hineingeglättet; animierte Weltobjekte erhalten ihre übergeordneten Posen. Die Wiedergabe verwendet aufgezeichnete Grafikdaten und startet keine Animator-Controller, Animation-Events oder Monster-KI.

Ton liegt in einer zugeordneten **.isaudio**-Begleitdatei: maximal 256 MiB, begrenzter Speicherpuffer, Schreiben im Hintergrund. Aussetzer, Limit und fehlende historische Daten werden angezeigt. Keine Mikrofon- oder Desktopaufnahme. Alte Replays bleiben lesbar; Ton und Animationsdetails lassen sich daraus nicht nachträglich herstellen. Neue Clip-Projekte: **ISCLIP 7**, Frame-Kanal 3. Details: **AENDERUNGEN_v7_8_0.md**, **PRUEFUNG.md** und **REPLAY_FORMAT.md**.

## Neu in 7.7.0: Exportprofile und Fullbright

**Clip Studio → Output**: Share, Best Quality, Archive, Editing Master, Size Limit, Quick Preview, Transparent Overlay, GIF Loop und Windows MP4. **Best Quality** verwendet verlustfreies UtVideo/AVI, **Archive** weiterhin FFV1/MKV. Videoexport mit 30/60/120 FPS; **Normal speed (1x)** übergeht Tempo-Keypoints und behält die Kamerafahrt. **Size Limit** komprimiert einen vollständig gerenderten Clip in zwei Durchgängen auf höchstens die gewählten MB/GB. Dafür ist zusätzlicher temporärer Speicher nötig. Lossless-Dateien besitzen kein beliebiges Größenlimit. Bis Version 7.7.0 waren Videos stumm; neue Aufnahmen ab 7.8.0 unterstützen Ton.

**Fullbright** ist in der 3D-Wiedergabe und im Clip Studio verfügbar, auch pro Keypoint unter Scene layers. Der neue gemeinsame Beleuchtungsmodus behält Originalmaterialien und Transparenz. Die ursprüngliche Beleuchtung wird beim Abschalten wiederhergestellt; Replay-Einstellungen ändern keine Run-Gültigkeit.

Clip-Projekte verwenden jetzt **ISCLIP 6**, alte Versionen 1–5 bleiben lesbar. Auch **PracticeSuiteEncoder.exe** ersetzen: Das Update enthält alle drei Binärdateien und die zum neuen FFmpeg-/x264-Encoder gehörenden Lizenzen/Quellen. Details: **AENDERUNGEN_v7_7_0.md** und **PRUEFUNG.md**.

Die folgenden Versionsabschnitte beschreiben frühere Erweiterungen.

## Neu in 7.6.0: Originalgrafik und Speed-Keypoints

**3D preview / Clip Studio → Camera:** Die Originalgrafik ist jetzt der Standard. **Simplified graphics** schaltet auf den bisherigen Analyse-Stil zurück. Ein Profilwechsel lädt die aufgezeichnete Szene neu. Verwendet werden die installierten Originalmaterialien, Texturen, Modelle, statischen Batch-Bereiche, Lightmaps, Schatten und die Bildeffekte der tatsächlichen Spielkamera. Die Suite enthält keine kopierten Spielassets.

**Neue Aufnahmen** speichern zusätzlich beobachtete Knochenposen, Sprite-/Rendererzustände und veränderliche Lichtdaten, maximal 30 Hz und nur bei Änderungen; statische Mesh-Zustände maximal 10 Hz. Reine Grafik-Proxys spielen diese Daten zeitbasiert ab, ohne KI, Collider, Audio oder Animator-Controller zu duplizieren. Pose-Interpolation funktioniert auch bei Sprüngen und rückwärts. Die originalen Volumetric-Light-Renderpfade bleiben aktiv; ihre reinen Grafik-Updates werden nicht mit der KI eingefroren. Alte Replays erhalten Originalmodelle an ihren Positionsspuren, aber keine erfundenen historischen Animationen.

**Speed-Keypoints:** Einen Kamerapunkt auswählen, **Playback speed at this point** aktivieren und **0.1x–8x** setzen. **Cut** hält das Tempo bis zum nächsten aktivierten Speed-Punkt; **Linear / Smooth / Ease…** führt zum Tempo des nächsten aktivierten Speed-Punkts. Beispiel: Punkt A = 1x / Smooth, B = 0.1x / Cut, C = 1x / Cut. Kamera- und Speed-Übergänge sind unabhängig. Deaktivierte Speed-Punkte übernehmen die laufende Spur. Ohne aktive Speed-Punkte gilt **Default playback speed**. **Use keypoints** muss aktiv sein.

Vorschau und Export verwenden dieselbe Zeitabbildung. Slowmotion verlängert die Ausgabe; 30/60 Ausgabe-FPS bleiben konstant. Die Timeline zeigt weiterhin die originale Run-Zeit. PNG-Sequenzen tragen bei variabler Geschwindigkeit absolute `sourceTimes` im Manifest. Clip-Projekte verwenden **ISCLIP 4**, alte Formate 1–3 bleiben lesbar.

**Grafikgrenzen:** Das ist eine beobachtete visuelle Wiedergabe, keine vollständige Simulation aller Weltzustände. Fehlende alte Modellposen, Shaderzeit, Partikel, Radar-Renderziele und Audio lassen sich nicht aus dem Datenordner rekonstruieren. Für sehr lange Aufnahmen gelten sichtbare Größenlimits; bei erschöpftem Grafikpuffer wird der letzte Zustand gehalten. Das ändert keine Run-Validierung. Neue Grafikdaten werden im begrenzten Speicherbudget der Schreibwarteschlange berücksichtigt. Details: **AENDERUNGEN_v7_6_0.md**, **REPLAY_FORMAT.md**, **PRUEFUNG.md**.

## Neu in 7.5.2: Kamera-Keypoints

Jeder Kamerapunkt speichert Recorded / Follow / Free sowie eigene Replay-Layer. **Use keypoints** bleibt beim Bearbeiten aktiv, sodass Vorschau und Export dieselbe Spur verwenden. **K / Add keypoint**, Liste, Timeline und **Path overview** setzen bzw. wählen Punkte zu exakten Zeitstempeln. Die Vorschau nennt den Zeitpunkt des tatsächlich bestätigten Bildes.

**Free:** rechte Maustaste halten zum Umschauen, dabei WASD bewegen, Q/E senken/heben, Shift schneller; Mausrad bewegt entlang der Blickrichtung, mittlere Maustaste verschiebt. **Follow:** rechte Maustaste dreht, Mausrad zoomt. **Scene layers** ändert beim bearbeiteten Punkt die Darstellung ab dessen Zeitpunkt bis zur nächsten Überschreibung. Pfad und Blickrichtungen sind Editorhilfen und werden nicht in den Clip exportiert. L/R im Maus-HUD sind zentriert.

Details der damaligen Kamera-Erweiterung: **AENDERUNGEN_v7_5_2.md**. Seit 7.6.0 verwenden neue Clip-Projekte **ISCLIP 4**; alte Formate bleiben lesbar. Prüfungen und Live-Testgrenzen: **PRUEFUNG.md**. Die folgenden Abschnitte dokumentieren weiterhin vorhandene Funktionen und frühere Änderungen.

## Neu in 7.2.0: Eingaben, Kamera und Run-Pakete

Neue behaltene Runs erhalten eine eigene `.isinput`-Begleitdatei: WASD, beide Shift-Tasten, R, die vom Spiel gelesenen Bewegungs-/Maus-/Controllerachsen, tatsächlicher Sprint-/Radarzustand, Spieler- und Kameraposen, FOV und tatsächliche Lichtdaten. Update-Ereignisse, Physikschritte und Kameraframes teilen dieselbe monotone Run-Uhr; zusätzliche Sequenznummern unterscheiden mehrere Physikschritte im selben Frame. Speicherung erfolgt auf dem vorhandenen Hintergrundthread. Die bestehende Aufbewahrungsregel bleibt gültig.

Im Tab **Replay** stehen eine Eingabeanzeige, Frame-Schritte und eine anklickbare Ereignisliste bereit. **Export package** schreibt eine ZIP mit Originalaufnahme, Eingabedaten, vorhandenen Notizen und dem ursprünglichen Validierungsbericht. **Copy path** kopiert ihren Pfad. **Import package** nimmt diesen vollständigen Pfad entgegen, prüft CRC/SHA-256, Run-Zuordnung und Begleitdateien und bewahrt vorhandene Aufnahmen bei Konflikten. Neue Begleitdateien liegen neben `run-<ID>.isrun` im Spielordner unter `UserData/ItStealsPracticeSuite/RunReviews`. Der Trainer kopiert sie beim Speichern mit. Archive werden auf einem Hintergrundthread gelesen.

## Neu in 7.5.1: Clip Studio / Input-HUD

Überarbeiteter großer Arbeitsbereich, einfachere Kamerapunkte und Wechsel zwischen Free-/Follow-/Keyframes während des Abspielens. Fit-Vorschau rendert kleiner und vermeidet unnötige GPU-Rücklesungen; Exporte bleiben in voller Auflösung. MKV besitzt ein schneller decodierbares, weiterhin verlustfreies FFV1-Profil mit expliziter Bildrate/Zeitbasis. Input-HUD mit WASD-Anordnung, unmittelbaren Tastenzuständen, optionalem Nachleuchten und einer gemeinsamen **Player activity**-Karte. Einzelheiten: **AENDERUNGEN_v7_5_1.md**; tatsächliche Prüfergebnisse: **PRUEFUNG.md**.

## Neu in 7.5.0: Cinematic-Exporter

**Replay → Export clip / GIF**: Camera / Overlays / Output, direkt steuerbare Kamera, benannte Keypoints mit Schnitten und Easing, frei platzierbare Input-Bausteine, getrennte Exportprofile, RGB-Lossless-MKV, PNG-Sequenzen und transparente Overlay-Assets. **RMB** dreht, **Mausrad** zoomt, **MMB** verschiebt die Kamera. **Add keypoint** speichert die Ansicht am Playhead; der Übergang gilt zum nächsten Punkt. **3 s test export** prüft die gewählte Ausgabe mit dem tatsächlichen Encoder.

Live-Inputs unter **Settings → HUD → Inputs**: einzelne Tasten/Gruppen/Zustände, Mausgröße, Hintergrund/Outline/Schatten und Opazität. Die Live-Konfiguration bleibt unabhängig vom Export. **AENDERUNGEN_v7_5_1.md** enthält Bedienung und Grenzen; **PRUEFUNG.md** die tatsächlich ausgeführten Tests.

## Neu in 7.4.0

- **Visueller HUD-Editor:** Settings → HUD → Edit HUD on screen. Compact-, Speedrun- und Input-HUD auf einer gemeinsamen Vorschau verschieben und an der Ecke skalieren. Fanglinien, Alt zum freien Platzieren, Rückgängig, heller/dunkler Hintergrund, Farben und Schriftwahl. Apply layout übernimmt; Cancel/Escape verwirft den gesamten Entwurf. Während eines aktiven Runs gesperrt.
- **Live-Eingabe-HUD:** Settings → HUD → Inputs. Tastatur/Maus und Controller gemeinsam oder einzeln, kompakte Tasten oder vollständige logische Tastaturansicht, Skalierung, Transparenz und frei wählbare Position. Drei kurze Zustandsmeldungen blenden nach 2,5 Sekunden aus; Zeitangabe wahlweise Run-/Sessionzeit, Spiel-Frame oder Physiktick. Die akzeptierten Spielzustände Sprint, Crouch, Radar, Licht und Uhr sind von beobachteten Tasten getrennt. Die optionale Anzeige folgt auch dem ausgewählten 3D-Replay und hält Abstand zu dessen Steuerleiste.
- **Clip-/GIF-Export:** Replay öffnen, Clip start / Clip end setzen oder vorhandene Marker bzw. A–B-Bereich nutzen, Export clip / GIF öffnen. Start/Ende, Tempo, Auflösung und Bildrate sowie Input-, Zeit-, Speed-, Seed-, Zustands-, Trail- und Monsteranzeige auswählen. 2D-Karte oder 3D-Replayszene mit aufgezeichneter, Verfolger- oder freier Kamera. Die Vorschau verwendet denselben Offscreen-Renderer und dieselbe Komposition wie der Export; für GIF auch dieselbe Palette. MP4 zeigt das unkomprimierte Quellbild vor der H.264-Kompression. Fit/100 %, Scrubbing und Vorschau-Wiedergabe sind verfügbar.

MP4 verwendet weiterhin Windows Media Foundation/H.264. Der mitgelieferte separate FFmpeg-Encoder wird nur für FFV1/MKV verwendet; GIF und PNG schreibt die Suite selbst. Ausgaben landen unter `%LOCALAPPDATA%\ItStealsPracticeSuite\Clips`; nach erfolgreichem Export öffnet Open export folder diesen Ordner. MP4: 720p/1080p/1440p, 30/60 FPS; GIF: 10/20/25 FPS. Clips sind stumm und als REPLAY mit Version/Autor und ursprünglichem Run-Status gekennzeichnet. Exportiert wird die Replay-Analyse, einschließlich ihrer bekannten Rekonstruktionsgrenzen. Ein aktiver Run lässt keinen Export zu.

Die Kodierung läuft mit einer Warteschlange von höchstens zwei Bildern im Hintergrund. Bei Engpässen wartet der Export statt Bilder auszulassen. Abbruch entfernt nur seine eigene unvollständige Ausgabedatei. Bestehende Dateien, Originalreplays und Validierungsberichte bleiben erhalten. Der erste 3D-Export kann die aufgezeichnete Szene laden; dafür muss das Spiel verbunden sein. Für 2D genügt eine im Trainer geöffnete Aufnahme; es wird keine Spielszene gerendert.

**Eingabeabdeckung:** Neue Aufnahmen enthalten logische Windows-Tasten, sieben Maustasten, vier vollständige XInput-Gamepads und die von Unity verfügbaren B0–B19-Tasten für bis zu acht Joysticks. Bei anderen Controller-Protokollen werden die vom Spiel gelesenen Achsen angezeigt, keine unbekannten Triggerbelegungen erfunden. Xbox-/PlayStation-Symbole ändern nur die Darstellung. Die Tastaturbeschriftung ist logisch und US-orientiert, keine Erkennung jedes physischen Tastaturlayouts. Eingaben werden ausschließlich bei Spielfokus und ohne Menü-Eingabeübernahme beobachtet; es gibt keinen globalen Tastatur-Hook. Die Daten sind Beobachtungen und kein manipulationssicherer Eingabenachweis.

Vollständige Gerätedaten stehen erst in **neuen** Aufnahmen zur Verfügung. Formate 1/2 bleiben lesbar; fehlende historische Tasten, Crouch-/Clock-Zustände oder Geräte werden als fehlend/teilweise markiert. Neue Eingabedateien verwenden Format 3, Annotationen Format 2, Einstellungen Format 17. Eigene Clip-Grenzen verändern den A–B-Loop nicht. Der Reportdienst/Discord-Versand bleibt offen und ist in diesem Update nicht enthalten.

Aktuelle Änderungen und Prüfgrenzen: **AENDERUNGEN_v7_6_0.md**, **PRUEFUNG.md**, **REPLAY_FORMAT.md**. Die folgenden Abschnitte dokumentieren weiterhin vorhandene Funktionen und frühere Änderungen.

## Neu in 7.3.5: Rücknahme und neuer Fade-Pfad

Der 7.3.4-Ansatz wurde auf Nutzerwunsch vollständig entfernt: keine GPU-Mesh-Rücklesung, keine Mesh-Aufteilung, keine Ersatzrenderer oder zusammengefassten Ersatzflächen. Die Szene verwendet wieder die Originalrenderer mit ihren ursprünglichen Batch-Bereichen. Details: RUECKNAHME_7_3_4.md.

Die Replay-Kamera behält die Layer-Maske der Spielkamera und fügt nur die eigene Markerebene hinzu. Der Ladevorgang schaltet keine zusätzlichen Radar-/Hilfsebenen frei. Verfolgerkamera und Verdeckungsprüfung verwenden die tatsächliche aufgezeichnete Kameraposition als gemeinsamen Fokus.

Der Alpha-Pfad ist ohne Instancing eingerichtet, passend zu den tatsächlich gelieferten Standard-Shader-Varianten. Wandrenderer erhalten eigene vorbereitete Alpha-Materialien; ihre Materialfarbe wird direkt verändert. Gemeinsame Boden-/Prop-Materialien und indizierte Property-Blocks werden nicht für diesen Fade verändert. Bodenhöhe und tatsächliche Floor-/Prop-/Clock-/Cieling-Layer schützen nicht zu fadende Flächen; horizontale oder ungültige Physiktreffernormalen lösen keinen Wand-Fade aus.

Die Diagnose nennt `walls`, `hits`, `matched`, `faded`; Hitzahlen summieren beide Sichtstrahlen und können deshalb dieselbe Wand zweimal erfassen. Das Log nennt den echten Kameramaskenwert und gewählten Alpha-Shader. Die Zahlen sind CPU-/Physikbeobachtungen und kein Beweis einer korrekt gerasterten GPU-Ansicht.

**Moderation:** Der unabhängige Replay-Prüfer ist als gewünschter nächster Ausbau dokumentiert, noch nicht implementiert. Weitere Vorschläge stehen in MODERATION_PLAN.md.

## Neu in 7.3.2

**Verfolgerkamera:** Leiste mit **Tab** schließen. **Rechte Maustaste halten und ziehen** dreht horizontal und vertikal; **Mausrad hoch** zoomt hinein, **herunter** heraus. Separate Abstands-/Höhenregler entfallen. Zoom und Winkel bleiben bei Pause, Zeitsprüngen, Loops und gewöhnlichen GUI-Befehlen erhalten. Zoom ist auf 0,75–20 u begrenzt. **Backspace** richtet einmal neu aus. Die Kamera bewegt sich weiterhin ohne Wandkollision.

**Hide ceiling** blendet Decken-Materialabschnitte aus. **Fade blocking walls** lässt in der Verfolgeransicht tatsächlich getroffene Wand-/Tür-Renderer weich transparent werden. Eine Hin-/Rückprüfung berücksichtigt auch Kameras innerhalb einer Wand. Die Sichtprüfung läuft höchstens mit 20 Hz; Materialarrays und Collider-Zuordnungen entstehen nur beim Laden. Ab 7.3.5 wird Alpha direkt an vorbereiteten, renderer-eigenen Materialien gesetzt. Meshes, statische Batch-Bereiche, physische Collider und aufgezeichnete Daten bleiben erhalten. Beide Optionen sind rücksetzbare Replay-Darstellung; die Wandtransparenz gilt nur für die Verfolgerkamera.

Die echte **Spieler-Hitbox im 3D-Replayer** hat einen 2,5-Pixel-Kern mit dunkler 4,5-Pixel-Kontur bei 1080p. Gezeichnet wird die ursprüngliche Form mit besser sichtbarer Strichstärke; die Maße werden nicht vergrößert. Die Linie wird an der Kamera-Nahgrenze sauber abgeschnitten.

**Installation:** Hauptprotokoll **19**, DLL und EXE gemeinsam ersetzen. Einstellungen und Originalreplays bleiben erhalten. Datei-/Map-/Replay-Paketformate ändern sich nicht. Vorschläge zur nächsten Moderationsstufe stehen in **MODERATION_PLAN.md**; sie sind kein Teil der aktuellen Run-Validierungslogik.

## Neu in 7.3.1

Die **Verfolgerkamera** hat eine eigene Orbit-Ausrichtung: **Tab** schließt die Leiste, **rechte Maustaste halten und ziehen** dreht um den Spieler. Spieler-Drehungen, Seek und A–B-Schleifen ändern diesen Winkel nicht. **Backspace** richtet einmal an der aufgezeichneten Blickrichtung aus. Die Kamera passiert Wände ohne Kollisionsabfragen; Der Zoom wird ab 7.3.2 am Mausrad bedient.

Die **Geschwindigkeitsspur** ist ein breites Band mit dunkler Kontur. Gemessene **0–10 u/s** laufen weich von Dunkelrot über Rot/Amber nach Türkis; ab 10 bleibt Türkis erhalten. Die nominale Breite beträgt sechs Pixel bei 1080p und skaliert mit der Ansicht. Lücken und Teleports werden weiter nicht verbunden.

Die **Collider-Anzeige** besitzt zusätzliche Shaderpfade statt eines stillen Ausfalls bei fehlendem internem Shader. Aufnahmen erfassen auch Spieler-Collider außerhalb des Body-Unterbaums und separat gebundene Monsterkörper. Die Optionen zeigen die tatsächlich vorhandenen Spuren je Gruppe. **Colliders through walls** ist zunächst aktiv, damit kleinere echte Collider nicht innerhalb der Replay-Marker verschwinden. Ältere Aufnahmen bleiben nutzbar; fehlende historische Formen benötigen eine neue Aufnahme.

## Neu in 7.3.0: 3D-Replay-Arbeitsplatz

**3D preview** verwendet die ursprünglichen Level-Renderer und ihre statischen Batch-Bereiche. Eine schattenfreie Richtungsbeleuchtung mit Umgebungslicht, getrennte Flächenfarben und dezente Farbvarianten machen Wandflächen und Ecken besser unterscheidbar. Standard/Diffuse wird nur bei Unterstützung verwendet; eine einfache unbeleuchtete Darstellung bleibt als Fallback verfügbar. Diese damalige Darstellung bleibt seit 7.6.0 unter **Simplified graphics** verfügbar; der Standard verwendet Originaltexturen, Schatten und Kamerabild-Effekte.

Die Ansicht öffnet sich nach erfolgreichem Laden automatisch mit einer großen Eingabeanzeige unten mittig. **WASD, Shift und R** zeigen die aufgezeichneten Tasten, **SPRINT/RADAR** den akzeptierten Spielzustand; ein Blickindikator zeigt beobachtete Maus-/Controllerachsen. Die Geschwindigkeit stammt aus den tatsächlich aufgezeichneten Positionen/Zeitstempeln. Die Ansicht bleibt als **REPLAY / 3D**, **v7.6.0 / mab** gekennzeichnet.

| Bedienung | Funktion |
| --- | --- |
| **Tab** | 3D-Bedienleiste ein-/ausblenden; Maus freigeben/zurückgeben |
| **Space** bei Spiel-Fokus | Wiedergabe/Pause |
| **Pfeil links/rechts** bei Spiel-Fokus | Tatsächliches vorheriges/nächstes Kameraframe |
| **Home/End**, **+/−** bei Spiel-Fokus | Anfang/Ende, Tempo ändern |
| **M** bei Spiel-Fokus | Benannte Markierung am angezeigten Zeitpunkt anlegen |
| **F3** | Vollständiges Trainer-Menü |
| **Escape** | 3D verlassen; bei offener Bedienleiste zunächst Steuerung schließen |

Die Bedienleiste enthält dieselbe Timeline wie die Kartenansicht: anklickbare Pickups, Flashes und orangefarbene eigene Markierungen, Ziehen und Mausrad zum Spulen, Einzelbildschritte, **0,1×–8×** stufenloses Tempo und direkte Sekunden-Eingabe mit **Go/Enter**. Kamera-/Darstellungsänderungen positionieren die laufende Replay-Uhr nicht neu. Bei kleiner Darstellung ist die Leiste scrollbar.

**Set A**, **Set B**, **A–B loop** wiederholen einen Ausschnitt über die monotone Uhr im Spiel, auch bei geschlossenem Menü. Eigene Marker lassen sich benennen, verschieben oder entfernen; sie sind persönliche Annotationen, keine nachträglich erzeugten Spielereignisse. Marker und Loop-Bereich werden separat in `.ismarks` gespeichert und beim Paketexport mitgenommen. **Markers / loop** in der Kartenansicht bietet dieselben Werkzeuge.

**3D options / View options** bietet aufgezeichnete Kamera, Verfolgerkamera mit unabhängig drehbarem Orbit und Mausrad-Zoom ohne Wandkollision sowie freie Kamera. Frei bewegen: Leiste schließen, WASD/Q/E, Shift und rechte Maustaste. Eine kurze, farbige **Speed trail** zeigt gemessene Bewegung; sie verbindet weder Kameralücken noch Teleports.

**Actual colliders:** Neue Runs zeichnen echte unterstützte Box-, Kugel-, Kapsel- und CharacterController-Formen mit Dimensionen, Transformationen und Aktivierungs-/Triggerzuständen auf. Spieler, Monster und Kristalle lassen sich getrennt anzeigen, optional durch Wände. Der Spieler folgt den Physikbeobachtungen, andere Quellen werden mit begrenztem Aufwand bis zu 10 Hz beobachtet. Nicht unterstützte Mesh-/Terrain-Collider werden ausgelassen, ohne Ersatz durch ungefähre Boxen. **Loaded level colliders** zeigt unterstützte solide Primitive der geladenen Szene; dieser Schalter ist keine aufgezeichnete Geschichte beweglicher Türen. Alte Aufnahmen behalten Kamera/Eingaben/Marker/Loops; fehlende frühere Collider-Daten werden ausdrücklich angezeigt.

Das Originallevel wird nötigenfalls geladen; Build, Spawn und Kristalle werden geprüft. Die tatsächlichen Culling-Kataloge des Spiels werden rücksetzbar sichtbar. Andere inaktive Varianten und gesammelte Ziele bleiben erhalten. Originale Spiel-Updates, Koroutinen, skalierte Zeit und Ton pausieren; ihre vorherigen Zustände werden bei jedem Ende/Fehler wiederhergestellt. Auch Materialien, Property-Blocks, Lightmaps, Probes, Umgebung, Kameras und Lichter werden gesichert. Aktive Runs sperren die Analyse.

Die sichtbare 3D-Geometrie und reale Frametimes benötigen weiterhin den Test im laufenden Spiel. Seit 7.6.0 werden neue Monsterposen mit aufgezeichnet. Radarbildschirm, Ton und vollständige dynamische Weltzustände werden weiterhin nicht vollständig rekonstruiert.

Ältere Replays bleiben mit ihren vorhandenen Daten verwendbar. Fehlende frühere Eingaben/Blickbewegungen werden nicht erfunden. Begrenzte Speicher und ausdrücklich sichtbare Aufnahmeflags verhindern unbegrenzten RAM-Verbrauch. Lokale Eingaben und unsignierte Prüfsummen sind eine Hilfe für Moderatoren und kein manipulationssicherer Herkunftsnachweis. Details: `REPLAY_FORMAT.md` und `PRUEFUNG.md`.

## Oberfläche 7.1.0

- Animierte Seitenleiste mit eigenen skalierbaren Vektorsymbolen; in schmalen Fenstern als Symbolleiste. Farben und reduzierte Bewegung folgen den Einstellungen.
- Cheats als kompakte Funktionskarten mit eindeutigen Zuständen und Sperren. Freecam bleibt über **F1 im Spiel** bedienbar.
- Seed-Suche mit laufender Aktivitätsanzeige, Kennzahlen und kurzen Markierungen neuer Top-Ergebnisse. Die Suche und ihre Bewertung bleiben unverändert.
- Replay mit großer Karte und gemeinsamer Timeline darunter. Pickup-Rauten und Lichtmarkierungen sind anklickbar; Ziehen, Pfeiltasten, **Shift** für 1 s sowie **Home/End** springen durch die Aufnahme. Die Wiedergabe pausiert beim manuellen Springen.
- Minimap mit vier zum Kartenwinkel passenden Himmelsrichtungen, lesbarer Richtungsangabe, Spieler-Halo und klarerem Wand-/Bodenkontrast. Hide & Seek / Outside bleiben WIP.
- Dezente Schatten, Farbverläufe, Kanten und Routenmotive. Verständliche leere Ansichten und animierte Lade-Platzhalter ohne erfundene Messdaten.
- Erst nach abgeschlossener Ergebnisdarstellung erhält ein tatsächlicher gültiger lokaler PB ein animiertes Abzeichen, seine Differenz und einen eigenen leisen Ton. Ton und Bewegung sind abschaltbar; die bestehende Run-/Fokus-Policy bleibt wirksam.

Die Vorschau unter **tests/previews-7.1.0** zeigt die damalige echte ImGui-Zeichenausgabe mit synthetischen Testaufnahmen. Das ist keine Windows-/Unity-Liveaufnahme. Vorhandene Einstellungen und ältere Replay-Dateien bleiben lesbar; DLL/EXE benötigen gemeinsam das aktuelle Hauptprotokoll 17.

## Technische Finish-Details 7.0.3

Die Prüfsequenz dauert jetzt 5,04 sichtbare Sekunden und zeigt **42 technische Felder** aus dem eingefrorenen Run-Snapshot. Auf ausreichend großen Flächen (ab 1620×900 logischen Pixeln) steht rechts eine eigene **Technical scan**-Ansicht; auf kleineren Flächen öffnet der feste Knopf unten die Detailansicht. Die vorhandene native Schrift bleibt erhalten.

Die Ansicht folgt dem gerade präsentierten Check. Ein Klick auf eine der sieben Prüfzeilen hält diese Auswahl fest; **Follow scan** kehrt zur Automatik zurück. Nach Abschluss erscheint automatisch der erste fehlgeschlagene Check, bei vollständig gültigen Checks der Build. Der Evidence stream zeigt die zuletzt in der Animation vorgestellten Daten. **Copy report** kopiert sämtliche Werte, den vollständigen Datei-SHA256, die Run-ID und alle Check-Ergebnisse.

Zusätzlich sichtbar: verwendete/bei Aufnahme aktive Hilfen, initialer RNG-Seed, Start-/End-UTC-Ticks, Scene/Epoch, Zeitspanne der UTC-Uhr, Aufnahme-ID und Samplezahl, vollständiger Datei-Hash in zwei Hälften, Helper-/Trainer-Version, ursprüngliche Helper-Begründung, Monitor-Statuscode, PID/Sitzung, erfasster Eingabezustand und IPC-Größen. Momentaufnahmen sind als Captured beschriftet. UTC-Zeitspanne ist weder der Spieltimer noch eine neue Gültigkeitsregel. Der Datei-Hash beschreibt die Datei auf dem Datenträger, keine Prüfung des laufenden Speichers. Die zusätzlichen Daten werden einmal pro Ergebnis zusammengestellt; die Animation führt keinen weiteren Prozess-/Speicherscan aus.

Analyse Run, Replays, X, Seed-Kopie, PB-Differenz und dauerhaftes Ergebnis bleiben erhalten. Aktive Runs unterdrücken die Anzeige weiterhin. Reduzierte Bewegung zeigt die Daten sofort; Ausblenden pausiert die Sequenz. Intro, Routing, Minimap, Seed-Suche und Aufzeichnung sind unverändert.

## Finish-Screen 7.0.2 (weiter enthalten; Dauer siehe 7.0.3)

Das dauerhafte Ergebnisfenster zeigt eine 3,32 Sekunden lange Prüfsequenz mit sieben Segmenten, aktiver Scan-Zeile und wechselnden tatsächlichen Run-Daten: Hilfen-/Warnmasken, Seed-/RNG-Nachweis, beobachteter Start, gestoppte Zeit, Build-Hash, Spiel-/Unity-Version, lokale Tool-Prüfung und HUD-/Befehlsbeobachtung. Danach stehen grüne **[VALID]**- bzw. rote **[INVALID]**-Tags neben weißen Beschriftungen. Fehlende erforderliche Nachweise bleiben ungültig; Tooltips zeigen die vollständigen Werte. Die Animation präsentiert die beim Run erfassten Beobachtungen und startet keinen neuen Speicher-/Prozessscan. Ausgeblendete Fenster pausieren den Ablauf; ausgeschaltete Animationen zeigen sofort alle Ergebnisse. Ein einzelner leiser Abschlusston respektiert die bestehenden Ton-/Fokus-/Run-Regeln.

Große finale Zeit, tatsächliche vorherige PB-Differenz, Modus und Layout-Seed bleiben sichtbar. **Copy seed** kopiert genau den erfassten Layout-Seed. **Analyse Run** ist jetzt direkt im festen unteren Bereich des Ergebnisfensters verfügbar, solange die passende Aufnahme existiert; die Aktion endet nicht nach 15 Sekunden. Sie öffnet ausschließlich diese Run-ID und diesen Modus, gegebenenfalls aus dem gespeicherten Archiv nach Szenen-/Sitzungswechsel oder Trennung. **Replays** öffnet die Liste. Speedrun mode bleibt aktiv. Bei fehlender Aufnahme steht der Grund am deaktivierten Analyse-Knopf; es wird kein älterer oder neuerer Run eingesetzt.

**X**, Analyse und Replays bleiben auch bei kleinen Fenstern erreichbar; nur der Mittelteil scrollt. Das Ergebnis bleibt bis zur bewussten Aktion erhalten. Die getrennte **F8**-Einladung bleibt wie bisher 15 Sekunden und endet beim Szenenwechsel. Die technischen Details haben eine eigene native 16-px-Schrift bei 100 % DPI, unabhängig von der optionalen HUD-Skala. Intro, Routing und Movement-Lernen behalten ihr Verhalten.

## Korrektur 7.0.1 (weiter enthalten)

Das Intro wartet auf eine frische Helper-Verbindung und eine erfolgreiche sichtbare Bildausgabe. Ein aktiver Run, Laden oder ein verdecktes Fenster verbrauchen den Start nicht mehr. Bei Alt+Tab pausiert die Anzeigezeit. Auch mit ausgeschalteten optionalen HUDs erhält das Intro seine eigene Anzeigeanforderung. Deaktivierte Animationen ergeben ein ruhiges Standbild; der Schalter Start / exit graphic kann das Intro weiterhin vollständig abschalten.

Das Run-Ergebnis bleibt ohne Zeitablauf bis zum bewussten Schließen über **X** sichtbar, auch nach dem automatischen Wechsel in die Endszene. Der Cursor wird für dieses Fenster freigegeben. X stellt die vorherige Menü-/Spielansicht wieder her. Bei kleinen Viewports bleiben Kopfzeile und X sichtbar, die Details sind scrollbar. Während eines neuen aktiven Runs werden Ergebnis und Eingabeübernahme unterdrückt; das gespeicherte Ergebnis geht dabei nicht verloren. Die getrennte Analyse-Abfrage behält ihre 15-Sekunden-Grenze.

## Funktionen seit 7.0.0

- Sechs Bereiche: **Cheats, Seed Map, Speedrun, Settings, Replay, Diagnostics**. Overview entfällt. Refresh map geometry liegt in Diagnostics.
- Größere, in der benötigten Pixelgröße gerasterte Schriften: bei 100 % DPI Menü 18 px, HUD 19 px, HUD-Labels mindestens 16 px bei Standardskalierung, Timer 44 px, Intro-Titel 32 px und Intro-Text 20 px. HUD-Skala mindestens 0,85. Nach Skalierungsänderungen wird der Atlas zwischen Frames neu erstellt; reine Farbänderungen lösen keinen Neuaufbau aus.
- Lesbare HUD-Karten mit großer Zeit, zweispaltigen Messwerten und einer eigenen Koordinatenzeile, wenn genügend Platz vorhanden ist. Bei sehr kleinen Viewports bleibt die Schrift lesbar; zusätzliche Messwerte werden mit einem Hinweis auf die Layout-Einstellungen ausgeblendet. Der Pflichtstatus bleibt sichtbar.
- **Shift+T** schaltet Speedrun mode ein/aus; eine animierte Meldung und ein kurzer eigener Ton bestätigen die Änderung. Einschalten über die Taste startet den aktuellen unterstützten Modus als sauberen Zufallsrun. T ohne Shift startet wie bisher neu.
- **Nach dem Finish:** finale Zeit, vorherige lokale PB und echte Differenz, Layout-Seed und sieben animierte Prüfgruppen. **Analyse Run? [F8]** bleibt 15 Sekunden sichtbar. F8 öffnet genau diesen Versuch im Replay-Tab, ohne Speedrun mode abzuschalten. Neuer Run, Scene-/Sitzungswechsel und Ablauf entfernen die Abfrage. Das Ergebnis bleibt bis X sichtbar. Die Karten bevorzugen freien Platz neben HUDs; auf kleinen Viewports nutzt das Ergebnis einen begrenzten Dialog mit scrollbaren Details. Nur zeitlich begrenzte Hinweise dürfen bei Platzmangel entfallen.
- Replay ist während eines aktiven Runs gesperrt. Beim neuen Start schließt eine offene Analyse automatisch.
- **Aufzeichnungsregel:** standardmäßig werden nur Versuche mit einer Dauer **größer als 10 Sekunden** dauerhaft aufgenommen. Alternativ nach mindestens X tatsächlich erfassten Kristallen. Unter Replay → Recording rule einstellbar; die Regel wird beim Run-Start festgehalten. Kürzere Versuche sind nur vorübergehend analysierbar und werden beim Neustart verworfen. Statistik und Integritätsdaten laufen davon unabhängig weiter. Vorhandene Aufnahmen werden nicht nachträglich entfernt.
- Keine Replay-Löschaktion im GUI. Qualifizierte Versuche erscheinen automatisch in der Liste. Rename / notes ergänzt Name, Kategorie und Notizen; es verändert keine aufgezeichneten Run-Daten. Dateien außerhalb der Anwendung sind dadurch nicht gegen Löschen oder Manipulation geschützt.
- **Diagnostics → Run diagnostic scan:** verständliche Codes, Beobachtungen und konkrete Vorschläge für Verbindung, Cursor, Renderer, gemessene CPU-Arbeit, FPS/VSync, RNG, Geometrie, Speicherung und UI-Audio. Copy scan result kopiert den Befund mit technischen Angaben. Technical details bleibt eingeklappt. Der Scan verwendet vorhandene Messungen; er führt keinen Belastungstest aus und verändert keine Systemeinstellungen.

## Seed Map

Eine Karte einmal laden, damit Spielkatalog, Generator und Geometrie erfasst werden. Die Suche bewertet immer vollständige Routen und läuft nach Start search bis Stop search oder bis der 32-Bit-Suchraum ausgeschöpft ist. Keine feste Suchzahl im GUI. Die vorhandene CPU-/Game-Thread-Pipeline, Lastregelung, einstellbare Worker und Navigationscache bleiben erhalten. Bei Kontextwechsel oder sauberem Speedrun wird die Suche gestoppt.

Der Spawn-Filter zeigt eine kleine Karte; der gewählte Spawn pulsiert grün mit gestricheltem Ring. **Show** in der Top-Liste öffnet **Preview & replay** und hält genau das gewählte Ergebnis fest, auch wenn die Suche weitere Ergebnisse liefert. Eine neue Vorschau startet mit ausgeschalteter animierter Optimalroute. **Use displayed seed** überträgt Karte, Seed und Seed-Typ gemeinsam in SET Practice, ohne das Spiel neu zu laden. **Use best seed** tut dasselbe mit dem besten Ranglistenergebnis. Danach mit T oder dem Speedrun-Startknopf laden. Die zusätzlichen früheren Übungs-/Minimap-Routenknöpfe entfallen.

**Hide & Seek / Outside: WIP** bei Minimap, Karte und Seed-Suche. Die verbleibenden Wandkontur-/Durchgangsprobleme sind nicht als gelöst gekennzeichnet. Classic, Camera und die anderen Modi verwenden weiterhin die bestehende gemeinsame Routing-Pipeline. Bei fehlender bestätigter Navigation werden ungesicherte Wegsegmente nicht durch Wände gezeichnet. Das Modell garantiert keine weltweit schnellste reale Route durch dynamische Hindernisse.

## Replay und Zeitschätzung

Die Liste zeigt Modus, Run-Zeit, Seed und **VALID / UNVALID** für abgeschlossene Runs bzw. orange **ABORTED** bei Reset, Tod oder Unterbrechung. Die unabhängige Integritätsbewertung steht im Tooltip und bleibt gespeichert. Suche nach Name, Kategorie, Modus oder Seed. Pinke Spielerroute und türkisfarbene Vorschlagsroute sind unabhängig schaltbar, animiert, pausierbar und über eine gemeinsame Zeitleiste wiederholbar. Expand map vergrößert das Fenster.

Rote Monster und ihre dünnen gestrichelten Navigationspfade folgen der erfassten Zeit. Weiße Pulse markieren tatsächliche Lampenaktivierungen, Kamerablitze und bestätigte Kontakte, auch bei einem verfehlten Blitz. Der Radius ist symbolisch. Alte fehlende Ereignisse lassen sich nicht rekonstruieren. Details zeigt Ereigniszahl, erreichte Beobachter und verworfene Samples.

Die Prognose lernt während Speedruns aus tatsächlichen Eingaben und Physikverschiebungen: schnelle Gerade, sanfte/enge Kurven, Steigung und Anlaufen getrennt. Menüs, Freecam, Gehen, Sprünge, Teleports, blockierte oder unplausible Bewegung liefern keine Lernfenster. Das Modell ist an Modus, Build, Physikschrittzeit und Spielermaße gebunden und wird mit Prüfsumme/Backup im Hintergrund gespeichert. Replays erzeugen kein Referenzprofil; ihr Öffnen trainiert die Schätzung nicht. Unbeobachtete Klassen nutzen die erfasste Spielmechanik. Ohne Messprofil nutzt die Schätzung den erfassten Sprintweg pro Physikschritt; ohne diesen die gekennzeichnete 8,1–10-u/s-Spanne. Gegnerwartezeiten und dynamische Eingriffe sind nicht vorhersagbar. Details rekonstruiert die gespeicherte tatsächliche Run-Zeit separat.

Die bisherige 2D-Spielerroute wird mit maximal 50 Hz erfasst, Monster zunächst mit 10 Hz und geprüften linearen Schlüsselbildern, Monsterpfade höchstens 2 Hz und vier nativen Pfadkopien pro Frame. Die neue separate Kamera-/Eingabeaufnahme folgt den tatsächlichen Render-/Update-/Physikschritten. Langzeitdaten und Dateien haben Größenlimits; bei Reduktion warnt Replay Details. Datei-I/O und Archivprüfungen laufen im Hintergrund; Unity-Objektabfragen bleiben auf dem Game-Thread und werden begrenzt.

## HUDs und Gestaltung

Settings → **Appearance**: Hintergrund, Text, Buttons, Animation/Hover, Outlines; Hex-Eingabe und Zurücksetzen pro Farbe. Zehn Systemschriftfamilien, soweit installiert, getrennt für Menü, HUD und Timer. Settings → **HUD**: gemeinsame/eigene Palette, Compact/Speedrun-Layout, Größe, Position, Alpha, Abstände und Reihenfolge. Änderungen gelten auch bei geschlossenem Menü. Der grüne/rote Pflichtstatus behält seinen eigenen Kontrast.

Compact kann zusätzlich **Rotation, Player state, Monster state, Monster speed, Player noise und Health** anzeigen. Speedrun erhält von diesen neuen Werten ausschließlich **Rotation**; bestehende Koordinaten und Run-Statistiken bleiben verfügbar. Sauberer Speedrun unterdrückt weiterhin die bestehende Bewegungs-/Gegnerhilfe. Fehlende oder alte Telemetrie wird als -- angezeigt. Monster speed verwendet verfügbare Spiel-Felder bei Hide & Seek/Ghost; für Classic/Timer wird keine Geschwindigkeit erfunden.

Settings → **Minimap**: Follow/Übersicht, Player-Up/North-Up, Zoom, Radius, Position, Größe, Intensität und Filter. Der Trail hält höchstens 128 Samples bei 10 Hz. Player-Up berücksichtigt die tatsächliche Vorwärtsachse playerBody.right. Scene-/Teleportwechsel setzen die Interpolation zurück. Die passive Anzeige überträgt nur belegte, zusammengefasste Regionen statt fortlaufend den ganzen Bildschirm.

Settings → **System**: UI-Ton/Lautstärke, Animationen, Start-/Exit-Grafik und temporäre Spiel-FPS-/VSync-Overrides. Die Overlay-Rate steuert ausschließlich den Trainer. Ohne Override bleiben Spielwerte unverändert. Änderungen werden bei Trennung/Beendigung zurückgesetzt. Die Startgrafik erscheint nach der ersten erfolgreichen sichtbaren Ausgabe für 2,6 s mit größerer Schrift und optional einmalig weichgezeichnetem Ausschnitt hinter der Intro-Karte; keine laufende Bildschirmaufnahme. Exit dauert 0,24 s. Animationen und Grafik sind abschaltbar und während Runs unterdrückt. Eigene leise Töne verändern keinen Spiel-/Windows-Mixer; Hover bleibt still, aktive Runs und Alt+Tab bleiben stumm. Ein kurzes Shift+T-Signal darf auch bei fokussiertem Spiel erklingen, solange noch kein aktiver Run läuft. Veraltete Signale werden verworfen.

## Cheats und Freecam

**Show actual colliders** ist die einzige Hitbox-Option: tatsächliche Box-, Sphere-, Capsule- und CharacterController-Geometrie. Nicht unterstützte Mesh-/Terrain-Collider werden ausgelassen, ohne Bounding-Box-Ersatz. Hide player hitbox bleibt ein Filter. Freecam nutzt eine getrennte Kamera ohne Collider; der echte Spieler bleibt stehen und Kristall-Pickups sind gesperrt. Freecam mit sichtbarer Spielerhitbox erlaubt die Untersuchung des echten Körpers von außen.

## Tasten

| Taste | Funktion |
|---|---|
| F1 | Freecam, nur Normal/Practice und Spiel-Fokus |
| F2 | Optionale Anzeigen ein/aus; Pflichtstatus bleibt |
| F3 | Menü öffnen/schließen |
| F4 | Compact HUD / Menü in Normal/Practice |
| F5 | Geometrie aktualisieren; auch in Diagnostics |
| F6 | Unity-Borderless anfordern |
| F7 | Live-Inspector-Picker starten oder abbrechen |
| F8 | Fertigen Run analysieren, solange die 15-s-Abfrage sichtbar ist |
| Shift+T | Speedrun mode umschalten |
| T ohne Shift | Ausgewählten Speedrun-/Practice-Level neu starten |
| Escape | Tatsächlicher Spielmenüwechsel bricht den Run ab |
| WASD / Q / E | Freecam bewegen |
| Shift / rechte Maustaste | Freecam schneller / drehen |

Passive Anzeigen nehmen keinen Eingabefokus. Menü und Ergebnisfenster geben den Cursor für ihre Bedienung frei. Alt+Tab verbirgt die Anzeige; bei Rückkehr wird die Eingabeübernahme erneuert. Bei verweigertem Windows-Fokus wird das Menü geschlossen. Windowed/Borderless sind die vorgesehenen Betriebsarten.

## Validierung und Nachweise

Lokale Prüfungen erfassen beobachteten Start, Seed/RNG, Trainerhilfen, Timing/Spielerzustand, unterstützten Datei-Build, Hooks, lokale Tool-Erkennung und den Pflicht-HUD. Tatsächlich aktivierte Hilfen und Fehler bleiben für den ganzen Run gesetzt. Verbotene Cheat-Befehle werden vor Unterdrückung protokolliert; eine während eines Runs abgewiesene Replay-Anforderung aktiviert keine Hilfe. Nur ein nach lokalen Regeln gültiger abgeschlossener Zufallsrun kann eine lokale PB erhalten. Die Abschlusskarte zeigt die erfassten Beobachtungen; fehlende erforderliche Daten können keinen grünen Check ergeben.

**VALID ist keine manipulationssichere Anticheat-Attestierung.** Versteckte/umbenannte Memory-Tools, native Änderungen und Dateien außerhalb der Suite können damit nicht vollständig überprüft werden. Ein Archiv ohne Löschknopf ersetzt keine externe Wettbewerbsprüfung.

Settings Format **18** liest alte Einstellungen, erhält eigene Farben/Positionen und migriert alte Standard-HUD-Breiten für bessere Lesbarkeit. Beschädigte Originale, Backups und unbekannte zukünftige Formate bleiben geschützt. Aktuelle Details, Prüflogs und Vorschauen: **PRUEFUNG.md**, **tests/results-7.6.0**, **AENDERUNGEN_v7_6_0.md**. Kein Windows-/Unity-Livetest auf diesem Host; daher keine Zusage realer Spiel-FPS oder live geprüfter 3D-/Grafik-/Fokus-/Audio-Treiber.

Mesh-Wire-Präzision: Primitive werden live gelesen. Nicht-konvexe lesbare Mesh-Collider zeigen einen begrenzten Source-Mesh-Snapshot vom Auswahl-Frame; bei Topologieänderungen neu auswählen. Ausgetauschte Mesh-Referenzen werden nicht mit alten Kanten gezeichnet. Cooked-Convex-Hüllen sind nicht verfügbar und werden ohne Ersatz ausgelassen.
