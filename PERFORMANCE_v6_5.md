# Performance-Vergleich 6.4 → 6.5

Stand: 12. Oktober 2026. Alle Werte stammen vom Entwicklungs-Host, nicht aus einem laufenden Windows-/Unity-Spiel. Gemeinsame Host-Auslastung und CPU-Takt schwanken; kleine Unterschiede sind keine belastbare Aussage für jeden Rechner. Die Benchmarks werden sequenziell ausgeführt.

## Einzigartige synthetische Routen

Gleiche Testquelle, g++ -O3, ein Worker, unveränderte vorberechnete NavMesh-Distanzen. Pro Heuristik-Größe 3.000 unterschiedliche Layouts; für exakt 14 Kristalle 48 Layouts. Drei Vergleichsläufe; Tabelle zeigt den Median der jeweiligen Raten. Längen- und Reihenfolge-Prüfsummen sind in jedem Paar exakt gleich.

| Kristalle / Methode | 6.4 Layouts/s | 6.5 Layouts/s | Verhältnis |
|---|---:|---:|---:|
| 14 / heuristic | 31,653 | 50,999 | 1.61× |
| 14 / exact | 80 | 383 | 4.77× |
| 22 / heuristic | 8,706 | 10,029 | 1.15× |
| 40 / heuristic | 1,422 | 1,878 | 1.32× |

Das vergleicht den tatsächlichen Routensolver inklusive Auswahlmatrix. Der Solver nutzt Scratch-Tabellen und Bitmengen; die frühere Tie-Reihenfolge und Methode bleiben erhalten. Dieselben Ergebnisse werden zusätzlich gegen den vorherigen Solver bei 1–64 Kristallen und gegen vollständige Permutationen bei 1–8 Kristallen geprüft. Es ist keine Messung einer vollständigen In-Game-Suche.

## Auswahl und Pakettransport

50.000 Layouts je Kataloggröße, 14 Kristalle, identische System.Random-Draws, Stock-kompatible Auswahl, echter Mono-MemoryMappedFile-/Mutex-Kanal mit sofortigem synthetischem ACK. Zwei Vergleichsläufe; Medianraten. Keine echte Unity-InitState-, Frame-, native Routen- oder getrennte Consumer-Prozessmessung.

| Kandidaten | 6.4 Layouts/s | 6.5 Layouts/s | Verhältnis |
|---|---:|---:|---:|
| 32 | 253,064 | 392,273 | 1.55× |
| 64 | 177,946 | 237,404 | 1.33× |
| 128 | 98,362 | 107,677 | 1.09× |
| 256 | 54,986 | 49,912 | 0.91× |
| 512 | 25,388 | 26,250 | 1.03× |
| 1024 | 9,994 | 11,725 | 1.17× |

Alle Spawn-/Kristall-Prüfsummen stimmen überein. Je Testlauf benötigt 6.4 196 Pakete, 6.5 25 Pakete. Gen0-Collections während der Messung: 6.4 jeweils 2, 6.5 jeweils 0. Der neue Loop benutzt vorhandene Auswahl- und Byte-Puffer; der Kanal kopiert deren belegten Inhalt synchron.

Die Rate steigt nicht in jeder Kataloggröße. Bei 256 Kandidaten war der zweite neue Durchlauf langsamer; die einzelnen neuen Raten lagen zwischen 43.017 und 56.806 Layouts/s. Bei großen Katalogen dominieren die unveränderten echten Zufallsziehungen und die Stock-kompatiblen Löschungen. Hieraus lässt sich kein garantierter In-Game-Zuwachs ableiten.

## Begrenzte native Pipeline

Ein Millionendurchlauf verwendet dieselbe SeedSearch-Pipeline, vier Worker und wiederkehrende synthetische Layouts mit vielen Cache-Treffern. Letzte Messung: 6.4 1,199 s, 6.5 0,202 s. Alle 1.000.000 Inputs werden verarbeitet; Queue höchstens 8, Treffer höchstens 10, keine wachsende Sample-Liste. Der Peak des neuen Testprozesses lag bei etwa 6 MiB. Dieser Sonderfall misst die native Pipeline und Cache-/Validierungskosten, keine Million einzigartiger Spiel-Layouts.

## Auswirkungen im Spiel

Das bisherige Limit von 256 pro Paket wird auf 2.048 erhöht. Die durch Paketgröße allein bedingte theoretische Obergrenze steigt damit achtfach, ohne achtfachen realen Durchsatz zu versprechen. Das 0,25–8-ms-Generierungsbudget bleibt als kooperative Grenze bestehen. Unity-RNG-Aufrufe bleiben auf dem Game-Thread, mit finally-Restore. Höhere Budgets können die Game-Frametime erhöhen.

Neue Filter sparen Routensolver-Arbeit, nachdem die echte Auswahl erzeugt wurde. Sie beschleunigen den Game-RNG selbst nicht. Die UI zeigt durchschnittliche/aktuelle Gesamtraten, generierte Layouts pro aktivem Generation-Frame, Mikrosekunden je Seed und Cache-Treffer. Matches zählen passende Inputs; alle geprüften Inputs einschließlich früher Verwerfungen zählen zur eingestellten Seed-Anzahl.

Für einen fairen Live-Vergleich denselben Level, Suchmodus, Workerzahl, Renderlimit und Generation-Budget verwenden; Game-FPS/Frametime und tatsächliche Suchrate beobachten. Ein echtes Windows-/Unity-Spiel steht auf diesem Host nicht zur Verfügung.
