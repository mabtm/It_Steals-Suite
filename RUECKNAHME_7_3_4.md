# Rücknahme 7.3.4 / neuer Versuch 7.3.5 / mab

Auf ausdrücklichen Nutzerwunsch wurde der fehlgeschlagene 7.3.4-Darstellungsansatz verworfen. Grundlage ist wieder der 7.3.3-Originalrenderer-Pfad. Entfernt sind ReplaySurfaceGeometry und ReplayHorizontalBatch, Vertex-/Index-GPU-Readback, eigener Dreiecks-/BVH-Katalog, Mesh-Aufteilung, zusätzliche Levelrenderer und horizontale Ersatz-Mesh-Batches.

Der neue Versuch beschränkt sich auf Kameramaske/Fokus, Flächenschutz und Alpha-Materialien. Ursprüngliche Renderer, Materialslotzahl, statische Batch-Bereiche und die bestehende ReplayRendererScope bleiben erhalten. Shader- und Kameraprobleme sind anhand von Code/Asset-Metadaten nachgewiesen; ihre Beteiligung am tatsächlichen Bildfehler ist ohne Ingame-Messung nicht abschließend bewiesen.

Die 7.3.4-Archive und damaligen Prüflogs bleiben als Historie erhalten. Der damalige Host-Test war kein Ingame-Nachweis. Die aktuellen Ergebnisdateien stehen unter tests/results-7.3.5. Replays und Nutzerdaten werden nicht gelöscht oder umgeschrieben.
