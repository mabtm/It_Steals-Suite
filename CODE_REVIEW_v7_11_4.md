# Codeprüfung – PracticeSuite 7.11.4 / mab

## Bestätigte Ursachen

SuggestedSlice verwarf bereits beim globalen estimated-Flag die gesamte Route. Die nicht animierte Minimap zeichnete dagegen einzelne bekannte Segmente. Der neue Slice trägt pro ausgegebenem Segment eine Ausblendungsmaske und einen getrennten Markerstatus. Die Ausgabe lässt ausschließlich bekannte Teile durch; fehlende Metadaten bleiben konservativ unbekannt. Das verhindert Linien über Lücken. Zeitangaben bleiben an vollständige Prüfung gebunden.

SeedMapUI verglich Daten-Shared-Pointer. Neue Veröffentlichungen derselben Karte/Verifikation setzten deshalb die Benutzerauswahl zurück. Die Bindung prüft jetzt PID/Sitzung/Szene/Epoch/Seedtyp/Seed/Spawn und tatsächliche Spawn-/Kristallauswahl; Metadaten-/Arbeits-/Geometrie-Refresh erhält die Einstellung. MapWorker erhält vorhandene Boden-/Umrissdaten während eines kompatiblen Hintergrundauftrags.

UnityNavigationCapture nahm den ersten Monster-Agententyp. Die gelieferten Daten zeigen im Outside-Level Surface−334000983 neben Spider-Agent0. Aktive geladene Surfaces derselben Szene sind jetzt maßgeblich. Native NavGraph-Fallbacks verglichen zuvor nicht, ob ein AI-Mitschnitt überhaupt besser war: ein Ausreißer konnte eine brauchbare Bodentriangulation gegen leere Anker austauschen. Der neue Vergleich berücksichtigt gültige Anker und verbundene Katalogpaare; schlechte Mitschnitte verdrängen den Boden nicht. Explizite ältere Korridormodelle bleiben bei vorhandenen Ankern unterstützt, Indizes werden geprüft.

## Gemeinsame Wegvorbereitung

PreparedRoutes cached pro NavGraph eine unveränderliche Tabelle. Zuerst werden vorhandene Korridore ohne wiederholte teure Reparatur geprüft. Vier nahe fehlende Nachbarn pro Katalogposition werden mit dem bisherigen Reparaturcode verfeinert (max.512); verbleibende Komponentenbrücken sind auf min(256,4n) Versuche begrenzt. Eine Shortest-Path-Closure verbindet ausschließlich bereits geprüfte Polylinien. Ihre Rekonstruktion behält reale Zwischenpunkte; Kürzungen verwenden dieselbe PhysicalNavigation.Visible-Prüfung. Auch alle einzelnen normalen Checked-Segmente benötigen Bodenabdeckung, nicht nur Wandfreiheit. Ein vollständiger Reparaturweg kann einen ungeeigneten rohen Korridor ersetzen.

Maximal384 Katalogpositionen, zwei Millionen gespeicherte Punkte je Basis-/Endtabelle; Überschreitung ist ein ausdrücklicher Fehler. Cancel unterbricht Vorbereitung/Closure/Rekonstruktion; call_once wird nach einem geworfenen Abbruch erneut verwendbar. Tabellen halten keine Caller-/Progress-Referenz. Keine Unity-Aufrufe auf nativen Workern und keine neuen Game-/Renderthreads. Bestehender zweistufiger Hintergrundpfad und Zwei-Katalog-LRU bleiben erhalten.

AnalyzeMap und SearchDistances teilen dieselbe globale Katalogtabelle. Keine unabhängig abweichenden Punkte-/Längenmodelle in Vorschau oder Top10. Nur tatsächlich benutzte Kanten entscheiden das Route-estimated-Flag; unbenutzte Matrixlücken machen einen geprüften Weg nicht pauschal unbekannt. Fehlende Daten bleiben als geschätzte Längen zur vollständigen Seed-Erfassung erhalten, aber ohne erfundene Linie/ETA.

Die Top10 sortiert vollständige geprüfte Wege vor Schätzrouten. Der optimistische MST-Schnitt verwendet erst eine vollständige geprüfte Top10 als obere Grenze; eine kurze Schätzroute darf keinen längeren tatsächlich geprüften Kandidaten ausschließen. Alle Seed-/Filter-/Match-Zähler bleiben erhalten.

## Große Routen und Kompatibilität

Sortierte Nachbarreihenfolgen erhalten die vorherigen NN-Ties und sämtliche Starts. Bisheriges offenes 2-opt bleibt erhalten. Höchstens16 zusätzliche Relocation-Pässe verbessern den Gewinner; die Länge wird danach aus der finalen Reihenfolge neu summiert. Der exakte DP ist unverändert. Der Vergleich gegen den alten Solver prüft kleinere exakte und große heuristische Fälle, einschließlich64-Ziel-Bitmasken, ohne schwächere Heuristik-Starts einzuführen.

Helper/Trainer7.11.4, Main29 / Inspector3; Größen und Dateiformate unverändert. Encoder bleibt byteidentisch samt zugehörigem Quellarchiv. Replay-Import7.11.3 explizit ergänzt. Run-Sperren, Input-/Audio-/Exportpfade unberührt. WIP-Geometrie bleibt als solche sichtbar. Nachweise und Grenzen in PRUEFUNG.md; keine Live-Windows-/Unity- oder globale Optimalitätsbehauptung.
