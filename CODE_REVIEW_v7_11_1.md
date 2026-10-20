# Codeprüfung – PracticeSuite 7.11.1 / mab

## Auswahl und Daten

Der frühere native Picker nutzte die aktuelle ImGui-Mausposition beim Verarbeiten des Klicks. Der neue Windows-Pfad hält die erste noch nicht verbrauchte WM_LBUTTONDOWN-Position fest und rechnet vom Overlayclient über ScreenToClient zum tatsächlichen Spielclient um. Die GUI-Testfallback-Abfrage verwendet MouseClickedPos. Cursorzustand, Klickereignis und bereits bestätigte Auswahl sind getrennt. Beim Warten auf den zugehörigen Hover bleibt der geklickte Pointer eingefroren. Klicks auf das eigene Hinweisfeld werden nicht in die Welt übernommen.

InspectorPicking.Viewport bildet den normierten Spielclient über Camera.rect auf Camera.ViewportPointToRay ab. Screen.width/height werden nicht als Fenstergröße verwendet. Hover und Bestätigung nutzen dieselbe Query; der Helper prüft frische Pointer-ID, erwartete Objekt-ID und die tatsächliche aktuelle Oberfläche erneut. Ein neuer Nachbartreffer übernimmt das alte Ziel nicht stillschweigend. RaycastNonAlloc hat eine feste 512er-Grenze; vollständiger Puffer wird abgewiesen, da Unity in diesem Fall keine vollständige nächste-Treffer-Menge verspricht. Reichweite bleibt 10000 u. Spielerkörper wird übersprungen; Trigger sind Standard. Renderer-Bounds sind explizit approximativ und ein Katalog-Snapshot.

Das sind nachgewiesene Codepfad-Korrekturen. Ohne konkreten Live-Windows-/Unity-Abgleich ist nicht bewiesen, dass damit jede gemeldete Szenen-/Materialbesonderheit behoben ist. Kamera-ID, Viewport und Clientmaße stehen im Hover-Vertrag; Fehlmeldungen bleiben sichtbar.

## Last und Bedienung

F7 ist neu registriert und im Fallback gleich zugeordnet; F8/Shift+T und bestehende F2–F6 behalten ihre Belegung. Abbruch stellt vorherige Menü-/Compact-/F2-hidden-Einstellung wieder her. Kleinfenster, passive Pin-Karte, Rückgabe der Eingabe und ein eigener Inspector-Navigationseintrag ersetzen den bildschirmfüllenden Sonderarbeitsbereich. Erfolgreicher Fokus-ACK ist weiterhin Voraussetzung für automatische Rückgabe der Steuerung. Ausgeblendete/gesperrte/disconnected Inspector-Sitzungen erzwingen keine Menü-Eingabe.

Grundauswahl liest feste kleine Engine-Attribute, keine Materialien oder Spielskriptfelder. Katalog nur bei Bedarf, bisheriger auf Frames verteilter Refresh. Detailflag16 begrenzt zusätzliche Definitionen/Lesearbeit auf explizite Anforderung. Ausdrücklich gesetzte Watches bleiben unabhängig aktiv. Kein zusätzlicher vollständiger Geometrieplaner für den reinen Picker; Geometrie wird nur für sichtbare Probe/Konturen angefordert. Hover30Hz, Details5Hz plus Befehlsbestätigung; getrennte bounded banks, Mutex0 und ein bestehender Latest-only-Parser-/Probe-Worker. Keine Threads für Unity-Abfragen.

## Kompatibilität und Prüfung

Main28 / Inspector2, Commands192 / Payload16952. Map10, MapRun13, Input6, Frame4, Audio1, ClipProject7 und Encoder unverändert. Alte 7.11.0-Pakete explizit weiter importierbar. Keine Routen-/Kontaktoptimierung geändert, H&S/Outside weiterhin WIP. Helper- und native Clean-/Replay-Sperren bleiben unabhängig; LiveInspector-Aid512 bleibt sticky.

Prüfungen: tatsächlicher SDK-Helper und Windows-x64-Trainer mit Warnungen als Fehler, alle bisherigen nativen/managed Kernregressionen, vollständiges produktives Headless-ImGui, C#→C++ Hover-/Detail-/Katalog-Verträge, jede Kürzung, Fuzzing, Worker, Pointerfreeze, unabhängiges JSON und ASan/UBSan. Es gibt keinen Live-Spiel-, echten GPU-FPS- oder Windows-Inputnachweis auf diesem Host. Renderte GUI-Bilder verwenden klar bezeichnete synthetische Auswahl, keine Spielszene. Aktuelle Ergebnisse siehe PRUEFUNG.md.
