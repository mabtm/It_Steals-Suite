# PracticeSuite 7.5.1 / mab

## Clip Studio

Große Vorschau mit gemeinsamer Timeline, klarer Trennung von **Camera**, **Overlays** und **Output**. Auf kleinen Fenstern wechseln **Preview / Settings** zwischen den Ansichten. Tastaturkürzel werden beim Schreiben in Textfelder unterdrückt.

- **RMB halten:** Kamera drehen; **Mausrad:** zoomen; **MMB halten:** verschieben. Diese Gesten und Kameramoduswechsel pausieren die Clip-Wiedergabe nicht mehr.
- **Recorded / Follow / Free / Keyframes:** direkte Auswahl. Beim Wechsel von Free/Keyframes zu Follow bleibt die aktuelle Bildgestaltung erhalten und folgt ab dann dem Spieler. Free übernimmt die aktuelle Kamerapose und bleibt beim Scrubben an dieser Position. Verspätete Frames eines vorherigen Modus setzen die Kamera nicht zurück.
- **K / Add keypoint:** aktuelle Position am Playhead als Shot A, B, C … übernehmen; der Modus bleibt erhalten. Punkt in der Liste oder als Diamant unter der Timeline auswählen: die Freecam bearbeitet direkt diesen Punkt. Kamerabewegungen/FOV werden sofort übernommen. **Space / Play** beendet die Punktbearbeitung und spielt die Kameraspur. **Done editing** beendet die Bearbeitung ohne Moduswechsel.
- **To the next shot:** Cut, Linear, Smooth, Ease in, Ease out oder Ease in/out. **Update selected** übernimmt explizit die aktuelle Pose; **Remove point** entfernt ausschließlich Schnittmetadaten. Bis zu 256 Punkte.
- Timeline umfasst die ganze Aufnahme; **[ In / Out ]** setzen den Exportbereich. **Pfeil links/rechts:** einen Ausgabeframe in der Quellzeit vor/zurück. Play startet innerhalb des gewählten Bereichs; beim Seek bleibt Free frei positioniert.
- **Overlays:** sichtbare Gruppen, Auswahl einer Ebene, Position/Größe und unabhängige Ebenenopazität. Die Stiloptionen zeigen passend zur gewählten Eingabeebene nur die relevanten Geräteeinstellungen. Der Stil ist innerhalb des Clips gemeinsam, Live-HUD-Einstellungen bleiben unabhängig.
- Fit-Vorschau: 640/960/1280 Pixel Breite je nach sichtbarer Fläche. **100%** und Exporte verwenden die gewählte vollständige Auflösung. Die 2D-Vorschau wird direkt im GUI gezeichnet; 3D-Vorschaubilder benötigen keinen zusätzlichen D3D-Staging-Readback, außer für die GIF-Palettenvorschau. Der Szenenpuffer wird wiederverwendet. Das notwendige Unity-ReadPixels für den Prozesskanal bleibt bestehen.

## MKV / FFV1

Die bisherige Konfiguration konnte trotz korrekter 60-FPS-Zeitstempel beim Abspielen zu langsam decodiert werden. Das neue weiterhin verlustfreie Profil verwendet Rice-Coding, kleines Kontextmodell, zwölf Slices und vier Encoderthreads. Bildrate und Encoder-Zeitbasis sind explizit gesetzt. Dateien werden größer; die Software-Decodierung war im Host-Vergleich deutlich schneller.

Die Exportzeit ergibt sich weiterhin ausschließlich aus Frameindex / FPS; die Quellzeit aus Start + Index × Geschwindigkeit / FPS. Auf langsames Rendern wird gewartet: Exportframes werden weder nach Echtzeit verworfen noch zur FPS-Kosmetik vervielfacht. Gaps oder niedrige Abtastraten der Originalaufnahme werden dadurch nicht nachträglich rekonstruiert.

Ein Vergleich mit 90 synthetischen 1080p-Frames und gleicher FFmpeg-8.1.3-Quelle ergab bei zwei Decoderthreads etwa 10,6 → 33,8 FPS und bei sechs etwa 40,1 → 123,0 FPS. Dies sind Host-Messwerte, keine Garantie für einen bestimmten Player/PC. Das vollständige neue Produktionsprofil wurde separat bei 30/60 FPS auf Framezahl, Reihenfolge, Zeitstempel und jedes RGB-/Alphapixel geprüft. Maximaler PTS-Rundungsfehler: 0,334 ms durch Matroskas Millisekunden-Zeitbasis.

MP4, GIF und PNG bleiben verfügbar; kein neuer Codec-Download nötig. Der Encoder neben der Trainer-EXE ist unverändert und weiterhin anhand seiner SHA-256-Identität geprüft. Verlustfreie Ausgabe bewahrt gerenderte Pixel, nicht die ursprünglichen Spieltexturen oder nicht aufgezeichneten Ton.

## Input-HUD

- Kompakte Tastatur in WASD-Anordnung mit Shift, Ctrl, R und Space; vollständige/individuelle Tastatur weiterhin verfügbar.
- Größere, klarere Tastenflächen. Gedrückt wird sofort angezeigt; nur tatsächliches Loslassen erzeugt 160 ms Nachleuchten. **Subtle animations** schaltet diese Effekte ab, ohne Eingangszustände zu verzögern.
- Mausgröße und sichtbare Maustasten bleiben einstellbar. Die Geräteansichten stehen zentriert zueinander; die Maushöhe passt zur Anzahl sichtbarer Extratasten.
- **Player activity** vereint aktuelle akzeptierte Spielzustände und Zustandswechsel in einer Karte. **Recent changes** schaltet die kurze Meldung darunter ab. Aktive Zustände und neu gewechselte Zustände werden getrennt hervorgehoben. Zeit, Game Frame oder Physics Tick sind auswählbar; mehrere gleichzeitige Änderungen überfüllen die Karte nicht.
- Panel, Tastenoutline, Schatten, Ruhefüllung sowie Panel-/Inhaltsopazität bleiben anpassbar. Farben/Schrift folgen dem bestehenden Theme. In Clips besitzt jede Ebene außerdem eigene Opazität, Größe und Position.
- Fehlende Daten werden weiterhin als unbekannt gekennzeichnet. Tastendrücke sind beobachtete Eingaben; die Activity-Karte zeigt akzeptierte Spielzustände.

## Update / Kompatibilität

**ItStealsTrainer.exe und ItStealsHelper.dll gemeinsam ersetzen.** PracticeSuiteEncoder.exe bleibt neben der Trainer-EXE. Lizenz und vollständige zugehörige Encoderquelle liegen unter **Encoder**.

Hauptprotokoll **21**, Frame-Kanal **2** und Replay-/Input-Aufzeichnungsformate bleiben unverändert. Einstellungen **18** lesen ältere Konfigurationen; die alten beiden Activity-Schalter werden sinnvoll zusammengeführt. Clip-Projekte **2** lesen Format 1 und migrieren dessen Activity-Position. Unbekannte zukünftige Dateien bleiben geschützt. Profile übernehmen Ausgabe-/Overlayeinstellungen und erhalten die Kamera des aktuellen Projekts.

Run-Aufnahmen und ursprüngliche Validierung werden nicht umgeschrieben. Das frühere Wand-Fading und zukünftige Grafik-/Reportdienst-Arbeiten bleiben unverändert. Kein Windows-/Unity-Livetest auf diesem Host. Tatsächlich ausgeführte Prüfungen und Grenzen: **PRUEFUNG.md**.
