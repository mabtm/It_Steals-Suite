# Änderungen 6.4.0

- Seed-Suche wählt zufällige Seeds aus dem gesamten gültigen 32-Bit-Seedraum. Die fortlaufende Suche, Start-/Endbereich und Range-Anzeige sind entfernt.
- Jede Suche startet mit unabhängigem OS-Zufall eine neue Reihenfolge. Die Auswahlpermutation ist bijektiv: innerhalb eines Auftrags keine wiederholten Seeds, kein Speicherbedarf für eine Millionenliste besuchter Seeds.
- Normale Seed-Eingabe und Random-Seed-Vorschau bleiben für einzelne Karten verfügbar; sie beeinflussen die Suchauswahl nicht.
- Mod und Trainer verwenden dieselbe Auswahlreihenfolge. Jedes erzeugte Layout überträgt außerdem den tatsächlich verwendeten Seed. Schlüssel, Paket-Index, Seed und Kontext werden geprüft; falsche Zuordnungen stoppen die Arbeit.
- Originaler Unity-RNG und finally-Restore, frei wählbare Seed-Anzahl, Frame-Budget, parallele Routenworker, begrenzte Pakete/Caches und Stop-Verhalten erhalten.
- Hauptprotokoll 8 mit 100-Byte-Commands, Kartenkanal 4 und Seed-Paketformat 2. Alte Karten-Dateiformate bleiben lesbar; Config bleibt Version 9.
- Bestehende HUDs, Minimap, Eingaben, Escape-Abbruch, Run-Daten und Verifikation erhalten. Spawn-Statistiken bleiben entfernt.

DLL und EXE gemeinsam ersetzen. Genauigkeit bleibt ausdrücklich auf die modellierte NavMesh-Weglänge bezogen. Die Zufallsauswahl ersetzt den Game-RNG nicht.
