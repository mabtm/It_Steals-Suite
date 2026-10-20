# PracticeSuite 7.0.1 / mab

## Intro

Der bisherige Startzustand konnte die einmalige Anzeige bereits bei einem aktiven Run als verbraucht markieren. Außerdem war die kurze Intro-Laufzeit an den ersten Zeichenversuch statt an die sichtbare Ausgabe gebunden, und das Intro forderte bei ausgeschalteten HUDs keine eigene Overlay-Fläche an.

Die Startgrafik wartet jetzt auf eine frische Verbindung und eine erfolgreiche Bildausgabe. Fehlgeschlagene/verdeckte Präsentationen verbrauchen keine Zeit. Aktive Runs verschieben die Anzeige; Ausblenden pausiert sie. Das Intro ist unabhängig von optionalen HUDs sichtbar. Interface animations aus bedeutet ein ruhiges Intro-Standbild; Start / exit graphic aus bleibt eine vollständige Abschaltung. Exit und die vorhandene einmalige Blur-Aufnahme bleiben erhalten.

## Ergebnisfenster

Der automatische 11-Sekunden-Ausblender und das Verwerfen beim Scene-/Epoch-Wechsel sind entfernt. Das eingefrorene Ergebnis bleibt bis X erhalten, einschließlich Zeit, Seed, PB-Differenz und lokaler Prüfungen. Auch eine vorübergehend fehlende Verbindung oder geänderte Helper-Sitzung löschen es nicht. Ein neuer Abschluss ersetzt es; während eines aktiven Runs werden Anzeige und Eingabeübernahme ausgesetzt.

Das Ergebnis ist jetzt ein interaktives Fenster mit sichtbarem X und freigegebenem Cursor. X kehrt zur vorherigen Ansicht zurück und gibt die Eingabe entsprechend zurück. Kleine Viewports behalten X und Kopfzeile, während Details scrollbar sind. Das gewöhnliche Hauptmenü liegt während der Ergebnisanzeige nicht darüber. Die getrennte F8-Analyse-Abfrage läuft weiterhin 15 Sekunden und endet beim Szenenwechsel.

Version 7.0.1 / mab; Protokoll 16 und Settings 15 bleiben gleich. DLL und EXE gemeinsam ersetzen. Routing, Seed-Suche, Replay-Aufzeichnung, Flashes und Movement-Lernen wurden nicht geändert. Hide & Seek / Outside bleiben WIP.
