# Nächster Ausbau / 7.3.5 / mab

Der unabhängige Replay-Prüfer ist vom Nutzer als sinnvoller nächster Ausbau benannt. Er ist noch nicht implementiert. Die zwölf ergänzenden Ideen sind ausschließlich Vorschläge und keine zugesagten oder enthaltenen Funktionen. Nicht gewünschte Vorschläge aus der vorherigen Fassung werden nicht weiterverfolgt.

## Gewünschter unabhängiger Replay-Prüfer

Ein separates, rein lesendes Werkzeug auf dem Rechner des Moderators soll Originaldateien selbst prüfen und dem gespeicherten VALID-Feld nicht blind vertrauen. Es soll bestehende strikte Decoder wiederverwenden und zusätzliche Konsistenzregeln nachvollziehbar ausführen: Zeitfolgen, Aufnahmeabdeckung, Start/Ende, Pickups, Zustandswechsel sowie Bewegung passend zum bestätigten Spielcode. Jede Auffälligkeit braucht Regelcode, Zeitintervall, konkrete Messwerte, Toleranz und die betroffene Datenquelle.

Ergebnisse unterscheiden bestanden, regelwidrig und nicht ausreichend prüfbar. Fehlende Daten oder ein Stotterer allein ergeben kein Cheater-Urteil. Ein unabhängiger Prüfer erhöht die Nachvollziehbarkeit, beweist aber keine saubere Ausführung eines lokal veränderbaren Clients. Originaldateien werden beim Prüfen nicht umgeschrieben; Moderatorentscheidungen werden separat gespeichert.

## Zwölf weitere Ideen

1. **Video und Replay synchronisieren.** Eine lokale Run-Aufnahme mit der Replay-Zeit verbinden; gemeinsames Spulen und Einzelbildvergleich. Das macht Unterschiede zwischen sichtbarem Spiel und gespeicherten Daten leichter untersuchbar. Video bleibt lokal und ist optional.
2. **2D-Karte neben der 3D-Ansicht.** Beide Ansichten verwenden dieselbe Abspielposition; Wegverlauf und konkrete Raumsituation sind gleichzeitig sichtbar. Eine bestehende 3D-Kamera plus die leichte 2D-Karte genügt.
3. **Dynamische Türen und bewegte Weltobjekte aufzeichnen.** Tatsächliche Zustandswechsel bzw. begrenzte Positionsänderungen ergänzen die momentan nicht vollständig gespeicherte Weltgeschichte. Alte Dateien zeigen ausdrücklich fehlende Weltgeschichte statt erfundener Zustände.
4. **Monster als Beobachtungsziel.** Ein bestimmtes Monster auswählen, seine aufgezeichnete Bewegung verfolgen und Zustände/Reaktionen an der gemeinsamen Timeline untersuchen. Nicht aufgezeichnete Blickrichtung oder Animation wird nicht als Original ausgegeben.
5. **Tatsächlichen Lichtkegel in 3D anzeigen.** Vorhandene Lampenposition, Rotation, Reichweite und Winkel als dezente Kegelkontur anzeigen; bestätigte Flash-Ereignisse und Monsterreaktionen darüberlegen. So wird die räumliche Situation verständlicher, ohne aufwendige Echtzeitbeleuchtung.
6. **Eingabe und Spielaktion gegenüberstellen.** Nicht nur R/Shift-Taste und bisherigen akzeptierten Radar-/Sprintzustand anzeigen, sondern bei Bedarf auch die tatsächlichen spielinternen Freigaben, Cooldowns oder Ressourcen ergänzen. Das erklärt, warum eine Eingabe erfolgreich war oder keine Aktion ausgelöst hat; zusätzliche Zustände erfordern neue Aufnahmen.
7. **Aufnahmequalität direkt auf der Timeline.** Kamera-, Physik-, Monster- und Lichtdaten erhalten schmale Abdeckungsbänder. Lücken und veraltete Beobachtungen sind sichtbar, bevor jemand eine Interpolation für einen echten Spielzustand hält.
8. **Datenlupe für einzelne Physikschritte.** Für einen ausgewählten Moment die benachbarten Original-Samples mit Zeitabstand, Position, Rotation, Eingaben und Zuständen kompakt vergleichen. Interpolierte Bilder und tatsächlich aufgezeichnete Samples werden klar unterschieden.
9. **Separate Moderatorenebene.** Befunde mit Zeitraum, betroffenen Objekten, Kommentar und Prüfstatus sammeln. Sie ergänzen vorhandene Nutzernotizen und Marker, verändern aber nicht die Originalaufnahme und deren ursprüngliche Bewertung.
10. **Objekt- und Ereignissuche.** Nach Kristall-ID, Monster oder Ereignistyp filtern, zur betreffenden Aufnahmezeit springen und das Objekt in 2D/3D hervorheben. Besonders bei großen Outside-Karten erspart dies langes manuelles Suchen.
11. **Sparsame Ereignistonspur.** Tatsächliche Spielereignisse wie Radar-, Flash- oder Warnsound-Aufrufe mit Zeitstempel erfassen und im Replay gezielt wiedergeben. Keine dauernde Mikrofon-/Systemtonaufnahme; ältere Dateien bleiben ohne nachträglich erfundene Töne.
12. **Verlustfreie Replay-Komprimierung.** Große Archive ohne Datenverlust platzsparend ablegen. Vorhandene Originaldateien bleiben erhalten; eine spätere Archivfunktion muss beim Entpacken exakt dieselben Originalbytes zurückliefern und Größenlimits weiterhin erzwingen.

Als Ergänzung zum Prüfer sind Video-Synchronisierung, sichtbare Datenabdeckung und dynamische Weltzustände besonders wertvoll: Sie liefern zusätzliche überprüfbare Zusammenhänge statt nur mehr Prüf-Labels.

## Grenzen der bisherigen Validierung

Die vorhandene Suite prüft beobachtete Start-/Seedbedingungen, Trainer-Hilfen, verbotene Befehle, ausgewählte Zustände, Hooks, Zeitmaßstab, HUD und bekannte externe Prozessnamen/Debugger. Der Game-Assembly-Hash betrifft die Datei auf Datenträger. Decoder und Pakete prüfen Datenformat, Zeitfolgen, Identität, CRC/SHA-256. Das ist keine unabhängige Bescheinigung eines unveränderten Prozessspeichers. Ein erkanntes Programm beweist nicht, dass es diesen Run verändert hat; nicht erkannte Programme beweisen keine saubere Umgebung.
