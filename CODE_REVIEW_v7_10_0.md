# Codeprüfung 7.10.0 / mab

## Befunde im Originalspiel

Untersucht wurden die gelieferte Assembly (SHA256 ead3f5f861ad7988ad5fdb6f188bb3f77639927dd496b73066860d6c853f0dd8) und die nativen Collider-/NavMesh-Daten der Szenen. Das ist eine statische Prüfung, kein Live-Physiknachweis.

MouseLook.OnTriggerEnter testet den eigenen Point-Tag des ankommenden Colliders und zieht die Objectives ab; ein Mittelpunktabstand wird nicht geprüft. MouseLook befindet sich auf Main Camera. Dessen Box-Trigger ist in Classic aktiv; Hide and Seek/Outside haben stattdessen einen aktiven Capsule-Trigger (Radius ungefähr 0,47824, Höhe 1,71). Der bewegende CharacterController liegt auf First Person Player. Seine Form für die Wandfreiheit als Pickup-Form zu verwenden wäre falsch.

Die Point-Kugel hat in den geprüften Szenen lokalen Radius 2 und Welt-Skalierung ungefähr 0,5603603. Der echte Kontakt hängt zusätzlich von Kamera, Center-Offset und vertikaler Lage ab. Deshalb werden nur konservative horizontale Kernbereiche der tatsächlich eingeschalteten Kamera-Trigger für stehende Figur/waagerechte Kamera verwendet; kein aus dem Controller oder einem pauschalen u/s-Wert erfundener Pickup-Radius. Abweichende Blickneigung, Headbob, Ducken und spätere Zustandsänderungen sind Grenzen dieses statischen Modells.

Die Hide-and-Seek-Controller sind mit Höhe 0,28 und Radius 0,4 gespeichert. Die effektive Kapselhöhe ist mindestens 0,8. Die vorherige Wandprojektion verwendete die rohe Höhe, das separat exportierte Spielerprofil bereits die effektive Höhe. Außerdem wurde die erste Projektion unabhängig von Bodenkontakt eingefroren.

Die nativen Box-Kataloge enthalten 94 inaktive Collider in Hide and Seek und 58 in Outside; darunter ungenutzte Varianten und Objectives. OccludeWalls/OccludeObjects verwenden die konkreten AllWalls/AllProps/CielingLights-Kataloge. Nur daraus dürfen inaktive Objekte zurück in die statische Erfassung gelangen. Der bestehende Replay-Zugriff auf diese Kataloge wird wiederverwendet.

Die Outside-NavMesh-Daten verwenden AI-Radius 0,15; die Spielerbewegung verwendet Radius 0,4. Eine geometrisch gültige AI-Ecke muss daher noch keine Spieler-Ecke sein. Die bisherige native Routenlogik prüfte Dreiecksabdeckung, benutzte aber die separat erfassten physischen Hindernisse nicht für die Strecke.

## Korrekturen und Datenfluss

SceneGeometry erfasst die tatsächliche stehende Höhenzone und getrennte konvexe Teile. Unlesbare Meshes werden gezählt; ihre AABB ist kein Ersatz für eine Türöffnung. Keine GameObjects werden dafür aktiviert, verschoben oder gebacken. Die kurze Aufnahmewartephase ist zeitlich begrenzt. Die Rebuild-Aktion erfasst erneut.

NavGraph behält Boden/Adjazenz/Funnel und verwendet zusätzlich unveränderliche räumliche Indizes für statische Hindernisse und Bodenflächen. Zu enge Anker/Ecken werden nur auf eine nachgewiesene Bodenposition korrigiert. Jeder neue Umweg bleibt auf Boden und außerhalb der Bewegungskapsel. Unnötige Abknickpunkte werden nur bei vollständiger Sicht-/Bodenprüfung entfernt. Diese Methode wird durch die neuen physischen Daten in allen Modi benutzt. Ältere Aufnahmen behalten ihre etablierten Fallbacks.

PickupRouting bereitet Kontaktkosten im Hintergrund vor. Ein Layer-DP pro Auswahl wählt EINEN Kontakt für ankommenden und weiterführenden Weg. Unabhängige Kontaktpaar-Minima dienen nur der MST-Untergrenze; das fertige Ergebnis wird aus zusammenhängenden Kontakten bewertet und gezeichnet. Die Mittelpunktreihenfolge wird nicht als exakte Bereichsreihenfolge bezeichnet. Canonicalisierte Paare halten Ranglisten, Vorschau und Zeichnung bei unterschiedlichen Katalogreihenfolgen identisch. Alte Daten ohne Physikmodell behalten ihr bisheriges gerichtetes Korridormodell.

Der Kartenkanal wurde bislang nur bei gesetztem Planner-Bit gelesen, obwohl das Radar ausschließlich das Geometry-Bit setzen konnte. Die neue native Abfrage akzeptiert beide. Eine reine Geometrieanforderung startet keine CPU-RNG-Verifikation. Physik-, Frame- und Inputspuren werden dadurch nicht verändert.

Neue v13-Felder gehören zum Cachevergleich und zur geschützten binären Datei einschließlich Prüfsumme. Beide Leser prüfen Größen, Koordinaten, Flags und konvexe Konturen. Kosten werden einmal vorbereitet, nicht im Spiel pro Seed berechnet. Der Kontaktkatalog ist durch maximal 256 Knoten und 2000 Kontaktproben insgesamt begrenzt; größere/unklare Fälle bleiben bei Mittelpunktzielen. Sichtreparaturen und Suche unterstützen Abbruch. Kein Modus wird allein wegen einer fehlenden Geometrieverbindung als unspielbar verworfen.

## Praktische Grenzen

Eine Route bleibt ein statisches Modell und keine garantierte schnellste echte Ausführung. Später bewegte Objekte, Blick-/Haltungsänderungen, diskrete Unity-Kontakte, Gegner und Wartezeiten brauchen einen Live-Abgleich. Hide and Seek/Outside bleiben deshalb WIP. Tests und genaue Prüfgrenzen: PRUEFUNG.md.
