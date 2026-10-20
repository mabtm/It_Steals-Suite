# PracticeSuite 7.10.1 / mab

Nachprüfung der gemeinsamen Pickup- und Routenlogik aus 7.10.0. Der Patch ergänzt keine Hide-and-Seek-Sonderroute.

## Korrekturen

- Ein Kristallkontakt kann jetzt auf nachgewiesenem Boden erreichbar sein, obwohl der Weg zum Mittelpunkt fehlt oder durch eine Wand blockiert wird. Jeder direkte Kontakt wird separat gegen Boden und tatsächliche Hindernisse mit Spielerabstand geprüft.
- Fehlende aufgezeichnete Unity-Korridoranker entfernen keine gültigen Bodenkontakte mehr. Die Indexräume von Boden und Korridorgraph bleiben getrennt. Ein Ersatzpunkt außerhalb des bestätigten Einsammelbereichs wird nicht als gültiger Kontakt akzeptiert.
- Vertikale Mesh-Dreiecke können bei der Höhenprojektion eine Linie ergeben. Diese Linien blockierten schon vorher den Spieler; jetzt liefert derselbe begrenzte Umweggraph auch sichere Kandidaten um ihre Enden. Zusammengesetzte Segmente und beide Orientierungen werden unterstützt.
- Die Kontaktoptimierung bevorzugt für die bestehende Besuchsreihenfolge nachgewiesene Verbindungen. Geschätzte Verbindungen bleiben ausdrücklich markiert und auf der Karte verborgen. Ein unbekannter Mittelpunktweg macht eine vollständig nachgewiesene Kontaktroute nicht mehr pauschal zur Schätzung.
- Bewertung, Zeichnung und Zeitmodell verwenden dieselben tatsächlich ausgewählten Teilstücke. Auch bei Schätzungen mit unterschiedlichen ursprünglichen Höhen stimmen Score und gezeichnete Linienlänge überein.
- Die Seed-Suche erklärt fehlende Mittelpunktverbindungen im Tooltip, ohne deshalb jede Ergebnisroute als geschätzt zu bezeichnen. Tabellen und Text-Export behalten den Status der tatsächlichen Ergebnisroute.
- Die zusätzliche Kontaktvorbereitung ist abbrechbar. Kosten und Prüfungen werden einmal pro Katalog vorbereitet und unveränderlich zwischen Workern geteilt; keine zusätzlichen Unity-Abfragen pro Seed.
- Pakete aus 7.10.0 bleiben einschließlich Marker, Inputs und Begleitdateien importierbar. Der HUD-Editor verwendet die gemeinsame Versions-/Autorzeile.

## Installation

Spiel und Trainer schließen. **ItStealsHelper.dll** und **ItStealsTrainer.exe** gemeinsam ersetzen; danach das Spiel starten und die Szene laden. Einstellungen und Replays behalten. Der Encoder bleibt unverändert.

Protokoll **26**, Kartenkanal **10**, Map/Run **13**, Input **6**, Frame **4**, Clip-Projekt **7** bleiben unverändert. Hide and Seek / Outside behalten ihre WIP-Kennzeichnung.

Das Pickup-Modell bleibt ein konservatives Modell für stehende Figur und waagerechte Kamera mit endlichen Randproben und bestehender Mittelpunkt-Besuchsreihenfolge. Es ist kein Beweis für die global kürzeste echte Route. Ein Live-Test unter Windows/Unity war auf diesem Host nicht möglich.
