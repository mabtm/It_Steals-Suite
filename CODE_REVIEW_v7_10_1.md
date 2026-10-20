# Codeprüfung 7.10.1 / mab

## Reproduzierte Ursachen

In der 7.10.0-Kontakttabelle wurden zusätzliche Kontakte verworfen, sobald der Mittelpunktweg geschätzt werden musste. Damit blieb selbst ein sicher auf der eigenen Wandseite berührbarer Trigger ungenutzt. Außerdem setzte die Kontaktvorbereitung einen gültigen Katalog-/Korridoranker voraus, obwohl sie ihre Randkontakte bereits auf der gemeinsamen Bodentriangulation suchte. Beide Bedingungen verwechselten die Erreichbarkeit eines Mittelpunkts mit der Erreichbarkeit des tatsächlichen Einsammelbereichs.

Die räumliche Kollisionsprüfung akzeptierte zweipunktige Fußabdrücke korrekt als Wände. Die Umwegvorbereitung übersprang jedoch alle Teile mit weniger als drei Punkten. Aus einem senkrechten Mesh-Dreieck projizierte Segmente konnten deshalb einen Weg blockieren, aber keinen Umweg beitragen. Dies wurde mit einer zusätzlichen Wand auf ansonsten zusammenhängendem Boden reproduziert.

Die bisherige Schätzungskennzeichnung galt für Mittelpunktpaare im ganzen betrachteten Katalog. Nachgewiesene ausgewählte Kontakte und nicht benutzte unbekannte Paare waren damit nicht sauber getrennt. Bei unbekannten Wegen konnten außerdem Rohpositionen und projizierte Bodenpositionen für Score und Linie auseinanderfallen.

## Implementierung

PhysicalNavigation verwendet für Segmente denselben Sichtgraph, dieselbe Spielerradius-Aufweitung und dieselben Boden-/Kollisionsprüfungen wie für Polygonkanten. Endkappen sind konservativ umrundet. Grenzen von 192 Hindernisteilen, 1024 Knoten und acht Erweiterungen bleiben bestehen; Kandidaten überschreiten die Knotengrenze nicht mehr während eines einzelnen Teils.

PickupRouting prüft direkte Kontakte unabhängig vom Status des Mittelpunktwegs. Fehlende Korridoranker dürfen durch einen separat geprüften, innerhalb der Pickup-Region liegenden Bodenanker für die Kontakttabelle ergänzt werden. Der Korridorsuchbaum erhält dabei keinen fremden Dreiecksindex. Die Metadaten jeder konkreten Verbindung speichern auch deren Unsicherheit.

Der vorhandene Layer-DP verwendet weiterhin genau einen zusammenhängenden Kontakt pro Kristall. Er minimiert zunächst die Zahl unbekannter Verbindungen, dann die echte Länge. Ein vollständig bekannter Weg darf deshalb länger sein als eine kurze, unbrauchbare Luftlinien-Schätzung. Kontaktpaar-Minima bleiben ausschließlich Untergrenzen für die sichere MST-Aussortierung; sie werden nicht als fertige Route ausgegeben. Die Besuchsreihenfolge bleibt Mittelpunkt-basiert und wird nicht als global optimal für Bereiche bezeichnet.

AnalyzeMap und SearchDistances verwenden dieselbe Gewissheit der ausgewählten Verbindungen. Die alte gerichtete Korridorbehandlung für Archive ohne physische Daten bleibt erhalten. Schätzungen verwenden konsistente Endpunkte; unbekannte Teilstücke bleiben beim Zeichnen ausgeblendet. Der vorhandene Zeitansatz bekommt unverändert die tatsächliche Ergebnislinie und deren Status.

Der Spielthread und die Formate bleiben unverändert. Managed-Änderungen betreffen nur die Versionsangabe. Die Gameplay-/Collider-Erfassung und die in 7.10.0 untersuchten Originalspielmechaniken wurden wiederverwendet. Konkrete Nachweise und Grenzen stehen in PRUEFUNG.md.
