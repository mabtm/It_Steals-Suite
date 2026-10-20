# Technische Finish-Ansicht 7.0.3 / mab

Die Bilder stammen aus der tatsächlichen ImGui-Zeichenausgabe einschließlich Font-Atlas. Synthetische Run-Daten, 1920×1080 und 100 % DPI; kompakte Detailansicht bei 480×360 / 150 %. Host-Schrift DejaVu Sans. Keine Windows-/Unity-Liveaufnahme; Windows lädt die gewählte verfügbare Systemschrift.

- scanning.png: vierte aktive Prüfung, Detailspalte und Evidence stream.
- result.png: vollständige Prüfungen und vollständiger Datei-Hash in zwei Hälften.
- invalid.png: ForbiddenCommand-Warnbit; automatische Auswahl der betroffenen Prüfung.
- technical-small-150.png: begrenzte Detailansicht mit erreichbarem Copy report, scrollbareren Werten und Close details.
- finish-scan.gif: 21 native Zeichenausgaben der Prüfsequenz, auf das Ergebnisfenster zugeschnitten; längeres Schlussbild. Identische Bilder dürfen im GIF zusammengefasst werden.

Die Tests aktivieren Prüfzeilen per tatsächlicher Maus sowie Follow scan, Copy report und den kompakten Detailknopf per ImGui-Tastatur. Alle 42 Felder sowie vollständiger Hash und exakte Run-ID erreichen die Clipboard-Schnittstelle; Run-Modus und Archivdaten bleiben unverändert. Code: ui_tests.cpp / FinishVerificationCases und milestone_tests.cpp.
