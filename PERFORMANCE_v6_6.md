# Performance 6.6 gegenüber 6.5

12. Oktober 2026. Gleiche deterministische Eingaben, 600 synthetische Layouts pro Fall, ein Worker, tatsächliche Seed-Suchpipeline einschließlich NavMesh-Distanzvorbereitung und Top-10-Pfaden. GCC 13, -O3. Drei Wiederholungen; Wall-Time-Median. Die Verteilung zieht zufällige geordnete Kristallmengen aus 100 Testpunkten, abwechselnd zwei Spawns. Es sind keine echten It-Steals-Maps und kein ausgeführter Unity-RNG.

| Kristalle / Verfahren | 6.5 Median | 6.6 Median | Faktor | Solver-Aufrufe durch Untergrenze erspart |
|---|---:|---:|---:|---:|
| 14 / exakt | 1.417940 s | 0.140621 s | 10.08× | 519/600 (86.5 %) |
| 14 / Heuristik | 0.017611 s | 0.011052 s | 1.59× | 517/600 (86.2 %) |
| 22 / Heuristik | 0.053037 s | 0.016671 s | 3.18× | 503/600 (83.8 %) |
| 40 / Heuristik | 0.288565 s | 0.134032 s | 2.15× | 367/600 (61.2 %) |

Alle zehn Ergebnis-Indizes und ausgegebenen Routenlängen stimmen zwischen 6.5 und 6.6 überein. Zusätzlich prüfen Regressionen die Rangliste gegen eine unbeschnittene Auswertung in beiden Genauigkeiten und mit 1/mehreren Workern. Das neue Verfahren ändert die Modell-Distanzen oder Solver nicht: MST der Kristalle plus minimaler Spawn-Anschluss ist eine Untergrenze der offenen Tour. Nur wenn sie bereits sicher über Platz zehn liegt, wird die aufwendige Berechnung übersprungen. Gleichstände bleiben erhalten. Für Longest route ist diese Beschneidung deaktiviert.

**Messunsicherheit:** Besonders der alte exakte 14-Kristall-Fall schwankt auf dem geteilten Host stark (0,874100 / 2,266450 / 1,417940 Sekunden; neu 0,140621 / 0,128908 / 0,143807). Der Median-Faktor ist deshalb keine portable Erwartung. Die Heuristik-Fälle sind sehr kurz und ebenfalls vom Scheduling beeinflusst. Belastbarer als ein allgemeines Speedup-Versprechen sind die unveränderte Rangliste und die konkrete Zahl eingesparter Solver-Aufrufe.

Ein gezielt günstiger Anfangsfall (die ersten zwanzig Layouts konzentrieren Kristalle näher am Spawn) sparte bei 14 Kristallen 580 von 600 Bewertungen und blieb ebenfalls Top-10-identisch. Solche günstigen Beispiele sind nicht die Basis der Tabelle. Auf Maps mit fast identischen Touren kann die Untergrenze wenig ausschließen und zusätzlichen Aufwand verursachen; es gibt keinen garantierten Faktor.

## Was sich tatsächlich beschleunigt

- Nicht konkurrenzfähige Shortest-Route-Layouts benötigen keinen exakten/heuristischen Solver mehr. Native Matrix-/DP-Puffer und Paardistanzen bleiben wiederverwendet und geteilt.
- Begrenzte vollständige Layout-Caches speichern sowohl berechnete Touren als auch verworfene Untergrenzen. Gleichstände/Hash-Kollisionen werden nicht versehentlich verloren.
- Kleine Touren und reine Layout-Scans veröffentlichen pro begrenztem Paket; aufwendige exakte Touren häufiger. Das hält Fortschritt/ETA erreichbar, ohne für jede Mini-Tour Locks zu erzeugen.
- Die ursprüngliche paketweise, stockgleiche Unity-Auswahl bleibt erhalten. Ein Fenwick-Rank/Select-Versuch war auf dieser Mono-Version langsamer: etwa 28.043 statt zuvor rund 49.912 Layouts/s bei 256 Modellpunkten und etwa 12.761 statt 26.250 bei 512. Das waren nicht gleichzeitig ausgeführte System.Random-/Mono-Paketmodellmessungen; sie reichen nicht für eine garantierte Rate, zeigten aber keinen Grund zum Übernehmen. Der Versuch wurde vollständig entfernt.

## Automatische Last und reale Suche

Die neue Regelung maximiert nicht blind die Arbeit im Spielthread. Sie startet vorsichtig, beobachtet tatsächliche unscaled Framezeiten und ACK-/Worker-Rückstau und passt die kooperative Grenze schrittweise an. Auf langsamen Frames/Stalls wird sie reduziert; stabile Frames erlauben eine Erhöhung. Die Regelung ist in synthetischen stabilen/langsamen/Stall-Fällen getestet, nicht live im Spiel profiliert. Einzelne Layouts können die Grenze überschreiten. Wer bewusst höher belasten möchte, kann die verständlich benannte manuelle Obergrenze verwenden.

Die reale Gesamtgeschwindigkeit wird durch den langsameren Pipeline-Teil begrenzt: Unity-InitState/Range und Spiel-FPS oder native Routenbewertung. Ist die Generierung der Engpass, führt ein schnellerer Solver nicht automatisch zu entsprechend mehr Seeds/s. Keine Unity-Aufrufe wurden in Hintergrundthreads verschoben, kein unbestätigter RNG-Nachbau eingeführt und kein riesiger besuchter Seed-Speicher angelegt.

Die Restzeit benutzt den jüngsten Durchsatz geprüfter Seeds einschließlich Filter-/Untergrenzen-Verwerfungen; damit bildet sie beide Pipeline-Stufen ab. Anlauf/Stalls ergeben Calculating… statt einer falschen festen Zahl.

## Reproduzieren

Siehe tests/README.md und tests/pruning_benchmark.cpp. Derselbe Test kann gegen die 6.5- und 6.6-Trainer-Header gebaut werden. Für eine echte Spielmessung identische Map, Seed-Anzahl, Genauigkeit, Workerzahl und manuelle Last verwenden; mehrmals wiederholen und Spiel-Frametime/FPS zusätzlich erfassen. Den Auto-Modus separat auf Stabilität und Rückstau prüfen. Zahlen für den reinen Solver nicht als In-Game-Rate ausgeben.
