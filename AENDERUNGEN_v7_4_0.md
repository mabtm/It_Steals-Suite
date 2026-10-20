# PracticeSuite 7.4.0 / mab

## HUD-Werkstatt

Gemeinsamer Editor für Compact-, Speedrun- und Input-HUD, echte bestehende HUD-Zeichenpfade, Dragging, Eckskalierung, Rand-/Mittelpunktfang, Alt-Übersteuerung, 32 Rückgängig-Schritte und heller/dunkler Vorschauhintergrund. Alle Änderungen liegen bis Apply layout in einem Entwurf. Cancel/Escape verwirft Position, Anzeigeoptionen und Farben zusammen. Der verpflichtende Speedrun-Status bleibt erhalten. Beschriftungen stehen im Inspector über den Reglern; die Werkzeugleiste bricht bei wenig Platz um.

## Eingaben und kurze Zustandsmeldungen

Neue formatierte Gerätebeobachtungen zusätzlich zu Kamera, Physik und bisherigen WASD-/Shift-/R-Ereignissen. Logische Tastatur/Maus, XInput-Tasten/Trigger/Sticks und generische Unity-Joysticktasten; Spielachsen bleiben Original-Rückgabewerte. Die Tastaturbeschriftung ist US-orientiert. Nicht konfigurierte Analogachsen anderer Controller werden nicht behauptet.

Live-HUD ist optional, passiv und frei positionierbar. Es zeigt Eingaben und akzeptierte Spielzustände getrennt. Kurze Meldungen für Sprint, Crouch, Radar, Licht, Flash und Uhr tragen wahlweise Sekunden, Frame oder Tick und verschwinden nach 2,5 Sekunden. Im 3D-Replay verwendet es historische Daten und vermeidet die Steuerleiste. Unverbundene XInput-Geräte und Joysticknamen werden nur einmal pro Sekunde neu geprüft. Ohne HUD und ohne Aufnahme werden keine Geräte abgefragt. Beobachtungsfehler gelangen nicht als Ausnahme in den Spiel-Updatepfad.

## Clips und GIFs

Separate Clip-Grenzen, bestehende benannte Marker oder A–B-Bereich; MP4/H.264 oder GIF; 720p/1080p/1440p; Geschwindigkeit, Bildrate und Overlays auswählbar. 2D und 3D verwenden denselben Vorschau-/Ausgabepfad. Die GIF-Vorschau zeigt die tatsächliche reduzierte Palette, MP4 das Quellbild vor verlustbehafteter Kodierung. Vorschau kann laufen, pausieren, scrubben und in 100 % betrachtet werden. Kleine Exportfenster stapeln Vorschau und Einstellungen; Export/Abbruch/Schließen liegen außerhalb der Einstellungsliste.

3D-Aufnahmen werden ausschließlich aus dem passenden geladenen Replay gelesen, nicht vom Desktop oder einem Live-Run. Kamera, Zieltextur, Sichtrechteck, Zeit, Optionen, Wand-Fades und Diagnosezähler werden nach jedem Bild wiederhergestellt. Der Fade-Zustand des Exportbilds hängt nicht von der Kodiergeschwindigkeit ab. Ausgabe ohne Ton; Szenen-/Animationsgrenzen des Replayers gelten weiter.

Kodierung im Hintergrund, höchstens zwei wartende Bilder, feste Quellzeit-/Bildnummern ohne Echtzeit-Dropping, Größen-/Zeit-/Framegrenzen und Prüfung der vollständigen Bildanzahl. Ausgabe erst nach erfolgreicher Finalisierung umbenennen. Abbruch und Fehler erhalten bereits vorhandene Dateien. MP4 benötigt Windows Media Foundation/H.264, GIF keinen externen Encoder. Während der Werkstätten sind globale Szenen-/Speedrun-/Replay-Hotkeys blockiert.

## Kompatibilität

Hauptprotokoll 20; Telemetrie 16952 Byte; Commands 192 Byte. DLL und EXE zusammen ersetzen. Map-/Run-Payload 12 und Kartenkanal 9 bleiben erhalten. Eingabeleser akzeptieren 1/2/3, Annotationen 1/2, Settings 16 liest frühere Einstellungen. Replay-Pakete aus 7.2.0 bis 7.3.5 bleiben unterstützt. Originaldateien und ursprüngliche Validierung werden beim Clip-Export nicht verändert. Der Reportdienst bleibt ausdrücklich offen.

Prüfnachweise und echte Laufzeitgrenzen stehen in PRUEFUNG.md. Kein Windows-/Unity-Livetest auf diesem Host.
