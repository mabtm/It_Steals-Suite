# PracticeSuite 7.10.0 / mab

Neue Routen berühren bestätigte Einsammelbereiche, während der Bewegungscollider frei von statischen Hindernissen bleibt. Die Kamera verwendet in Classic einen Box-Trigger und in Hide and Seek einen Capsule-Trigger; der CharacterController ist davon getrennt. Die Suite verändert keine dieser Spielkomponenten.

## Routen

- Kristallkatalog einschließlich Kugelradius, Skalierung und Collider-Center wird vor der zufälligen Löschung der nicht ausgewählten Kristalle gespeichert.
- Nur eingeschaltete, passende Pickup-Collider und tatsächlich erlaubte Kollisionspaare qualifizieren den Bereich. Der konservative, unter jedem Heading nutzbare Bereich setzt eine stehende Figur und waagerechte Kamera voraus. Ein kleiner innerer Abstand vermeidet numerische Tangenten.
- Acht Randproben, ein sicherer Anker und bereits überlappende Spawnpunkte. Eine dynamische Optimierung verbindet Ein- und Weiterweg am selben Kontaktpunkt. Direkt erreichbare Kontakte können einen unnötigen Umweg zum Kristallzentrum vermeiden, auch wenn dessen Trigger über eine dünne Wand reicht. Die Bewegungskapsel geht dabei nicht durch die Wand.
- Seed-Rangliste, Vorschau, Replay-Vergleich und Zeitmodell verwenden denselben zusammenhängenden Weg. Keine Summe voneinander unabhängiger Bereichsabstände als angeblich fertige Route. Die Besuchsreihenfolge bleibt die bewährte Mittelpunktreihenfolge; für Bereiche gibt es keinen behaupteten globalen Optimalitätsbeweis.
- Gemeinsame Kollisionsprüfung und sichtgeprüfte Kürzung unnötiger Ecken. Lokale Umwege und Vorbereitung sind begrenzt und abbrechbar. Nicht nachweisbare Teilstücke behalten die bisherige Kennzeichnung und werden nicht durch Wände gezeichnet.

## Kartengeometrie

- Gemeinsame Erfassung für alle Modi. Exakte Schnittkonturen von Boxen, auch gedreht/geneigt; lesbare solide Meshes bleiben getrennte konvexe Teile. Runde Hindernisse werden konservativ tesselliert. Nicht lesbare/unterstützte Teile werden gezählt statt durch einen erfundenen Umriss ersetzt.
- Kollisionslayer, IgnoreCollision, Trigger, Spieler, Monster und verschwindende Objectives werden berücksichtigt. Physische Fußabdrücke sind eine stehende 2D-Projektion; bewegliche Props und Monsterwartezeiten werden nicht vorausberechnet.
- Nur aktive Objekte sowie tatsächlich vom Hide-and-Seek-Culling registrierte Objekte. Keine pauschale Aktivierung unbenutzter Levelvarianten.
- Effektive Kapselhöhe mindestens zweimal Radius; Aufnahme nach Bodenkontakt statt in der initialen Fallphase. Alte Karten bleiben lesbar.
- Radar nutzt die vollständigen Konturen auch ohne geöffneten Seed-Tab. Konturen, Props und Boden sind klarer getrennt; kein zusätzlicher, eingerückter AI-NavMesh-Rand als scheinbare Wand. Gemeinsame Darstellung in Vorschau und 2D-Replay.

## Installation und Grenzen

Helper und Trainer gemeinsam ersetzen; Spiel neu starten und Szene neu laden. Hauptprotokoll 26, Kartenkanal 10, Map/Run-Payload 13. Eingabeformat 6, Frame-Kanal 4, Clip-Projekte 7 und Encoder unverändert. Vorhandene Aufnahmen ohne neue Daten verwenden Mittelpunktziele. Hide and Seek und Outside bleiben WIP; auf diesem Host war kein Live-Test unter Windows/Unity möglich. Host-Prüfungen stehen in PRUEFUNG.md.
