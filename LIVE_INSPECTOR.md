# Live Inspector – 7.11.2 / mab

## Einstieg

**F7** startet den **Picker** direkt im Spiel; alternativ im eigenen **Inspector**-Tab auf **Picker [F7]** drücken. Clean Speedrun ausschalten oder Practice verwenden; während 3D-Replay/Export ist der Inspector gesperrt. Er ist ein Debugging-Werkzeug, kein zulässiges Speedrun-HUD.

1. Über das Objekt fahren: Name, Bildschirmrahmen und bei unterstützten primitiven Collidern die tatsächliche Drahtform werden sichtbar. **Mausrad** wechselt zwischen Treffern unter dem Zeiger. **Tab** öffnet die Trefferliste und friert den Abfragestrahl ein, damit die Maus die Liste bedienen kann. Gewünschte Zeile wählen; **Select highlighted**, Doppelklick oder **Enter** bestätigt. Linksklick in die Welt bestätigt das aktuell markierte Ziel. **Esc / RMB / F7** brechen ab und stellen den vorherigen Kontext wieder her. **F3** bricht ebenfalls ab.
2. Nach Auswahl erscheint eine kleine verschiebbare **Inspector / object**-Karte. Name, Typ, Aktivität, Weltposition und Kamera-Distanz bleiben sichtbar. **Fullbright**, **Picker**, **Copy ID**, **Focus in Freecam**, **Details** und **Return to game [F3]** sind direkt erreichbar. **Pin card** lässt die Karte beim Spielen passiv stehen. X beendet den Inspector samt Watches und eigener Freecam. **End inspection** im Haupttab beendet die Sitzung und kehrt zur Cheats-Seite zurück. Auf kleinen Bildschirmen ist die Karte scrollbar.
3. **Details** öffnet den eigenen Tab **Inspector** und dessen **Technical details**. Standardmäßig bleiben diese geschlossen. Nur bei geöffneter Detailansicht liest der Helper Materialien, zusätzliche Transform-/Collider-Eigenschaften und Felder der Spielskripte. Watches laufen nach expliziter Aktivierung weiter. Properties führt auch getrennt gelieferte Object-/Transform-Werte zu genau einer Gruppe zusammen; der Einstieg beginnt am Anfang. Feldsuche und +/− erlauben bis zu acht beobachtete Felder. Private Herkunft steht im Tooltip; komplexe Werte werden nicht beliebig expandiert.
4. **Object list** lädt den Katalog erst auf Anfrage. All / Colliders / Renderers / Inactive filtern den letzten Refresh. **Refresh objects** liest neu. Inaktive und colliderfreie Objekte sind hier auswählbar. **Select parent** und **Children / overlapping hits** erleichtern die Navigation; ein GameObject zeigt seine eigenen Komponenten.

### Akkurate Auswahl

Standard ist **Visuals + colliders**, einschließlich Trigger. **Visuals only** beschränkt auf Grafiken, **Colliders only** auf Physiktreffer; Trigger sind separat wählbar. Wand und aufliegendes Graffiti erscheinen als getrennte Ziele, auch ohne Collider. Mehrere Komponenten desselben GameObjects bilden eine Auswahl. Die Liste des letzten Strahls bleibt auch nach Auswahl auf der Karte und in der Grundansicht verfügbar. Eltern/Kinder bleiben separat erreichbar.

Die Treffer nennen ihre Quelle: **Mesh surface**, **Sprite surface**, **Collider** oder **Bounds estimate**. Lesbare MeshRenderer und einfache SpriteRenderer werden gegen ihre tatsächlichen Dreiecke getestet; statisch gebatchte Meshes verwenden nur den zugehörigen Submesh-Bereich. Textur-Alpha, Masken und Shader-Verformungen werden nicht ausgewertet. Die Liste enthält auch weiter hinten liegende Objekte auf demselben Strahl. Colliderfreie Skinned-/nicht lesbare/große Geometrie verwendet ausdrücklich geschätzte Bounds, keine erfundene Kollisionsfläche. Der Bildschirmrahmen umschließt die Bounds, er ist keine Pixelkontur.

