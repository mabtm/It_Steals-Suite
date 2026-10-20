# Änderungen 7.3.1 / mab

- Verfolgerkamera frei um den Spieler drehbar: rechte Maustaste bei Spiel-Fokus; Backspace richtet einmal an der Aufzeichnung aus. Kein Wand-Kollisionstest. Winkel bleibt über Pause, Seek und Loops erhalten; Polbegrenzung verhindert Umkippen.
- Breitere Geschwindigkeitsspur als echtes Band mit Kontur, gemessene 0–10 u/s weich von Dunkelrot über Rot/Amber zu Türkis. Höhere Geschwindigkeiten behalten Türkis. Stärke skaliert mit der Ansicht; maximal 360 Abschnitte, keine Verbindung über Aufnahme-Lücken/Teleports.
- Collider: unterstützte Shader-Fallbacks und korrekte Materialfarben, unabhängige Gruppen. Durch-Wand-Ansicht zunächst aktiv, damit Replay-Marker kleine Formen nicht verdecken. Spieler-Unterbaum und separater Monster-Agentenkörper werden dedupliziert aufgezeichnet. UI zeigt je Gruppe vorhandene Spuren; fehlende Daten bleiben ausdrücklich fehlend.
- Alte 7.3.0-Pakete einschließlich Marker/Loop-Bereichen bleiben importierbar. Protokoll 18, Map-Payload 12, Eingabeformat 2 und Einstellungen 15 bleiben unverändert; alte Eingabeformat-1-Aufnahmen bleiben abspielbar.
- Zusätzliche produktive Host-Adaptertests für Quellen, Orbit, Verlauf und GL-Ausgabe. Aktuelle Prüflogs: tests/results-7.3.1. Echte Unity-/D3D-Rasterisierung und Windows-Eingabe sind auf diesem Host nicht ausführbar.

Installation: Spiel und Trainer schließen, DLL in Mods und Trainer-EXE gemeinsam ersetzen. Nur eine Helper-Version in Mods behalten. Einstellungen und Originalaufnahmen bleiben erhalten. Fehlende historische Collider können nur durch einen neu aufgenommenen Run ergänzt werden.
