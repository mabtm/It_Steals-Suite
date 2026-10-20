# PracticeSuite 6.9.3

Fortführung von 6.9/6.9.1/6.9.2. Kein Code des verworfenen Navigationsrasters aus 6.10.

- Hide & Seek: Das Screenshot-Problem waren ausschließlich Ersatz-Luftlinien. Der eigene native Neubau entfällt. Die Navigation verwendet die erfassten Bodendreiecke wie Classic und verbindet nur wandfreie Abschnitte innerhalb dieser Dreiecke. Portalöffnungen und Wandkonturen berücksichtigen den Spielerradius. Flächen auf Wandoberseiten, unzulässige Steigungen und Verbindungen durch Wände werden ausgeschlossen. Die Berechnung läuft im Trainer-Worker.
- Seed-Karte, Review und Minimap zeigen in Hide & Seek dieselben physischen Hindernisse wie die Routing-Prüfung, einschließlich statischer Hindernisse ohne Wall-Tag. Sammelbare Point-Objekte und ihre Kind-Collider gelten dabei als Ziele, nicht als permanente Wände.
- Der Geometriecache berücksichtigt jetzt statische Hindernisse, Konturen, Spielerradius, Höhe, Bodenhöhe und Bewegungsprofil. Änderungen verwenden keine alten Wege mehr.
- Fehlende Navigationsdaten liefern weiterhin Distanzschätzungen für das Ranking und behalten alle Seeds in der Bilanz. Ungeprüfte Segmente werden in Karten/Minimap ausgeblendet; eine unvollständige vorgeschlagene Route wird im Review ausdrücklich als nicht verfügbar angezeigt.
- Freecam erzeugt ausschließlich eine neue Camera und deren Hitbox-Renderer. Kein Spielerkörper, Collider oder Spielscript wird geklont oder mitbewegt. Der originale Spieler und seine echten Collider bleiben zur Untersuchung erhalten. Ein Harmony-Guard unterdrückt seine Trigger-Effekte einschließlich Kristallaufnahme; beim Beenden werden Kamera und Scriptzustände wiederhergestellt.
- Run Review: große Karte mit seitlicher Wiedergabesteuerung bei breiten Fenstern, unabhängige Routenschalter, kurze animierte Spuren und Positionsmarker, Pause/Play, Restart, Zeitleiste, Geschwindigkeitsoptionen. Automatisch große Fensteransicht mit Wiederherstellung beim Tab-Wechsel. Für fehlgeschlagene Runs zunächst Vergleich derselben eingesammelten Kristalle; vollständiger Vergleich in Options.
- Laufzeitschätzung: 8,1–10 u/s; reine Bewegung `Distanz / Geschwindigkeit`. Vorschlag in der Animation standardmäßig 9 u/s, verstellbar. Spielerroute mit echten Aufnahmezeiten. Keine behauptete Zeitgarantie für Gegner, Pickup-Radien, Abbremsen oder Bewegungstricks.
- Übersicht: lange Erklärungen zu Freecam, Seed-Typen, Verifikation, Worker-Leistung und HUD-Konfiguration in Hover-Tooltips. Review-Details, Minimap- und Overlay-Konfiguration werden bei Bedarf aufgeklappt. Sichtbare Fehlermeldungen und der obligatorische VALID/UNVALID-Status bleiben erhalten.

DLL und EXE gemeinsam austauschen. Hauptprotokoll bleibt 14; Map-Format bleibt 7. Hide & Seek neu laden, um Boden, Wände und Spielerprofil frisch zu erfassen. Vorhandene Einstellungen, Seed-Pipeline, RNG-Verifikation und Run-Journale bleiben kompatibel.

Builds und reproduzierbare Tests: PRUEFUNG.md. Das tatsächliche Windows-/Unity-Spiel konnte in dieser Umgebung nicht live ausgeführt werden.