Der erste visuelle Picker baut den bestehenden Hierarchiekatalog verteilt über Frames auf; währenddessen ist **Preparing visual index** sichtbar. Refresh objects erneuert diesen Stand. Unsichtbare, deaktivierte und von der Kamera ausgeblendete Renderer werden ausgelassen. Der eigene Spielerkörper wird an der Spielkamera übersprungen, in der Spectator-Freecam bleibt er untersuchbar. Verschwundene Ziele werden nicht durch einen Nachbarn ersetzt.

Der Trainer speichert die Mauskoordinaten aus WM_LBUTTONDOWN und rechnet sie über den tatsächlichen Spielclient um. Die Kamera setzt dieselben normierten Koordinaten mit ihrem Viewport in einen Strahl um; Renderauflösung und UI-DPI werden nicht miteinander vermischt. Der Klick wartet auf die zugehörige Hover-Beobachtung. Vor dem Übernehmen prüft der Helper dieselbe Szene, Kennung und Objekt-ID erneut mit derselben Abfrage. Ein verändertes Ziel wird abgewiesen; der Picker bleibt für einen neuen Versuch aktiv. Gesättigte Trefferpuffer liefern einen ausdrücklichen Fehler statt einer zufälligen Auswahl. Kleine Rundungs-/Latenzgrenzen und Live-Grenzen siehe PRUEFUNG.md.

## Geometrie prüfen

Geometry zeigt den Mitschnitt aus dem tatsächlich verwendeten SceneGeometry-Pass. Included bedeutet, dass die Kontur in die statischen Daten aufgenommen wurde. Andere Gründe nennen z.B. Trigger, deaktivierten Collider, nicht akzeptiertes inaktives Objekt, bewegten Actor, Pickup, fehlende Mesh-Lesbarkeit oder die Spielerhöhenzone.

Die Farben im Spiel: **Cyan = aktiver echter Collider**, **Orange = inaktiver/deaktivierter echter Collider**, **Violett = erfasster Karten-Fußabdruck**. Der Fußabdruck liegt zur Lesbarkeit leicht über der erfassten Bodenhöhe. Er ist ein zweidimensionaler Ausschnitt aus dem Hindernis für das dabei erfasste Spielerprofil; kein vollständiges dreidimensionales Kollisionsmodell. Die Erfassung kann älter sein als die Live-Auswahl. Aufnahme-Frame, Zeit, Grounded-Zustand, Radius und Höhenzone werden angezeigt.

Box, Sphere, Capsule und CharacterController nutzen die vorhandenen tatsächlichen Formdaten. Mesh-Collider zeigen lesbare Dreieckskanten der Auswahl, maximal 8192 Kanten insgesamt als Auswahl-Snapshot mit Frame-Angabe (bei Topologieänderung neu auswählen). Cooked-Convex-Hüllen sind in der Engine-API nicht verfügbar und werden ausgelassen; nicht lesbare/größere Meshes werden ausdrücklich ausgelassen. Renderer-Bounds werden niemals als tatsächliche Collider-Drahtgeometrie ersetzt.

Focus in Freecam stellt die vorhandene Spectator-Kamera auf die Auswahl. Erfolgreiche Bestätigung bringt den Fokus zurück ins Spiel. RMB drehen, WASD/QE bewegen, Shift schneller, F3 zum Menü. End inspection oder X beendet eine vom Inspector gestartete Freecam. Eine bereits vorher aktive Benutzer-Freecam bleibt erhalten. Inaktive Objekte werden durch das Fokussieren nicht aktiviert.

## Werte und Ursachen

Watch liest nur die angehefteten Werte einmal pro beobachtetem Spiel-Frame; die Anzeige erhält bis zu fünf Veröffentlichungen pro Sekunde. Das Protokoll hält maximal 128 Änderungen mit einem zusätzlichen Byte-Budget. Ältere Einträge werden gezählt und verworfen. Diese Zeit bezeichnet die Beobachtung: mehrere Änderungen innerhalb desselben Frames oder Physikticks können fehlen. Die Ansicht nennt deshalb keine erfundene Ursache oder vermeintliche Aufrufstelle.

