# Version 5.0.1 – Maus und getrennte HUD-Modi

Beide Dateien aus `bin` gemeinsam ersetzen. Vorher alten Trainer und Spiel schließen; nur eine Helper-DLL laden. Protokoll v4, vorhandene Anzeigeprofile und Run-Daten bleiben kompatibel.

- Kein deaktiviertes, bildschirmgroßes HUD-Fenster mehr. Passive Layered-Fenster geben Mausereignisse vollständig weiter und nehmen keinen Fokus.
- Interaktives Menü behält den sichtbaren Cursor und die DirectComposition-Alpha-Darstellung. Beim Schließen werden Capture/Fokus zurückgegeben, die alte Oberfläche verborgen und die passive Oberfläche zwischen Frames neu erstellt.
- Nach dem Neuaufbau eines geöffneten Menüs wird der Fokus erneut übernommen, sofern das Spiel im Vordergrund ist. Alt+Tab zu anderen Anwendungen nimmt keinen Fokus.
- Reiner Speedrun: ausschließlich Speedrun-Overlay; F4 und Compact-Schalter gesperrt.
- Normal/Practice: Compact HUD erlaubt, Speedrun-Overlay verborgen. Alle Run-/Seed-Werte sind weiterhin im Compact-Profil auswählbar.
- Ein gemeinsamer Modusentscheid verhindert gleichzeitige HUDs, auch bei noch nicht aktualisierter Telemetrie.
- 128 zusätzliche Tests der tatsächlichen UI-Render-Einstiegspunkte. Bestehende UI-, Protokoll-, Geometrie-, Seed- und Run-Tests bleiben erhalten.

Passive HUDs verwenden zugunsten der dokumentierten Klick-Durchgabe den Color-Key-Pfad. Die Panel-Opazität verändert dort die Hintergrundintensität; echte Alpha-Transparenz bleibt im geöffneten Menü verfügbar. Diagnostics zeigt Renderer und Eingaberolle.

Direkte Windows-/Spieltests sind hier weiterhin nicht ausgeführt; Hinweise in `PRUEFUNG.md`.
