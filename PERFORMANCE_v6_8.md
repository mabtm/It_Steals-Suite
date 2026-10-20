# Performance 6.8 – funktionsfähige Seed-Suche

## Befund aus dem Screenshot

6.7 zeigt 3.083.450 erzeugte und geprüfte Seeds in 62,7 s (49.187/s), sieben Worker, alle neun Spawn- und 34 Kristall-Anker erkannt. Trotzdem gibt es null bewertete Treffer und 3.083.450 als „Unreachable“ ausgeschlossene Layouts. Diese Rate ist keine Leistungsmessung erfolgreicher Routenbewertung: der zusätzliche Komponentenfilter ließ den Solver gar nicht laufen.

Ein fehlender KI-NavMesh-Pfad beweist nicht, dass ein Seed im Spiel unspielbar ist. Der Filter wurde entfernt. Vollständig abgedeckte frühere Dreieckkarten bleiben maßgeblich. Fehlende Paarwege werden einmalig geschätzt, klar markiert und in derselben festen Tabelle wiederverwendet. Kein Filter wird automatisch hinzugeschaltet.

## Aktueller gepaarter Host-Vergleich

GCC -O3, gleiche synthetische Geometrie, gleiche tatsächliche Streaming-/Workerpipeline, gleiche Compilerflags. Reihenfolge zwischen 6.7 und 6.8 wechselt. Kein laufendes Unity-Spiel; Raten sind keine Zusage für die reale Seed-Generierung.

| Arbeit | 6.7 Median | 6.8 Median | Prüfung |
|---|---:|---:|---|
| 1 Million wiederholte 4-Kristall-Layouts, vier Worker, sieben Läufe | 0.143794 s | 0.146441 s | Cache/Queue-Pfad, alle Seeds geprüft |
| 600 unterschiedliche 8-Kristall-Layouts, ein Worker, Fast, gleichmäßige Verteilung, fünf Läufe | 0.00451112 s | 0.00445131 s | Identische Top-10-Indizes und Längen in jedem Lauf |
| 600 unterschiedliche 18-Kristall-Layouts, ein Worker, Fast, gleichmäßige Verteilung, fünf Läufe | 0.00987471 s | 0.00786963 s | Identische Top-10-Indizes und Längen in jedem Lauf |

Die Durchläufe sind kurz und unterliegen Scheduling-Schwankungen. Der stark gecachte Millionen-Test liegt etwa 1,8 % hinter 6.7; die einzelnen Zeiten streuen wesentlich stärker. Der 18-Kristall-Test ist im Median etwa 20 % kürzer. Daraus lässt sich kein belastbarer allgemeiner Live-Speedup ableiten. Der Vergleich zeigt vor allem, dass die wieder funktionsfähige Logik bestehende vollständige Karten nicht durch ein teures neues Verfahren ersetzt. Der gecachte Test hatte etwa 6,3–6,8 MiB Peak-RAM mit 6.7 und 7,0–7,3 MiB mit 6.8 auf diesem Host.

## Erhaltene Performance-Maßnahmen

- Automatische Lastregelung aus 6.7 unverändert; normale ACK-Wartezeit senkt die Last nicht künstlich auf 0,25 ms.
- Tatsächlicher Unity-RNG bleibt im Spielthread; native Worker teilen unveränderliche vorbereitete Distanzen.
- Keine erneute NavMesh-Abfrage oder Dateischreiboperation pro Seed.
- Wiederverwendete Solver-/Auswahlpuffer, bis zu acht Queue-Pakete, 2.048 Layouts pro Paket, 4.096 Cache-Einträge je Worker, zehn Ranglisteneinträge.
- Keine Komponentenprüfung pro Seed. Modelllücken werden beim Vorbereiten der Matrix behandelt.
- Exakte MST-Untergrenze für die vorbereiteten symmetrischen Distanzen; Kandidaten, deren beste mögliche Länge Platz zehn nicht erreichen kann, benötigen keinen vollständigen Solver. Sie werden trotzdem als geprüfte Seeds gezählt. Identische Ranglisten gegen unbeschnittene Referenzen getestet.
- Nur Top-10-Routen bekommen Pfadlinien. Schätzungsmarker sind begrenzt; lange gestrichelte Linien haben höchstens 256 Striche pro Segment.

## Funktionelle Regressionen

Isolierte und fehlende Anker, partielle Graphen, fehlende Geometrie, vollständige frühere Karten und Screenshot-großer Katalog (neun Spawns / 34 Punkte / 18 Kristalle) geprüft. Shortest/Longest, Exact/Fast, ein/mehrere Worker und tatsächlich aktive Spawn-Filter gegen unabhängige Bewertungen geprüft. Ohne Filter gilt scored + pruned = evaluated; mit Filter gilt filtered + scored + pruned = evaluated. Alle korrekt gelieferten Seeds bleiben berücksichtigt.

Geschätzte Distanzen sind 3D-Luftlinien; sie können echte Laufstrecken unterschätzen. Estimated-Kennzeichnung, amber gestrichelte Pfadstücke und TSV-Distanzmodell machen dies sichtbar. Die Suche behauptet damit keine bestätigte Navigation oder optimalen tatsächlichen Speedrun. Die realen 49.187/s aus dem defekten Skip-Pfad sind kein fairer Vergleichsmaßstab für funktionierende Routenbewertung.