Compare → Capture baseline speichert die aktuell angezeigte Auswahl. F3 schließen, eine Spielaktion ausführen, F3 zurück zu den geöffneten Details: geänderte, hinzugekommene oder nicht mehr beobachtete Werte werden verglichen. Replace baseline setzt den Bezug neu. Auswahlwechsel, neue Spielszene/-sitzung oder vollständiges Schließen verwerfen den Bezug.

## Zwei Punkte untersuchen

Route probe → Pick start / Pick end: zwei tatsächliche feste Oberflächen anklicken. Check route verwendet den passenden aktuellen Live-Kartenstand im Hintergrund. Angezeigt werden ursprüngliche und auf stehenden Boden projizierte Endpunkte, Komponentenverbindung, Weg und Länge. Die bestehende gemeinsame Bodentriangulation und die physische Spielerradiusprüfung werden wiederverwendet; alte Korridor-Knoten werden nicht als Boden-Dreiecksindizes benutzt.

Fehlende Böden, unverbundene Komponenten und ausgeschöpfte Umwegbudgets liefern keinen erfundenen direkten Weg. Navigation-only kennzeichnet fehlende/unvollständige physische Daten. Das Ergebnis beschreibt nur die erfassten statischen Hindernisse, nicht zukünftige Monster-/Türbewegung oder einen mathematischen Optimalitätsbeweis. Neue Punkte und Szenenwechsel verwerfen die alte Probe. Hide and Seek / Outside bleiben WIP.

## Lokaler Diagnose-Schnappschuss

Save diagnostic snapshot unter Technical details speichert die aktuell beobachtete Auswahl und optionalen Vergleich einschließlich Kamera, Probe, Spiel-/Tool-Version, Assembly-Dateihash, Seed und Seed-Art sowie BuildDiagnostics. Copy report path kopiert den Speicherort. Unbekannter Seed bleibt null; Kind 1 ist nur der initiale RNG-Seed, Kind 2 ein gesetzter Practice-Basisseed. Laufzeit-Objekt-IDs gelten nur für diese Sitzung.

Der Ordner liegt im bestehenden Trainer-Einstellungsbereich unter Diagnostics/Inspector. Es erfolgt kein Upload. Dateien werden mit neuem Namen exklusiv erzeugt und nicht überschrieben. Dies ist ein lokaler Debugging-Bericht, keine Save-State-Wiederherstellung und kein unabhängiger Unveränderbarkeits- oder Moderationsnachweis.

## Grenzen und Last

Der Katalog ist ein auf Frames verteilter Refresh, kein atomarer Mitschnitt aller Objekte. Der visuelle Picker fordert ihn beim ersten Gebrauch an; reine Objektlisten bleiben auf Anfrage. 20000 Objekte und ein 4-MiB-Paket sind die Grenzen; Teilabdeckung wird angezeigt. Scriptfelder werden nur aus der originalen Spielassembly gelesen, ohne beliebige Getter/Methoden. Bis zu 512 Werte und 32 eigene Collider, 512 Konturteile / 4096 Punkte; Paket- und Mesh-Limits werden kenntlich gemacht. Kleine Hardware-/Unity-Spitzen lassen sich ohne Live-Test nicht ausschließen.

Hauptprotokoll 29 und Inspector-Kanal 3 müssen zusammenpassen. Bei Verbindungs-/Szenenfehlern wird nicht aus einer anderen Sitzung weitergelesen. Der Helper wartet nie auf den Inspector-Mutex. Details werden begrenzt gelesen; Katalog, Details und Hover haben getrennte Revisionen. Hover-Abfragen sind auf 30 Hz begrenzt; ausgewählte Eigenschaften auf 5 Hz. Ein Inspector-Fehler unterbricht nicht den unabhängigen Telemetrie- und Run-Aufzeichnungspfad. Aktivierung ist als LiveInspector-Hilfe (512) sticky in der Run-Validierung; die Funktion ist in Clean Speedrun zusätzlich im Helper gesperrt.

