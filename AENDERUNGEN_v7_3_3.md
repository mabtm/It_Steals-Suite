# Änderungen 7.3.3 / mab

- Fade blocking walls: getrennte Visual-/Collider-Zweige werden für bestätigte Hauptwände unterstützt. Bisher verlangte die Zuordnung denselben Hierarchiezweig; das konnte Hauptwände ausschließen, während Objekte mit eigenem Collider funktionierten.
- Wandklassifizierung verwendet echte Walls-Layer, Wall-Tags am Objekt/Eltern und die ursprünglichen Wandmaterialien. Sehr breite bestätigte Wände werden nicht mehr allein aufgrund ihrer Bounds als horizontale Fläche klassifiziert. Identität bleibt beim Wechsel auf generische Replay-Materialien erhalten.
- Bounds-Überschneidung allein genügt weiterhin nicht: tatsächliche Collider-Sichtstrahlen, Treffer im Renderer-Bereich und Wandzugehörigkeit sind nötig. Unzugehörige Objekte, explizite Böden/Decken und Materialslots bleiben geschützt.
- Keine neue Szenensuche im laufenden Replay. Zuordnung einmal beim Laden, maximal zwei Sichtstrahlen bei 20 Hz, gecachte Materialvarianten und vorhandene vollständige Rücksetzung. Hide ceiling und die übrige Kamerabedienung bleiben erhalten.
- Hauptprotokoll 19 und Dateiformate unverändert. Beide DLL/EXE zeigen 7.3.3 / mab. Annotierte 7.3.2-Pakete bleiben neben älteren Paketen importierbar.
- 11 gezielte aktuelle Prüfgruppen; neue Hauptwand-Regressionen verwenden produktiven C#-Code mit Host-Doubles. Kein Windows-/Unity-/D3D-Livetest auf diesem Host; die tatsächliche Alpha-Darstellung muss im Spiel beurteilt werden.
- Der gewünschte unabhängige Replay-Prüfer ist noch nicht implementiert. MODERATION_PLAN.md enthält seinen Umfang sowie zwölf weitere Vorschläge. Zuvor nicht gewünschte Erweiterungen sind aus dem aktuellen Plan entfernt; die vorherige Fassung bleibt als historische Datei erhalten.
