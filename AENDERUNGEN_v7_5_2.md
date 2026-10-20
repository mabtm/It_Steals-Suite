# PracticeSuite 7.5.2 / mab

## Kamera-Keypoints

Jeder Punkt speichert jetzt **Recorded**, **Follow** oder **Free**, seine Linse, den Übergang und optional eigene Szenen-Layer. Vorschau und Export verwenden dieselbe zeitbasierte Auswertung.

- **K / Add keypoint** übernimmt die Kamera zum genauen Zeitpunkt und wählt den neuen Punkt zur Bearbeitung aus. Liste, Timeline-Diamant und Path overview springen exakt zu diesem Punkt. Ein Fehler beim Ermitteln des Einfügeindex nach einer Vergrößerung der Punktliste ist behoben.
- **Use keypoints** spielt die Spur. Beim Bearbeiten eines Punktes bleibt diese Option aktiv: Ein Wechsel zu Free verändert den ausgewählten Punkt und schaltet nicht versehentlich den ganzen Export auf eine statische Kamera um.
- **Recorded** verwendet die tatsächlich aufgezeichnete Kamera am jeweiligen Zeitpunkt. **Follow** speichert seinen eigenen Orbit und folgt der aufgezeichneten Position. **Free** speichert eine frei platzierte Kamera ohne festen Drehpunkt.
- **Free:** rechte Maustaste halten zum Umschauen; dabei WASD bewegen, Q/E senken/heben, Shift schneller fliegen. Mausrad bewegt entlang der Blickrichtung, mittlere Maustaste verschiebt die Kamera. Nur Umschauen verändert die Position nicht. Die Fluggeschwindigkeit ist einstellbar; Position und Winkel lassen sich zusätzlich numerisch bearbeiten.
- **Follow:** rechte Maustaste dreht unabhängig vom Spieler, Mausrad zoomt, mittlere Maustaste verschiebt den Bildausschnitt. Ein Wechsel von Free zu Follow erhält den aktuellen Bildausschnitt.
- **Exact time in recording:** sechs Nachkommastellen und Einzelschritte entsprechend Ausgabetempo/FPS. Punkte müssen mindestens 0,001 Sekunden auseinanderliegen. Eine nahe neue Position verschiebt keinen bestehenden Punkt unbemerkt.
- **Transition to next point:** Cut, Linear, Smooth, Ease in, Ease out oder Ease in/out. Cut hält den aktuellen Modus bis zum nächsten Punkt; Recorded und Follow bleiben währenddessen dynamisch. Die übrigen Kurven blenden Position, Orientierung und Linse auch zwischen unterschiedlichen Kameramodi. Vor dem ersten Punkt läuft Recorded; nach dem letzten bleibt dessen Modus aktiv.

## Layer an Punkten

**Scene layers** befindet sich direkt unter der Kameraauswahl. Beim Bearbeiten eines Punktes ändern **Actual colliders**, **Hide ceiling**, **Route trail**, **Monster** und der vorhandene Fade-Schalter ausschließlich die Replay-Darstellung ab diesem Zeitpunkt. Jeder geänderte Schalter wird bis zur nächsten expliziten Änderung weitergeführt; andere Punkte setzen ihn nicht automatisch zurück. **Inherit previous layers** entfernt die lokalen Überschreibungen.

Beispiel: An Punkt D Colliders einschalten, an Punkt G ausschalten. D–F und die dazwischenliegenden Zeiten zeigen Collider; ab G sind sie aus. Ohne bearbeiteten Punkt werden die Standard-Layer geändert. Spiel-Cheats, Originalaufnahmen und Run-Validierung werden dadurch nicht verändert.

## Kamerapfad und Vorschau

- **Show camera path:** farbiger Pfad und Blickrichtungen in der 3D-Vorschau. **Path overview:** zusätzliche anklickbare Draufsicht mit Punktbuchstaben, Richtungs-Pfeilen und Zeit-/Höhen-/Neigungs-Tooltip. Recorded ist türkis, Follow amber, Free violett; Sprünge bei Schnitten sind gestrichelt. Die Pfadzeichnung ist begrenzt, zwischengespeichert und ausschließlich eine Editorhilfe. Der fertige Clip enthält sie nicht.
- Ausstehende 3D-Bilder behalten ihre eigenen Zeitstempel und Kompositionseinstellungen. Spätere Änderungen am Editor werden nicht mit einem älteren Szenenbild vermischt. Aufnahmen, Bildgröße und Kamerawechsel werden bei Antworten weiterhin geprüft. Neue Ansichten überschreiben keine bearbeitete Kamerapose.
- Eine Anzeige nennt den Zeitpunkt des tatsächlich dargestellten Vorschaubildes. Beim Aktualisieren bleibt er erkennbar, bis die neue Szene bestätigt ist. Fit verwendet eine kleinere Renderauflösung; **100% preview** und der Export verwenden die eingestellte Ausgabeauflösung. MP4 zeigt das Bild vor H.264-Kompression, GIF die Ausgabepalette.
- Der Export berechnet Kamera und Layer erneut für jeden exakten Quellzeitpunkt. Das Bildtiming bleibt `Start + Frameindex × Tempo / FPS`; es werden keine Frames zur Anpassung an Echtzeit ausgelassen. Ein Punkt zwischen zwei Ausgabeframes wird im ersten Frame ab seinem Zeitpunkt sichtbar.
- **Input-HUD:** L und R sind anhand der tatsächlichen Schriftbreite in der jeweiligen Maustaste zentriert.

## Projekte und Installation

Neue Clip-Projekte verwenden **ISCLIP 3**. Formate 1/2 bleiben lesbar; alte statische Keypoints werden als Free-Punkte übernommen und die vorhandene Spur aktiviert. Identität, Dauer, Spielhash, Grenzen, Prüfsumme und Schutz unbekannter zukünftiger Dateien bleiben erhalten. Run-/Replay-Dateien und die Live-HUD-Konfiguration werden nicht umgeschrieben.

Trainer-EXE und Helper-DLL gemeinsam ersetzen; **PracticeSuiteEncoder.exe** neben der Trainer-EXE und den mitgelieferten Encoder-Ordner behalten. Protokoll 21, Frame-Kanal 2 und Einstellungen 18 bleiben unverändert. Der Encoder selbst ist unverändert.

Ausgeführte Prüfungen und Grenzen: **PRUEFUNG.md**, Logs unter **tests/results-7.5.2**. Keine laufende Windows-/Unity-Spielinstanz auf dem Testhost. Die Vorschau-PNG stammt aus der echten Oberfläche mit ausdrücklich gekennzeichneten synthetischen Replay-Daten und einer 2D-Ladeansicht. Wand-Fading und die Grafikrekonstruktion bleiben auf dem bisherigen Stand.
