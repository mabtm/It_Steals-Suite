# Performance 6.9 — ausgeführte Host-Vergleiche

Prüfstand: 13. Oktober 2026. C++20 / GCC 13 bzw. Host-g++, -O3, Linux-Host mit CPU-Quota entsprechend acht logischen CPUs. Kein laufendes Spiel und keine Ziel-GPU.

Die bisherige Erzeugung lief mit Unity-RNG und Seed-Paketen auf dem Spielthread; erst danach konnten die Trainer-Worker Routen bewerten. Ein gemeinsamer Erzeugungs-/Frame-/ACK-Engpass erklärt, warum 1 und 6 Route-Worker ähnliche Gesamtgeschwindigkeit haben können. Das gemeldete Absinken auf etwa 15.000 Seeds/s wurde hier nicht live reproduziert.

Nach erfolgreichem Runtime-Abgleich können die nativen Worker jetzt selbst erzeugen. Die Freigabe prüft Initialisierung, Integer-Range, finalen RNG-Zustand und vollständige Spielauswahl; sie ist keine ungeprüfte Annahme aufgrund eines ähnlichen Zufallsgenerators. Die Originalmethode bleibt als Fallback erhalten.

## Neue vollständige CPU-Pipeline

`tests/cpu_search_benchmark.cpp`: dieselben eine Million zufälligen Seeds je Wiederholung, synthetische 9 Spawns / 34 Kandidaten / 18 Kristalle. Erzeugung, Distanzvorbereitung, MST-Untergrenze, Heuristik, Ranking und Zeichnen der besten Routen enthalten. Drei Wiederholungen, Median:

| Worker | Sekunden | Seeds/s | Faktor zu einem Worker |
|---|---:|---:|---:|
| 1 | 2,241 | 446.199 | 1,00 |
| 2 | 1,132 | 883.032 | 1,98 |
| 6 | 0,400 | 2.503.030 | 5,61 |

Die Top 10 stimmen in allen Wiederholungen überein; bester Seed -15.600.447, modellierte Länge rund 99,0538. Alle eine Million Seeds wurden erzeugt und ausgewertet. Das Layout-only-Modell verarbeitet zusätzlich **zehn Millionen** Seeds vollständig: 1,990 s, etwa 5,03 Millionen/s, sechs Worker. Gemessener Prozess-Speicherhöchststand für die gesamte Benchmarkfolge rund 10 MiB.

**Grenze dieses günstigen Falls:** Die synthetischen Punkte verwenden geradlinige, als geschätzt markierte Distanzen. Rund 99,8 % der Layouts benötigen nach der Untergrenzenprüfung keinen vollständigen Route-Solver. Die Anzahl der Solver-Aufrufe variiert leicht mit Worker-Scheduling; Ergebnisse und Gesamtzählung bleiben gleich. Das ist keine reale Camera-Bright-/Timer-Karte und keine Zusage von Millionen Seeds/s im Spiel.

## Korrektheit und Stabilität

- Neue CPU-Erzeugung gegen dieselben Layouts in der bisherigen Stream-Pipeline: Raw-/Base-Seed, 8/18 Kristalle, 1/3/6 Worker, gleiche Top 10, gleiche Längen, eingezeichnete Route und vollständige Zählung.
- 320 binäre unabhängige C#-/C++-Auswahlvektoren für alle unterstützten Modelle, signed Seed-Grenzen und Scene-/Site-Ableitung.
- Runtime-Verifier gegen unabhängige Test-Referenzen für sämtliche 40 unterstützten Parameterkombinationen; abweichende Referenz muss Fallback liefern.
- Zehn-Millionen-Layoutlauf mit begrenztem Speicher; 64-Bit-Anzahl, Spawn-Filter, Stop während 14-Kristall-DP und sofortiger Neustart.
- ASan/UBSan und ThreadSanitizer auf vollständigen neuen parallelen Suchtests, einschließlich Stream-Vergleich und beschädigter Map-Format-4-Pakete.

## Zielsystem

EXE und DLL ersetzen, gewünschten Level neu laden, Seed Map öffnen. Vor der Leistungsmessung einmalige Navigationserfassung und RNG-Prüfung abwarten. Bei **Generator: verified CPU workers** werden Erzeugung und Bewertung außerhalb des Spiels ausgeführt. Bei **original Unity fallback** greift weiterhin das Spielthread-Zeitbudget.

Gleiche Map, Seed-Anzahl, Suchziel, Genauigkeit und Generator-Methode verwenden; 1/2/6 Worker über vollständige Läufe vergleichen. Suche und normale Spiel-FPS getrennt messen. Windows-Priorität hilft bei CPU-Konkurrenz, garantiert aber keine unveränderten FPS, und ersetzt keine CPU-Kapazität. Gesamt-Spielgeschwindigkeit wurde hier nicht gemessen.

## GPU-Entscheidung und Quellen

Kein GPU-Backend: Die ursprüngliche serielle Stufe wurde beseitigt, und die CPU-Pipeline zeigt Skalierung. Ein weiterer GPU-Pfad braucht zusätzliche deterministische Auswahl-/RNG-/Ranking-Prüfungen, Transfer- und Scheduling-Messungen; GPU-Rechenzeit würde außerdem mit dem Spiel geteilt. Ein GPU-Gewinn wird nicht behauptet.

Unity beschreibt Random als globalen Xorshift128 und warnt vor Aufrufen außerhalb des Hauptthreads: [Unity Random 2020.3](https://docs.unity3d.com/2020.3/Documentation/ScriptReference/Random.html). Die genaue Seed- und Integer-Abbildung wird deshalb gegen den laufenden Zielprozess geprüft. Die mitgelieferte UnityEngine.CoreModule.dll besitzt tatsächlich die vier State-Felder s0–s3; keine angenommenen Feldnamen.
