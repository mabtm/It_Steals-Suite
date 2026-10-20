# Prüfung – PracticeSuite 7.10.1 / mab

| Bereich | Aktueller Nachweis |
| --- | --- |
| Neue Regressionen | physical_edge_tests: alle acht Modi; unzugänglicher Kristallmittelpunkt bei erreichbar bleibendem Pickup, zwei kontinuierliche Kontakte auf derselben Wandseite, fehlender Korridoranker, einzelnes/umgekehrtes/verbundenes Wandsegment, unbekannter Kontakt bleibt unbekannt, unterschiedliche Roh-/Bodenhöhen, fehlende Geometrie und Abbruch während der Kontaktvorbereitung. Die ursprünglichen Pickup-/Segmentfälle scheiterten vor dem Fix. |
| Routen | Bestehende physical_route_tests, planner_tests, Hide-and-Seek-/Outside-Tests mit realen Asset-Floor-Fixtures, Seed- und Portable-Tests. Unbekannte Stücke werden nicht als bestätigte Strecke gezeichnet. |
| Seed-Bewertung | 4096 synthetische Auswahlen mit 22 Kristallen, Hindernissen und Ausreißer: identische Top 10 mit 1 und 6 Workern; vollständige Seed-Buchhaltung, wirksame MST-Aussortierung, unabhängige vollständige Gegenwertung, gleiche Längen in Score, Linie und separater Analyse. Host-Laufzeiten sind keine Game-FPS-Messung. |
| Speicher | Neue Grenzfälle und bestehende physische Routen mit AddressSanitizer und UndefinedBehaviorSanitizer. Kein Leak-Scan: LeakSanitizer kann auf diesem Host die benötigten /proc-Tasks nicht lesen. |
| Managed / Formate | Helper gegen tatsächliche Game-/Unity-/Loader-/Harmony-Referenzen; managed_tests, physical_map_tests und adaptive_capture_tests. Gemeinsam generiertes Layout geprüft. Keine Formatänderung. |
| Regressionen | Native Protokoll-/Replay-/Settings-/Playback-/Clip-/Kamera-/Tempo-/Timeline-/Exportprofil- und Aufnahmequalitätstests. Paketimport einschließlich 7.8.1, 7.9.0 und 7.10.0 mit Originaldaten und Markern. Audio-/Experience-Test mit historischem v6-Fixture. |
| Oberfläche | Vollständige produktive ImGui-Prüfung: 144 Tab-/Viewport-/DPI-/Verbindungsfälle, 216 HUD-Fälle, 128 Moduskombinationen, 288 Minimapfälle und alle Replay-/Finish-/Input-/Clip-/Kamera-/Recording-Check-Gruppen. Physische Karten/Radar zusätzlich bei 480/1920 und 100/150 % DPI. Abschlussmarker nach sämtlichen Gruppen. |
| Builds | Optimierter C#-Helper und Windows-x64-Trainer mit C++20/O2/statischem MinGW, separates ImGui-Testprogramm; Warnungen als Fehler. |

Aktuelle Logs: **tests/results-7.10.1**. Gerenderte produktive GUI mit ausdrücklich synthetischen Karten: **tests/previews-7.10.1**. Wiederhergestellte historische Input-/Audio-Fixtures dienen nur den Regressionen; keine Nutzeraufnahmen wurden verändert. Das Paket enthält keine Originalspiel-/Loader-/Harmony-DLLs.

## Reproduktion

`build.ps1 -GameDir <Spielordner>` baut mit den eigenen lokalen Referenzen. `python generate-layout.py --check` prüft das unveränderte Layout. Native Tests verwenden C++20, `-ITrainer -ITrainer/imgui -pthread -Wall -Wextra -Werror`. `physical_edge_tests.cpp` und `physical_seed_tests.cpp` brauchen keine Argumente. physical_edge_tests akzeptiert optional pickup / segment / anchor / unknown als einzelnen Prüffall.

Der physische Managed-Test und die C#→C++-Fixtures werden wie in **PRUEFUNG_v7_10_0.md** erzeugt. Der komplette ImGui-Test erhält einen absoluten Zeichenlisten-Ausgabeordner; daneben müssen managed-data/input-wire.isinput, managed-data/input-map.isrun und die historischen experience-Fixtures aus tests/results-7.9.0 liegen. Archiv-/Exporttests nutzen frische Ausgabeordner; der Schutz vorhandener Zieldateien bleibt aktiv.

## Grenzen

Kein Live-Test unter Windows/Unity und keine Messung tatsächlicher Spiel-/GPU-FPS. Die Pickup-Region ist der konservative stehende Level-Kamera-Kern aus 7.10.0 mit endlichen Randproben. Kamera-Pitch, Headbob, Ducken, bewegte Objekte, diskrete Physik und jede reale Hide-and-Seek-Wand benötigen weiterhin Live-Abgleich. WIP bleibt bestehen. Die besuchte Reihenfolge ist kein globaler Bereichsoptimalitätsbeweis.

Spielassembly SHA256 **ead3f5f861ad7988ad5fdb6f188bb3f77639927dd496b73066860d6c853f0dd8**, Unity **2021.2.19f1**. Protokoll **26**, Map-Kanal **10**, Map/Run **13**, Input **6**, Frame **4**, Clip-Projekt **7** und Encoder unverändert. Frühere Prüfstände bleiben in ihren Versionsordnern erhalten.
