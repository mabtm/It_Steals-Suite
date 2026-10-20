# Änderungen 7.3.2 / mab

- Verfolgerkamera: rechte Maustaste zum horizontalen/vertikalen Drehen, Mausrad hoch hinein / herunter heraus. Separate Distanz-/Höhenregler entfernt. Lokaler Zoom/Winkel bleiben über Pause, Seek, Loop und GUI-Befehle erhalten. Keine Wandkollision.
- Replay-Sichtoptionen: Decken-Materialabschnitte ausblenden; in der Verfolgeransicht verdeckende Wand-/Tür-Renderer sanft transparent darstellen. Tatsächliche Collider-Hin-/Rückstrahlen, maximal 20 Hz, gecachte Quellen und Materialarrays. Vollständige Rücksetzung bei Verlassen/Fehler. Originalrenderer/Batch-Bereiche und Weltcollider bleiben erhalten.
- Spieler-Hitbox im 3D-Replayer mit leicht breiterem Kern und dunkler Kontur für helle Böden. Echte Form/Dimensionen unverändert; Nahgrenzen-Clipping verhindert übergroße Linien an der Kamera.
- Hauptprotokoll 19 bei unveränderten Structgrößen. Beide DLL/EXE gemeinsam ersetzen. Settings 15, Map-Payload 12, Eingabeformat 2 (Leser 1/2), Marker-/Paketformate erhalten; alte 7.3.0-/7.3.1-Pakete einschließlich Markern importierbar.
- 13 gezielte Prüfgruppen; native Fuzz-/Sanitizer- und produktive GUI-/Host-Adaptertests erweitert. Keine laufende Windows-/Unity-/D3D-Umgebung auf diesem Host; reale Alpha-Darstellung und Eingabefokus müssen im Spiel beurteilt werden.
- Vorhandene Moderations-/Validierungslogik unverändert. MODERATION_PLAN.md hält konkrete geprüfte Ansatzpunkte und Grenzen für den nächsten Ausbau fest; vorgeschlagene Funktionen sind nicht als bereits implementiert auszugeben.
