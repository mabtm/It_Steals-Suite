> Historischer Stand 7.3.4: Dieser Mesh-/Readback-Ansatz wurde nach fehlgeschlagenem Ingame-Test auf Nutzerwunsch in 7.3.5 zurückgenommen. Die damaligen Host-Prüfungen belegen keine funktionierende Ingame-Darstellung.

# Änderungen 7.3.4 / mab

- Die 7.3.3-Fehlklassifizierung ist gegen die vorige Version reproduziert: Eine horizontale Bodenfläche auf dem Walls-Layer mit gemeinsamem Wall-Material wurde als Wand behandelt. Große Wandrenderer und gemeinsame Materialabschnitte wurden damit nicht zuverlässig unterschieden.
- Wand-Fade prüft jetzt sichtbare Mesh-Dreiecke je Renderer und Materialslot. Physik-Collider-Bounds, Parent-Tags und Materialnamen verknüpfen keine unzugehörigen Flächen mehr. Ein geometrischer Treffer im tatsächlichen Wandabschnitt ist nötig; Durchgänge bleiben frei.
- Statische Batches: nur eigene Submesh-Bereiche und die Renderer-Matrix werden ausgewertet. CPU-unlesbare Meshes werden beim Vorbereiten der pausierten Szene einmalig über Vertex-/Index-GPU-Puffer erfasst. Geteilte Meshes erhalten nur einen Snapshot.
- Gemeinsame Boden-/Wand-Materialabschnitte erhalten eigene, vorab erstellte Replay-Geometrie mit getrennten Wand-/Boden-/Deckenabschnitten. Originalmesh, Batchdaten, Materialien und Weltcollider bleiben unangetastet bzw. werden über die vorhandene Restore-Scope zurückgesetzt.
- Prop-/Clock-/Writing-/PhysicsProp-/SmallProp-Layer sowie Prop-/PhysicsProp-Tags schützen Einrichtung einschließlich untergeordneter Renderer. Wand-Fade und Hide ceiling bleiben getrennte Optionen.
- Sichtprüfung über vorab aufgebauten räumlichen Baum, maximal 20 Hz; keine Szenensuche, Mesh-Erstellung, Dateizugriffe oder GPU-Rücklesung im Frame-Pfad. Begrenzte Mesh-/Index-/Geometriebudgets. Nicht lesbare/ungültige Geometrie wird nicht geraten und bleibt deckend; Diagnose und Log nennen fehlende Daten.
- 7.3.4 / mab, Hauptprotokoll 19 und Dateiformate unverändert. Annotierte 7.3.3-Pakete bleiben importierbar. DLL/EXE gemeinsam ersetzen.
- 11 aktuelle Prüfgruppen, darunter produktive Geometrie-/Visual-/Renderer-Adapter, tatsächliche API-Builds, GUI-, Archiv-/Fuzz-/Sanitizerprüfungen. Kein laufendes Windows-/Unity-/D3D-Spiel auf diesem Host; GPU-Treiber, tatsächliche Pixel und Ingame-FPS sind nicht live geprüft.

Boden-/Deckenstücke aus gemischten Renderern werden nach den vier bestehenden Farbvarianten gebündelt: höchstens acht gemeinsame horizontale Zeichenabschnitte pro Szene. Wandabschnitte behalten unabhängige Fade-Steuerung. Bestätigte Props werden ohne unnötigen Mesh-Readback direkt geschützt.
