# PracticeSuite 6.9.9 / mab

## HUDs und Minimap

- Beide HUDs übernehmen die Gestaltung der Suite: ruhige Karten, feine Konturen und kurze Akzentlinie, klare Überschriften und getrennte Textstufen.
- Größere, standardmäßig gleichbreite Timer-Ziffern. Eine abschaltbare schmale Leiste zeigt den tatsächlichen Run-Fortschritt.
- Kompakter Hinweis „AIDS OFF / LOCAL CHECK“. Warnungen, Seed-Typ und lokale Gültigkeit bleiben erhalten. Der Pflichtstatus hat auch bei stark verkleinertem Layout eine Mindestschriftgröße und eigenen Kontrast.
- Minimap mit feinem Kompassring, klareren Konturen, Kristallen als Diamanten und abgestimmten Farben für Spieler, Trail und optimale Route. Projektion, Clipping, Geometrie und Routenberechnung bleiben unverändert. Hide & Seek / Outside bleiben WIP.

## Anpassung

**Settings → Appearance** enthält die gemeinsame Farbpalette: Hintergrund, Text, Überschriften, Highlights, Knöpfe, Hover-/Animationsfarbe und Umrandungen. Änderungen wirken direkt auf die tatsächlich gezeichneten Controls. Farbwähler oder Hex-Code; Rechtsklick setzt eine einzelne Farbe zurück. Bei schwachem Textkontrast gibt es einen freiwilligen Ein-Klick-Vorschlag.

Zehn Systemschriften: Segoe UI, Segoe UI Semibold, Bahnschrift, Arial, Verdana, Tahoma, Trebuchet MS, Calibri, Consolas und Georgia. Menü, HUD und Timer sind getrennt wählbar. Nicht installierte Schriften sind gekennzeichnet und nicht auswählbar; gespeicherte fehlende Schriften fallen auf Segoe UI bzw. die eingebaute Schrift zurück. Es werden nur die drei ausgewählten Schriftgrößen geladen. Der Font-Atlas wird ausschließlich zwischen Frames erneuert; reine Farbänderungen lösen keinen Neuaufbau aus.

**Settings → HUD** zeigt eine Stilvorschau mit ausdrücklich gekennzeichneten Beispieldaten und Schachbrett zum Beurteilen der Deckkraft. Beide HUDs können die GUI-Farben übernehmen oder eine eigene Palette verwenden. Scale, Deckkraft und Position bleiben direkt erreichbar, Detailabstände und die lange Feldliste sind aufklappbar.

**Settings → Minimap** enthält zusätzlich eigene Kartenfarben. **Settings → System** bündelt Audio, Bewegung, Intro und Display-Einstellungen. Alle Werte speichern automatisch. Das neue Einstellungsformat 14 übernimmt eigene Farben, Layouts, Seeds und Optionen aus 6.9.8.

## Intro und Kennzeichnung

Das Intro dauert jetzt 2,6 Sekunden, zeichnet das Routenlogo schrittweise und blendet sanft ein/aus. Dahinter kann ein einmalig aufgenommener, verkleinerter Spielausschnitt weichgezeichnet werden. Nur der Bereich der Intro-Karte wird aufgenommen, im Arbeitsspeicher gehalten und anschließend freigegeben; kein laufender Hintergrundfilter und keine Bilddatei. Bei fehlender oder schwarzer Aufnahme bleibt die normale Kartenfläche. Der Effekt ist separat abschaltbar und wird während aktiver Runs unterdrückt. Die Abschlussgrafik bleibt kurz; sie hält keine Eingabe fest.

Version und Autor **v6.9.9 / mab** stehen in Kopfzeile, Intro und HUDs. Such-Export und Diagnostik verwenden dieselben zentralen Angaben; die Mod-Metadaten nennen ebenfalls mab.

## Prüfung

19 erfolgreiche Prüfgruppen einschließlich echter ImGui-Ausgabe, SDK-/Windows-Build, Einstellungs-Rundlauf und eines zusätzlichen GDI-/D3D-Adapters für Aufnahmefehler, schwarze Bilder, Einmalaufnahme und Ressourcenfreigabe. Einzelheiten und Grenzen in PRUEFUNG.md. Windows-/Unity-Live-Spiel, reale Treiber und Spiel-FPS konnten hier nicht geprüft werden.

Hauptprotokoll 15, Kartenkanal 9 und Payload 11 unverändert. Bestehende Replays bleiben lesbar. Die Spiel-/Routing-/Erfassungslogik aus 6.9.8 bleibt erhalten.
