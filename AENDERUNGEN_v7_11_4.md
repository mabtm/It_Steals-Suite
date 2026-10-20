# PracticeSuite 7.11.4 / mab

- Routenanzeige bleibt beim Refresh desselben Seeds/Spawns/Kristalllayouts erhalten. Neues Layout oder neue Sitzung setzt sie weiterhin zurück.
- Einzelne unbekannte Verbindungen blenden nicht mehr die gesamte Animation aus. Geprüfte Teilstücke bleiben stehen; unbekannte Lücken bekommen weder Verbindungslinie noch wandernden Marker. Die Minimap verwendet dieselbe Kennzeichnung.
- Unity-Navigation verwendet die geladene NavMeshSurface derselben Szene; Outside hatte zuvor abhängig vom ersten Monster den falschen Agententyp. Fallback berücksichtigt nur aktive Agenten.
- Leere/schlechtere AI-Mitschnitte ersetzen keine besser verbundenen Bodenflächen. Insbesondere ein einzelner Katalog-Ausreißer löscht nicht die Routen aller anderen Kristalle.
- Gemeinsame unveränderliche Tabelle für geprüfte Boden-/Unity-Korridorwege, kurze lokale Reparaturen, Verbindung über vorhandene geprüfte Wege und sichere Kürzungen. Kein separates Hide-and-Seek-Raster; alle Modi teilen denselben Weg.
- Teure Reparaturen werden auf lokale Nachbarn/Komponentenbrücken begrenzt und vorbereitete Daten wiederverwendet. Bounding-Tests beschleunigen Bodenprojektion. Vorschau und Suche verwenden dieselbe Tabelle.
- Große Besuchsreihenfolgen behalten sämtliche Multistart- und 2-opt-Varianten, finden Nachbarn schneller und werden zusätzlich durch einzelne Versetzungen verbessert. 40/52 Kristalle sind heuristisch optimiert, keine mathematische globale Garantie.
- Top10 bevorzugt vollständig geprüfte Routen vor partiellen Schätzergebnissen. Alle Seeds werden weiterhin erfasst; Schätzwerte begrenzen keine möglicherweise besseren geprüften Kandidaten.
- Checked bezeichnet geprüfte Wegstücke; Partial zeigt fehlende Daten. Vorhandene Zeitmodelle erscheinen für vollständige geprüfte Wege wieder. Diagnose nennt wiederverwendete Verbindungen und Reparaturen.
- Helper und Trainer beide 7.11.4: gemeinsam ersetzen, Spiel/Trainer neu starten und Szene frisch laden. Encoder, Main29, Inspector3, Map10/Run13/Input6/Frame4/Audio1/Clip7 unverändert. Alte Replaypakete einschließlich 7.11.3 weiter lesbar. H&S/Outside behalten WIP-Markierung.

Prüfungen, Rohmessungen und Grenzen: PRUEFUNG.md. Kein Live-Windows-/Unity-Test auf diesem Host.
