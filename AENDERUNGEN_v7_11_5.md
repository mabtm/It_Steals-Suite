# PracticeSuite 7.11.5 / mab

## Live Inspector

- Lesbarere schwebende Objektkarte mit eigener Kopfleiste, schlichter Symbolleiste, klar getrennten Informationen und Versions-/Autorangabe.
- Bestehender projizierter Picker-Rahmen mit Eckmarkierungen, kurzem Randimpuls und unterschiedlicher Hervorhebung bei Auswahl und Anheften. Auswahl und Tastatur-/Mauseingaben reagieren sofort.
- Kurze Einblendung der Karte und unterbrechbare Bewegung beim Öffnen/Schließen der technischen Details. Tabs nutzen die bestehenden Suite-Animationen.
- Tatsächlich geänderte Eigenschaftswerte werden für bis zu 650 ms hervorgehoben. Beobachtungen werden ausschließlich aus neuen vorhandenen Detailpaketen gewonnen; begrenzter Cache.
- Eigenes Style-Menü: Auswahl-Effekte, Inspector-Sounds, Lautstärke, Deckkraft und Konturstärke. Einstellungen werden automatisch gespeichert; vorhandene Konfigurationen bleiben lesbar.
- Vier eigene dezente Sound-Cues auf dem bestehenden Audio-Worker. Lautstärke pro Cue, gemeinsame Master-Stummschaltung, Fokus-/Run-Sperren und verfallende Einzelereignisse.
- Escape schließt ein Popup innerhalb der Objektkarte, ohne gleichzeitig die Karte zu verlassen.
- Style-Popup bleibt auch bei 480 × 360 und 150 % DPI innerhalb des sichtbaren Bereichs und ist bei Bedarf scrollbar.

## Installation / Kompatibilität

Bei bestehender 7.11.4 nur **bin/ItStealsTrainer.exe** ersetzen. Der **Helper bleibt 7.11.4**; diese Kombination ist beabsichtigt. Das Vollpaket enthält genau diesen kompatiblen Helper. Keine Änderungen an C#-Spielcode, Routenlogik, Replay-/Clipformaten oder Encoder. Hauptprotokoll 29 / Inspector-Kanal 3 bleiben erhalten.

**F7** startet den Picker. Die neue **Style**-Schaltfläche befindet sich im Inspector-Tab und in der interaktiven Objektkarte.

Prüfungen und Einschränkungen: **PRUEFUNG_v7_11_5.md**. Ein laufender Windows-/Unity-Spieltest wurde auf diesem Host nicht durchgeführt.
