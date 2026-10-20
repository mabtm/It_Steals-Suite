# PracticeSuite 7.9.0 / mab

Dieses Update enthält ausschließlich Export-Pipeline, Aufnahmecheck und genauere Animationsaufnahme.

## Exporte

Der v4-Bildkanal besitzt drei Slots mit eigener Serial, Run-ID, Frame-Nummer, Quellzeit, Renderziel und GPU-Anfrage. Unity-GPU-Auslese erfolgt mit AsyncGPUReadback; Pixelreihenfolge und BGRA/RGBA werden einmal mit einer 2×2-Probe gegen ReadPixels verifiziert. Die Probe ist keine Vollbild-Wartebarriere. Nicht unterstützte Formate, fehlender Gerätesupport oder GPU-Fehler verwenden die synchrone Auslese desselben erhaltenen Renderziels.

Im Trainer besitzt jeder Kompositionsslot eine Staging-Textur und D3D11-Eventabfrage. GetData/Map warten nicht blockierend. Nur fertige Frames gelangen geordnet in die vorhandene begrenzte Encoder-Warteschlange. Höchstens drei Frames stehen gleichzeitig zwischen Szenenanfrage und Encoderübergabe. Bei Rückstau wächst der Speicher nicht mit der Clipdauer. Abbruch invalidiert eigene Anfragen; GPU-Ressourcen werden erst nach Abschluss ausstehender Rücklesungen freigegeben. Verspätete Antworten dürfen keine spätere Sitzung bedienen.

Die Quellzeit stammt weiterhin ausschließlich aus dem Frame-Index und derselben Tempoabbildung wie Audio und Vorschau. Dateiformate, Encoderprofile, Qualitäts- und Größenoptionen bleiben erhalten. Eine langsame Berechnung erzeugt keine Zeitlupe. Der JSON-Bericht neben dem fertigen Clip ergänzt tatsächliche Frame-/Async-Zähler, maximale Pipelinebelegung sowie beobachtete Render-/Transfer-/Readbackzeiten. Readbackwerte sind Warte-Latenzen, keine isolierten GPU-Ausführungszeiten; Summen paralleler Schritte dürfen nicht als Gesamtlaufzeit addiert werden.

## Aufnahmecheck

Im Replay ist **Recording check** aufklappbar. Sechs kompakte Zeilen zeigen Movement, Camera, Inputs, Colliders, Animation / visuals und Audio. **Complete**: bei der dokumentierten Beobachtungsauflösung keine erkannten Verluste. **Partial**: Lücken, Fehler, Limits oder unzuverlässige Audiowege. **Missing**: keine nutzbaren Daten. **Unknown**: historische oder nicht verifizierte Nachweise. **Not needed** gilt nur für tatsächlich nicht gebundene optionale Quelltypen.

Neue erfolgreiche Beobachtungen verlängern die Abdeckung auch bei unveränderten Tasten/Posen. Fehler und maximal 16 genaue Lückenintervalle pro Erfassungskanal bleiben erhalten. Ende und letzte tatsächlich beobachtete Zeit werden getrennt behandelt; ein erzwungener Endpunkt verdeckt keine frühere Lücke. Collider-/Grafiklimits, fehlende Spuren und Audioidentität/Checksumme werden zusätzlich geprüft. Unter Recorded gaps sind die zusammengeführten Lücken anklickbar; die Wiedergabe pausiert an deren Beginn. Der kopierbare Bericht enthält Run, Szene, Seed, Format, Kanalstatus, Counts, Gründe und Zeiten. Der Check wird einmal beim Laden berechnet und gecacht.

Der Aufnahmecheck bewertet Datenverfügbarkeit. Er ändert weder die Run-Validierung noch bestehende Originalreplays und beweist keine manipulationssichere Herkunft. Bei alten Formaten fehlen Beobachtungsnachweise; daraus wird keine Vollständigkeit erfunden.

## Animationen

Dynamische Transform-/Knochenposen: bis 60 Hz, bei schneller Translation/Rotation, Skalierung, Zustandswechseln und Übergängen bis 120 Hz, jeweils begrenzt durch tatsächlich beobachtete Spielframes. Der bereits vorhandene grafikbezogene Aufnahmepfad bleibt bestehen: keine zusätzliche Animation-/KI-Simulation, keine Prefab-Kopie oder Szenensuche in den Samplecallbacks.

Ein begrenztes Fenster von 16 Posen prüft alle beobachteten Zwischenwerte gegen die kürzeste Quaternion-Interpolation. Toleranzen pro Transform: 0,00075 u Position, 0,0001 Skalierung, 0,15 Grad Rotation. Nicht konstante Abschnitte erhalten spätestens nach 0,1 s einen Knoten. Statische Posen werden verdichtet; Beginn einer Bewegung, harte Wechsel und tatsächliches Aufnahmeende bleiben erhalten. Diese Grenzen gelten für beobachtete Posen, nicht für ungemessene Bewegungen zwischen Spielframes oder kumulierte Fehler einer kompletten Knochenkette.

Sprites werden bis 120 Hz auf tatsächliche Wechsel geprüft, dynamische Renderer/Lichter bis 60 Hz, statische Meshzustände weiterhin bis 10 Hz. Blendshape-Abtastung steigt bei schnellen Gewichtsänderungen/Übergängen auf 120 Hz. Unveränderte Werte müssen nicht pro Frame gespeichert werden. Bestehende Aufnahme-/Dateigrenzen bleiben erhalten und werden im Check sichtbar.

## Installation und Kompatibilität

Hauptprotokoll 25 (192-Byte-Commands / 16952-Byte-Telemetrie), Frame-Kanal 4, Eingabeformat 6. Beide geänderten Binärdateien gemeinsam ersetzen. Map-Kanal 9 / Map-Payload 12, ISCLIP 7 und Audioformat 1 bleiben erhalten. Eingabeformate 1–5 und bisherige Pakete einschließlich 7.8.1 bleiben lesbar; neue Format-6-Dateien benötigen den neuen Leser. Der Encoder bleibt binär unverändert. Die bisherigen Fullbright-/Pickup-Audio-Fixes aus 7.8.1 bleiben erhalten. Wandfading, Hide-&-Seek-Geometrie und Reportdienst wurden nicht überarbeitet.
