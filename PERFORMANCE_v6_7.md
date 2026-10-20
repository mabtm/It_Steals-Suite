# Performance und Routing 6.7

## Was die Screenshots tatsächlich zeigen

| Einstellung, Camera Bright | Durchschnitt | Aktuell | Automatische Grenze | Generierungsmodell |
|---|---:|---:|---:|---:|
| Exact up to 15 / auto | 17.153 Seeds/s | 18.446 Seeds/s | 0,25 ms/Frame | 268 Seeds/Batch-Frame; 0,94 µs/Seed |
| Fast heuristic | 17.390 Seeds/s | 17.695 Seeds/s | 0,25 ms/Frame | 267 Seeds/Batch-Frame; 0,94 µs/Seed |

Camera Bright hat 18 Kristalle. Beide Einstellungen verwenden deshalb denselben heuristischen Solver; nur bis 15 Kristalle ist die Reihenfolge exakt. Der kleine Unterschied ist keine Messung eines Exact-/Heuristic-Speedups. Unterschiedliche Zufallsschlüssel, FPS und Laufzeiten verändern den Vergleich zusätzlich.

Die 0,25-ms-Grenze ist ein klarer Generierungsengpass. Das bisherige Auto-System senkte die Arbeit, wenn die Paketbestätigung nicht in mindestens 70 % der Game-Frames bereit war. Bei schnellerem Spiel als Trainer ist normales ACK-Warten zu erwarten und keine Aussage über ausgelastete Routenthreads. Dadurch konnte die Regelung bis zum Minimum sinken, obwohl Erzeugung und Prüfung aufgeholt waren. Zusätzlich blieb eine moderate Änderung der Framezeiten unter 30 % am Minimum hängen.

Die neue Steuerung startet mit 1 ms, kalibriert mehrere Frames, berücksichtigt echte Frame-Verlangsamung/Stalls und kann am Minimum wieder auf normale externe Taktung reagieren. ACK/Queue begrenzen die Produktion weiterhin; die Arbeit wird aber nicht allein wegen normaler ACK-Wartezeit heruntergeregelt. Die vorhandene Queue kann keine ungelesenen Pakete überschreiben.

## Reproduzierbarer Controller-Test

60 Sekunden modellierter Verlauf, 120 Game-FPS, ACK nur jeden zweiten Frame. Für die angenommene Layout-Arbeit werden ausschließlich die 0,94 µs/Seed aus dem Screenshot und die Paketgrenze von 2.048 verwendet:

| Controller | Grenze am Ende | Modellierte Rate |
|---|---:|---:|
| 6.6 | 0,2500 ms | 16.218 Layouts/s |
| 6.7 | 1,6667 ms | 105.726 Layouts/s |

Das reproduziert den Steuerungsfehler und dessen Korrektur. Es ist **kein gemessener Unity-Durchlauf und kein Versprechen von 105.726 Seeds/s**. Tatsächliche Game-FPS, ACK-Takt, Unity-InitState/Range-Kosten und die nun vollständigere Routenbewertung bestimmen die reale Rate. Die Grenze bleibt kooperativ: eine einzelne Operation kann länger dauern.

## Warum bisher wenig oder gar keine Kandidaten erschienen

Scored matches plus Bound skipped ergeben auf den Bildern nur einen kleinen Teil der geprüften Seeds. Die restlichen Layouts werden hauptsächlich wegen fehlender Projektion/Verbindung als unerreichbar ausgeschlossen; dies war während der langen Suche nicht sichtbar. Ein schneller Solver hilft solchen Layouts nicht. Jetzt werden Unreachable und die Projektionen von Spawns/Kristallkandidaten laufend gezeigt.

Die neue bevorzugte Grundlage sind Unitys eigene vollständige NavMesh-Pfade statt ausschließlich rekonstruierter Dreieckskanten. Die Abfragen laufen einmalig und schrittweise im Game-Thread. Die Besuchsreihenfolge und Seed-Bewertung bleiben im nativen Workerpool. Für die vorbereitete Distanzmatrix liest der Captured-Graph unmittelbar die Dijkstra-Kosten statt pro Paar eine temporäre Linie zu erzeugen. Nur die besten sichtbaren Touren bekommen vollständige Pfadlinien.

Ein synthetischer 19-Kristall-Timer-Test ohne Roh-Dreiecke führt 2.000 Layouts durch die tatsächliche Suchpipeline und liefert zehn verschiedene Kandidaten samt Linie in beiden Genauigkeitsmodi. C#-Korridorpakete werden unabhängig im C++-Decoder getestet. Getrennte Komponenten/Stockwerke, blockierte Wegstücke und fehlende Anker erzeugen keine Luftlinien.

## Grenzen und fairer Spielvergleich

Die echte Timer-/Camera-Bright-Geometrie und die Unity-Abfragen sind auf diesem Host nicht ausführbar. Die genaue Ursache ihrer konkreten fehlenden Wege kann ohne deren Laufzeit-Capture nicht bewiesen werden. Das Update adressiert die Rekonstruktionsschwäche direkt und ergänzt Agent-/Anker-/Abfrage-/Kanten-Diagnose. Fehlende echte Navigation bleibt erkennbar statt als gültige optimale Route ausgegeben zu werden.

Zuerst DLL/EXE gemeinsam ersetzen und den Timer-Level im Planner neu laden. Die erste Navigations-Erfassung abwarten. Für den Vergleich dann gleiche Map, Anzahl, Worker und Präzision verwenden; Spiel-FPS/Framezeiten mitbeobachten. Einmaligen Capture-/Vorbereitungsaufwand getrennt von der eigentlichen Seed-Rate bewerten. Mehr gültige Routen bedeuten auch mehr echte Solver-Arbeit statt schneller Verwerfungen; vollständige Daten haben Vorrang vor einer künstlich hohen Rate.

MST-Untergrenze, genaue Hash-Kollisionsprüfung, begrenzte Caches/Queues, wiederverwendete Auswahl-/Matrix-/DP-Puffer und RNG-finally sind erhalten. Die Werte aus PERFORMANCE_v6_6.md sind historische synthetische Solver-Vergleiche, keine aktuellen Camera-Bright-Livezahlen.