Mesh-Wire-Präzision: Primitive werden live gelesen. Nicht-konvexe lesbare Mesh-Collider zeigen einen begrenzten Source-Mesh-Snapshot vom Auswahl-Frame; bei Topologieänderungen neu auswählen. Ausgetauschte Mesh-Referenzen werden nicht mit alten Kanten gezeichnet. Cooked-Convex-Hüllen sind nicht verfügbar und werden ohne Ersatz ausgelassen.


### Grenzen der visuellen Auswahl

Maximal 20000 Renderer; auf dem Host gecachte Bounds werden in Portionen von höchstens 64 Einträgen und mit einem Zielbudget von 0,35 ms pro Frame aktualisiert. Die bis zu 128 Strahlkandidaten werden vor Verwendung gegen aktuelle Engine-Bounds, Aktivität und Kameralayer geprüft. Bei sehr beweglichen Objekten kann der vorgelagerte Bounds-Stand kurz nachlaufen. Der Katalog wird nach Szenenwechsel verworfen; neu erzeugte Objekte werden beim nächsten Refresh erfasst.

Maximal eine neue Geometriekopie pro Spiel-Frame, 16384 Vertices / 24576 Indices pro Geometrie und 8 MiB / 1024 Cacheeinträge. Das Dreiecksbudget beträgt 32768 pro Abfrage; darüber bleibt der Treffer eine gekennzeichnete Bounds-Schätzung. Geteilte unveränderte Meshes nutzen denselben Cache. Einfache Spritewechsel und Flip-X/Y werden berücksichtigt. Mesh-Topologieänderungen an derselben Resource benötigen einen Refresh. Die gewählte Ebene ist an ihre ID gebunden; nicht verfügbare Ziele bleiben abgewiesen. Mehr als 32 veröffentlichte Ziele, ein voller Broad-Phase-Puffer oder ein begrenzter Rendererindex werden als Teilmenge markiert.

Keine Änderungen an Spielobjekten, Materialien oder Collidern, keine zusätzliche GPU-Auslese. Konkrete Windows-/Unity-Latenz und Spiel-FPS müssen im laufenden Spiel geprüft werden.

## Darstellung ab Trainer 7.11.5

- **Style** ist im Inspector-Tab und in der interaktiven Objektkarte erreichbar. Auswahl-Effekte, Inspector-Sounds, relative Lautstärke, Paneldeckkraft und Konturstärke werden automatisch gespeichert. Theme-Farben und Interface-Schrift bleiben die gemeinsame Grundlage.
- Hover zeigt Eckmarkierungen am bereits vorhandenen projizierten Auswahlrahmen. Bestätigung zeigt einen kurzen Randimpuls. Diese Bildschirmrahmen sind keine zusätzlichen exakten Mesh-Konturen; die eigentlichen Collider-Wires bleiben separat und unverändert.
- LIVE, INACTIVE und WAITING geben den tatsächlichen Beobachtungszustand wieder. Die Objektkarte zeigt Name, Typ, ID, Position und Entfernung. Lange Werte und Namen sind weiterhin im Detailbereich beziehungsweise Tooltip vollständig zugänglich.
- Änderungen werden beim Eintreffen einer neuen Detail-Veröffentlichung hervorgehoben. Es wird keine höhere Abtastrate vorgetäuscht. Der Cache ist auf 1024 Einträge begrenzt und wird beim Objekt-/Sitzungswechsel zurückgesetzt.
- Die Detailansicht fährt in 180 ms auf beziehungsweise zu; globale reduzierte Bewegung schaltet Übergänge unmittelbar. Auswahl-Effekte lassen sich zusätzlich lokal abschalten.
- Vier kurze originale PCM-Cues bestätigen Öffnen, Auswahl, Anheften und Schließen. Sie nutzen den bestehenden Audio-Worker, folgen zusätzlich dem globalen Lautstärkeregler und werden bei fremdem Fokus oder aktiven Speedruns unterdrückt. Hover und Datenänderungen bleiben stumm.
- Escape schließt zuerst ein offenes Popup. Ein weiterer Druck kehrt von der Objektkarte ins Spiel zurück.

Dies ist eine Trainer-Oberflächenänderung. Inspector-Kanal 3 und der Helper 7.11.4 bleiben unverändert; es kommen keine Objekt- oder Geometriescans hinzu.
