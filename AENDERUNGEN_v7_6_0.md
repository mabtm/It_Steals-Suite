# PracticeSuite 7.6.0 / mab

## Grafik

- Originalszene als Standard: echte Materialien, Texturen, Modelle, Lightmaps, Lichter und Schatten. Ursprüngliche statische Batch-Bereiche bleiben erhalten; keine GPU-Mesh-Rücklesung oder Nachbildung der Wände.
- Tatsächliche Spielkamera mit ihren bestehenden Bildeffekten statt einer bloßen Kamera-Kopie. Originale volumetrische Lichter behalten ihre registrierten Render-Callbacks. Renderpuffer werden vor jedem Durchlauf geleert, auch bei mehreren Offscreen-Frames im selben Spiel-Frame.
- Neue passive Grafikaufzeichnung: Modell-/Knochenposen, Spritewechsel, Renderer-Sichtbarkeit, erste Materialfarbe/-textur sowie Lichtpose, Farbe, Intensität, Reichweite und Winkel. Quellen werden beim Run-/Actor-Start gebunden; keine Szenensuche oder Materialinstanzierung in der Sample-Schleife. Bis 30 Hz, unveränderte Werte entfallen; statische Mesh-Zustände bis 10 Hz. Begrenzte Daten- und Speichergrößen.
- Reine Grafik-Proxys für Spieler, Monster und Kristalle: keine kopierten KI-Skripte, Collider, Physikkörper oder Audioquellen. Animator-Controller bleiben aus; beobachtete Posen werden direkt gesetzt. Skins nutzen Originalmeshes/-materialien und eigene Knochen. Sprite-Billboards folgen der tatsächlichen Replay-Kamera.
- Wiederherstellung von Kamerapose/-einstellungen, Materialien/Property-Blocks, Spritezuständen, Lichtern, Animator-Aktivierung und Weltposen beim Beenden oder Fehler. Simplified graphics bleibt als leichter Analyse-Stil verfügbar.

## Tempo pro Keypoint

- Unabhängige Tempo-Überschreibung von 0,1x bis 8x; Direktwahl 0,1/0,5/1/2x.
- Cut oder fünf weiche Übergangskurven bis zum nächsten aktivierten Tempo-Punkt. Punkte ohne Override ändern die Spur nicht. Vor dem ersten Override gilt das Standardtempo.
- Dieselbe absolute Zeitabbildung für Vorschau, Seek, Einzelbild, Testclip und Export. Feste Ausgabe-FPS; Slowmotion verändert die Clipdauer. Keine Zeitakkumulation über gerundete Frames.
- Clip-Projekte ISCLIP 4 speichern Tempo, Übergang und Grafikprofil; Formate 1/2/3 bleiben lesbar. PNG-Sequenzen speichern die Quellzeit jedes Frames.

## Installation und Grenzen

DLL und EXE zusammen ersetzen: Protokoll 22. Das kleine Update enthält nur diese beiden Binärdateien sowie diese Hinweise. Der vorhandene Encoder bleibt unverändert. Vollpaket mit Quellcode, Encoder/Lizenz und Prüfergebnissen ebenfalls verfügbar.

Eingabeaufnahmen verwenden neu Format 4; bestehende 1/2/3 werden gelesen und unverändert archiviert. Alte Dateien besitzen keine historischen Modellanimationen. Neue Grafikdaten sind lokale Beobachtungen, kein manipulationssicherer Moderationsnachweis; Run-Prüfung und Originaldateien werden durch Clip-Edits nicht verändert.

Die Grafik ist nicht als pixelidentische Reproduktion sämtlicher Spielzustände zu verstehen: Audio, Partikel, Shaderzeit, Radar-Renderziele und nicht aufgezeichnete Materialparameter fehlen weiterhin. Grafikpuffer maximal 24 MiB / 393.216 Samples; bei Reduktion hält der Player den letzten beobachteten Zustand und zeigt einen Hinweis. Wand-Fading bleibt beim bisherigen Verfahren und seinen bekannten Grenzen.

Gebaut und auf dem Host geprüft, **noch kein Windows-/Unity-Livetest** der Originalszene. Insbesondere alle Spielmodi, Schatten/Lichtkegel und Exportauflösungen im Spiel prüfen. Die beigefügte UI-Vorschau zeigt die echte Oberfläche mit synthetischen Daten und einer klar bezeichneten 2D-Ladeansicht. Sie ist kein 3D-Spiel-Screenshot. Tatsächliche Prüfungen: PRUEFUNG.md.
