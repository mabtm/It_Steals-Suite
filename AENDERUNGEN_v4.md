# Architekturprüfung und Änderungen in Version 4

Ausgangspunkt war das unveränderte, gespeicherte Version-3-Paket. Native Oberfläche, Shared-Memory-Verbindung, sämtliche Mod-Komponenten, Datenpersistenz, Layoutgenerator und Build-Dateien wurden durchgesehen. Bestehende Trainingsfunktionen sind weiterhin enthalten.

| Befund | Umsetzung | Praktische Wirkung |
|---|---|---|
| `HWND_TOPMOST` und `SWP_SHOWWINDOW` wurden in jedem Frame gesetzt | Zuordnung als Owned Popup; Sichtbarkeit anhand Spiel-/Menü-Fokus, Sichtbarkeit, Minimierung und DWM-Cloaking | Externes Overlay verschwindet beim Wechsel zu anderen Anwendungen |
| Fensterbewegung und Rendering liefen unabhängig vom Fokus | WinEvent-Benachrichtigungen plus 100-ms-Fallback; Geometrie nur bei Änderung anwenden | Weniger redundante Fensteroperationen; Client-Bereich und DPI werden berücksichtigt |
| Permanente Maus-Capture, Cursor-Zähler-Schleifen und Cursor-Clip | Nur ausdrückliche Menüaktionen wechseln Fokus; ImGui übernimmt Capture bei tatsächlichen Klicks | Keine dauerhafte Mausbindung beim Alt+Tab |
| F-Tasten wurden auch außerhalb des Spiels ausgewertet | Registrierung nur im Spiel-/Menü-Fokus; Zustands-Fallback verbraucht außerhalb gedrückte Tasten | Andere Anwendungen lösen keine Trainer-Befehle aus |
| Eigenes Spielfenster und Grafikgerät wurden nur einmal gebunden | Erneute Window-Suche bei fehlendem Handle, Neuaufbau der Oberfläche bei anderem Owner oder Grafikfehler | Fenster-/Moduswechsel beenden den Trainer nicht automatisch |
| Swapchain-Resize erfolgte direkt im Window-Callback | Größe vormerken, zwischen Frames resizen, bei Fehler Oberfläche neu aufbauen | Keine Ressourcenänderung im reentranten Window-Callback |
| Background-Frames wurden weiter gezeichnet | Rendering nur bei sichtbarem Overlay, auswählbares FPS-Limit; Heartbeat bleibt unabhängig | Hintergrundarbeit sinkt ohne Unterbrechung der Mod-Verbindung |
| Befehle wurden unverändert pro Frame geschrieben | Nur geänderte Records oder fällige Heartbeats senden; ausstehende Requests bis Ack erhalten | Weniger Mutex-Zugriffe und Memory Writes |
| Mod-Polling und vollständige Snapshots liefen bei jedem Spiel-Frame | Befehle höchstens 60 Hz, Snapshots höchstens 30 Hz bzw. 2 Hz ohne Details/Hitboxen | Bei hohen Spiel-FPS sinken Kopier- und Serialisierungskosten |
| Wall-Scans umfassten auch Ressourcen/Prefabs und liefen ohne sichtbare Karte | Aktive Szene-Collider; bedarfsabhängige Wand-/OOB-Erfassung und Wall-Zusammenführung | Keine allgemeinen Asset-Collider-Scans; unnötige Kartenerfassung entfällt |
| Pfade wurden ohne sichtbare Pfadanzeige abgefragt | Optionales Anforderungsfeld in bisher reservierten Befehlsbytes | NavMesh-Pfadkopien nur bei Bedarf |
| Minimap-Polygone, Wall-Vektoren und Kreis-Normalen erzeugten wiederholte Arbeit | Wiederverwendete Vektoren; Box-Vergleich ohne neue Kopie, Normalen einmal berechnen | Weniger Allokationen und Trigonometrie im Renderpfad |
| Eingaben und Statistiken konnten unlesbar bzw. ungültig sein | Begrenzte Konfigurationsgröße, validierte Werte, zusätzliche Telemetrie-Sanitierung | Beschädigte Configs und Zahlen werden abgefangen |
| Start-/Shutdown-Teilfehler konnten Ressourcen übrig lassen | Geordneter, gegen Einzelfehler abgesicherter Abbau; Besitzer-PID in beiden Kommunikationsrichtungen | Fehler einer Ressource verhindern nicht den restlichen Cleanup |
| Fullbright-Scans konnten nach Fehlern sofort erneut laufen | Scan-Intervall vor Ausführung setzen; Fehlerausgabe begrenzen; Teilallokationen besitzen | Keine schnelle Fehler-/Allokationsschleife |
| Run-/Death-Zähler wurden bei jedem Ereignis direkt gespeichert | Änderungen bis 500 ms sammeln; PB/Finish sofort, Shutdown abschließend speichern | Weniger Dateizugriffe bei schnellen Neustarts |

Die Befehlsverarbeitung, Timer-Uhr und Todes-/Finish-Erkennung bleiben auf dem Unity-Hauptthread. Die WinEvent-Callbacks laufen auf dem Fenster-Thread und markieren lediglich Änderungsbedarf. Sie lesen keine Spielobjekte und nehmen keine Kommunikationssperre. Es wurden keine neuen Hintergrund-Threads eingeführt.

`TrainerUI.h`, `TrainerPreferences.h`, `TrainerSettings.h`, `GameWindowBinding.h` und `OverlayPolicy.h` trennen Anzeige, Einstellungen und Fensterlebenszyklus vom bestehenden Render-/Kommunikationscode. Die reine Sichtbarkeitspolitik und Konfigurationsserialisierung sind ohne Windows prüfbar.

Die neue UI ordnet Features in fünf Tabs. Es gibt einheitliche Abstände, skalierte Schrift, Tooltips, sichtbare Einschränkungen für saubere Speedruns, tatsächlich gemeldete Hilfen-Zustände, eine Diagnosesammlung, ein speicherbares FPS-Limit und Layout-Reset. Die Minimap folgt der unteren linken Ecke und bleibt innerhalb des Client-Bereichs; Menü und HUD berücksichtigen eine veränderte Spielgröße.

F6 ist eine gezielte Borderless-Anforderung über `UnityEngine.Screen.fullScreenMode`. Exklusives Vollbild wird nicht als sicher mit externen Overlays kompatibel behauptet. Ownership, WinEvent, Größenanpassung und die Modusumschaltung sind umgesetzt und kompiliert; ihr tatsächliches Windows-Verhalten ist noch im Spiel zu prüfen. Dafür enthält `PRUEFUNG.md` eine kurze Checkliste.

Es wurden keine konkreten CPU-/RAM-Einsparungen gemessen. Die Verbesserungen lassen sich derzeit aus reduzierten Update-Frequenzen, ausgelassenem Rendering/Scans und vermiedenen Allokationen begründen. Geometrische Näherungen, feste Exportgrenzen und die Grenzen eines festen Seeds sind weiterhin in der README beschrieben.

Verwendete Win32-Dokumentation: [SetWinEventHook](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwineventhook), [Window Features / Owned Windows](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features), [SetWindowLongPtrW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowlongptrw) und [DXGI Best Practices](https://learn.microsoft.com/en-us/windows/win32/direct3darticles/dxgi-best-practices).
