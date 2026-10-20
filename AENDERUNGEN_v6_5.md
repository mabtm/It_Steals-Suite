# Änderungen 6.5.0

- Seed-Pakete fassen bis zu 2.048 Layouts statt 256. Das einstellbare Zeitbudget, unveränderte Unity-RNG-Reihenfolge, finally-Restore und ACK-Backpressure bleiben erhalten. Kein Hintergrundthread greift auf Unity zu.
- Generierung schreibt mit wiederverwendeten Auswahl- und Byte-Puffern direkt in das Paket. Der Kartenkanal kopiert ausschließlich dessen belegte Bytes synchron, bevor der Puffer erneut benutzt wird. Keine neue Layout-/Array-/Stream-Allokation im Generierungsloop.
- Native Routenthreads verwenden eine flache wiederverwendete Matrix und DP-Scratch. Bitmengen vermeiden Scans über nicht beteiligte Kristalle. Heuristik, Tie-Breaking und exakte Methode sind gegen den vorherigen Solver geprüft.
- Begrenzter Cache mit vollständigem Layout-Vergleich, keine Allokation für Suchschlüssel und Validierungs-Besuchslisten je Seed. Nur mögliche Ranglistentreffer werden kopiert; Speicher wächst nicht mit der gewählten Seed-Anzahl.
- Suche nach kürzester Gesamtroute, nächstem Kristall zum Spawn oder schwierigen langen Routen. Neue Spawn-, erforderliche/ausgeschlossene Kristall- und Distanzfilter; keine neue Spawn-Statistik oder Heatmap.
- Filter direkt per Rechtsklick auf der Karte; gut sichtbare grüne/rote Marker. Regeln pro Map persistent, an tatsächliche Katalog-Koordinaten und Reihenfolge gebunden. Ungültige oder unmögliche Regeln werden sichtbar abgefangen; laufende Aufträge behalten ihre Kriterien.
- Aktuelle/durchschnittliche Raten, passende/verworfene Seeds, Generierungskosten und Cache-Treffer. Beste Ergebnisse als TSV in die Zwischenablage kopierbar, einschließlich Interpretation und Modellgrenzen.
- Config 10 migriert alte Werte mit Defaults. Hauptprotokoll 8 und Run-Datenformat 6 unverändert; optionaler Kartenkanal 5 / Suchpaket 3. Alte Karten-Dateiformate 1/2 bleiben lesbar.
- Normal/Practice, Clean Speedrun, passive Eingaben, zwei HUDs, Minimap-Achsen, Escape-Abbruch und Verifikation erhalten. Ältere Releases bleiben unverändert.

DLL und EXE gemeinsam ersetzen. Messwerte und Grenzen stehen in PERFORMANCE_v6_5.md und PRUEFUNG.md. Die tatsächliche In-Game-Suchrate hängt weiterhin von Unity, Map, Frame-Budget, Genauigkeit und Rechner ab.
