# PracticeSuite 7.11.2 / mab

- Fehler beim Öffnen von Details behoben: versetzt gelieferte Object-/Transform-Felder werden einmal zusammengeführt; eindeutige Gruppen-/Tabellen-IDs, Einstieg am Anfang. ImGui-Konfliktprüfung bleibt aktiv.
- Wand, Graffiti und weitere Objekte auf demselben Mausstrahl getrennt auswählen. Mausrad wechselt das Highlight; Tab öffnet die eingefrorene Trefferliste. Enter, Doppelklick oder Select highlighted bestätigt.
- Standard Visuals + colliders inklusive Trigger; Visuals only / Colliders only verfügbar. Colliderfreie Meshes und einfache Sprites sind direkt auswählbar. Die letzte Trefferliste bleibt auch auf der Objektkarte und im Inspector verfügbar.
- Lesbare Meshdreiecke, zugehörige statische Submeshes und enge einfache Sprite-Dreiecke; Flip und Spritewechsel berücksichtigt. Sonstige Geometrie ausdrücklich Bounds estimate. Keine Textur-Alpha-Prüfung.
- Begrenzter visueller Index, gecachte Geometrie, eine neue Geometriekopie pro Frame, feste Kandidaten-/Dreiecksbudgets. Scan und Teilmengen bleiben erkennbar. Keine Collider/Material-/Spielobjektänderung und keine GPU-Auslese.
- ID der gewählten Ebene gilt für Hover und frischen Commit; verschwundene Ziele wechseln nicht stillschweigend zum Nachbarn. Vorhandene Szenen-, Session-, Pointer-, Clean- und Replay-Sperren bleiben bestehen.
- Main29 / Inspector3. Helper und Trainer zusammen ersetzen und Spiel neu starten. Encoder und Karten-/Replay-/Clipformate unverändert, Import 7.11.1 ergänzt. Hide and Seek / Outside bleiben WIP.

Host-Builds, echte ImGui-Bedienung, Wire/Fuzz, Engine-Vertragsdoubles und Regressionen bestanden. Kein Live-Spielnachweis auf diesem Host; Scope/Budgets in LIVE_INSPECTOR.md und PRUEFUNG.md.
