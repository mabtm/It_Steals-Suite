# Prüfung 6.9.6

Fortsetzung der 6.9-Linie. Die verworfene Rasterlösung aus 6.10 ist nicht enthalten. Geprüft wurden der bereitgestellte Spielcode, echte NavMesh-Asset-Ausschnitte, Windows-Builds, Host-Tests und die tatsächliche ImGui-Zeichenausgabe. Ein laufendes Windows-/Unity-Spiel war hier nicht verfügbar.

## Light Events: Ursachen und Korrekturen

Der bisherige Hide-&-Seek-Hook hing an MonsterRayTest. Diese Methode prüft, ob das Monster den Spieler sieht; sie ist nicht der Treffer der Spielerlampe. HideAndSeekIt.RayTest schreibt dagegen den tatsächlichen SeeingIt-Trefferzustand und erhöht den zugehörigen Zähler. Die Auswertung nutzt nun diese Quelle und fasst die Strahlen eines Physikschritts zu einem Kontaktzustand zusammen. Ein dauerhaft gehaltener Kontakt wird nicht pro Strahl als neues Ereignis gezählt.

Kameraereignisse hängen am tatsächlichen MouseLook der laufenden Aufnahme, an CameraFlash und am privaten IsStunned-Wechsel des Monsters. Das Raten anhand einer global beobachteten Light-Komponentenreferenz ist entfernt. Ein frameweiser Spielerzustandsbeobachter ergänzt die direkten Ereignisse unabhängig von der Positions-/Pfadabtastrate. Bereits zugeordnete Kamera-Sicht- und Stunbestätigungen sind idempotent. Der frühere pauschale 0,75-s-Zähler-Debounce ist entfernt; eine echte neue Zustandsfolge kann unmittelbar danach wieder ein Ereignis liefern. Die 0,75-s-Ausblendung des visuellen Rings bleibt eine reine Darstellung.

Classic-Lichtkontakte stammen aus dem tatsächlichen RayTest. HasBeenSpotted allein genügt nicht: Im Spiel wird dieser Wert auch durch Batterie-/Endgame-Logik erzwungen. Diese Zustände erzeugen keine zusätzlichen Lichtkontakte. Timer beobachtet alle vier WallAI.isSpotted-Schreibstellen der tatsächlichen Monster3-RayTest-Coroutine. Dadurch werden auch Kontakte erfasst, die vor der periodischen Gegner-Discovery auftreten. Die ursprünglichen Zustandswerte werden beibehalten; zusätzliche RNG-, Eingabe- oder Strahlenabfragen werden nicht eingeführt.

Prüfungen:

- Tatsächlicher Hide-&-Seek-Trefferwriter SeeingIt und der Kamera-Factory-Einstieg im bereitgestellten Spiel-DLL-Code.
- Genau zwei Kamera-Stun-/Erholungs-Schreibstellen und vier Timer-Sicht-Schreibstellen.
- Gehaltene und rasch erneut auftretende Zustandsfolgen, explizite/idempotente Bestätigungen, ungültige Zeiten/Typen/IDs und begrenzte Speicherung.
- C#-/C++-Format-10-Übereinstimmung: Ereignisse, erreichte Beobachter, verworfene Ereignisse und Bewegungsmodell. Ereignisse ohne Monster bleiben lesbar und darstellbar. Format 8/9 bleibt kompatibel.
- Reale ImGui-Ausgabe: weiße Pulse bei allen Kombinationen von Monster-/Pfad-/Flash-Schaltern, auch ohne Monster oder Spielerroute. Die Diagnose unter Details zeigt die tatsächlich erreichten Beobachter dieser Aufnahme. Alte Aufnahmen können fehlende Ereignisse nicht nachträglich erhalten.

Die SDK-/Zustandsprüfungen führen keine echte Unity-Szene aus. Die veränderte Ereigniskette benötigt daher weiterhin einen Live-Spieltest.

## Hide & Seek / Outside: gemeinsame Routenpipeline

Der zusätzliche PlayerFloor-Graph und die dazugehörige PlayerClearance-Hilfsdatei sind entfernt. Der Graph erzeugte zahlreiche Zwischenpunkte an Dreieckzentren und Übergängen, statt die bewährte Funnel-Linie zu benutzen. Eigene Abstands-/Ankergates konnten erreichbare Katalogpunkte verwerfen; nicht überprüfte Ersatzstücke wurden anschließend ausgeblendet und wirkten abgehackt.

Beide Modi nutzen jetzt dieselbe Projektion, Komponentenprüfung, Korridorsuche, Funnel-Konstruktion und Unity-Fallbackwahl wie Classic. Auch die UnityNavigationCapture-Sonderblockade für Hide & Seek ist entfernt. Fertige erfasste Korridore werden in Replays als reine Daten erhalten und streng auf gültige Knoten/Anker geprüft; es werden keine Unity-Agenten oder NavMeshes aus einem anderen Prozess wiederhergestellt. Navigation wird einmal pro Capture vorbereitet, nicht pro Seed.

Die Auswahl tragender Bodenkomponenten statt darüberliegender Dächer bleibt als Geometrievorbereitung erhalten: Beide Hide-&-Seek-Assets enthalten Wandoberseiten, die nicht als Spielerboden dienen. Konturen und Dreiecke stammen aus derselben Auswahl; physische Box-Konturen werden gezeichnet, erzeugen aber keinen eigenen Routengraphen.

Prüfungen:

- Für denselben erfassten Boden ergeben alle acht Modi identische Wegpunkte und Routenlängen.
- Innenkarte: 824 Vertices / 876 Dreiecke / 76 Dachpolygone; Outside: 4743 / 4390 / 130. Je 31 verbundene Boden-Ziele werden erreicht; Wegstücke werden unabhängig gegen die Original-Bodendreiecke geprüft.
- Beide Assets mit ihrem gespeicherten Weltversatz sowie abweichender physischer Spielerunterkante. Keine Rückkehr des leeren Höhenfilters; Rampen und gültige Boden-Konturen bleiben erhalten.
- Synthese-NavMeshes mit tatsächlichen Bodenlöchern, Änderungen des erfassten Meshes, Cache-Wechsel, Abbruch und Legacy-Korridore. Die Fixtures erfinden keine Live-Collider.
- Innenkarte: 2000 Seeds / 22 Kristalle; Outside: 128 Seeds / 52 Kristalle. Ein und sechs Worker liefern gleiche Top 10 und vollständige Zählung. Übrige RNG-/CPU-/Ranking-Regressionsprüfungen einschließlich einer Million eindeutiger Seeds bleiben erfolgreich.

## Routenzeit: echte Strecke und Spielmechanik

MouseLook.FixedUpdate setzt im untersuchten Spiel 0,14 Sprint- bzw. 0,05 Geh-Einheiten pro Physikschritt. Die Bewegungsachsen werden addiert, nicht normalisiert; vor dem Controller.Move wird kein Bewegungs-deltaTime-Faktor eingesetzt. Dieser Abschnitt wurde gegen die tatsächlichen Spiel-Instruktionen geprüft. Ein unterstützter Capture speichert den realen Physikschrittzeitraum. Bei 0,02 s ergeben sich rund 7 u/s geradlinig bzw. 9,90 u/s diagonal für freie Sprintbewegung. Dies ist kein universeller Wert für Ermüdung, Blickrichtung, Eingabeaufbau oder Gegnerverzögerungen.

Das vorhandene Bewegungsprofil integriert jetzt die Wegstücke jedes Messfensters. Die frühere Sehne verkürzte enge Kurven und verzerrte deren Geschwindigkeit. Klassenraten entstehen direkt aus gemessener Zeit/Strecke; ein nachträglicher Skalierungsfaktor ist nicht mehr nötig. Die regionale Abdeckung wird nach Wegstrecke gewichtet. Stillstand und unbekannte Zeit bleiben getrennt. OnTriggerEnter erzwingt keine Kristall-Einsammelpause, deshalb wird Stillstand in der Nähe eines Pickups nicht als feste Pause auf eine andere Route übertragen.

Prüfungen:

- Enge wiederholte Kurven: Profilstrecke entspricht der unabhängig summierten aufgezeichneten Polylinie.
- Gesamtdauer wird aus Bewegung, Stillstand und Lücken ohne angepassten Skalierungsfaktor rekonstruiert.
- Gleiche Strecken in schnell/langsam gelaufenen Regionen behalten verschiedene Schätzungen; Animation folgt der variablen Zeitachse.
- Ziele erzeugen keine erfundene Zusatzpause. Captured 0,14/0,02-Mechanik ergibt die erwartete axiale/diagonale Bewegungsspanne; fehlende/ungültige Modelle werden nicht verwendet.
- Profile bleiben an kompatible Modi, Builds, Spielermaße und bekannte Physikschrittzeiträume gebunden. Ohne Profil wird das Capture-Modell bevorzugt, ohne beides der bezeichnete 8,1–10-u/s-Fallback.

Die vorgeschlagene Distanzroute ist nicht zwangsläufig die global zeitoptimale reale Route. Gegner, Eingabeverhalten, dynamische Wände und genaue Pickup-Kontakte können ihre Ausführungszeit verändern. Eine Modellspanne ist kein statistisches Konfidenzintervall oder zugesagte Endzeit.

## Build- und GUI-Ergebnis

Alle 13 Gruppen erfolgreich: mod, trainer, managed, native, ui, planner, search, settings, portable, playback, hide-seek-player, replay und layout. Protokolle liegen unter tests/results-6.9.6; Vorschauen unter tests/previews-6.9.6 zeigen die reale ImGui-Ausgabe mit ausdrücklich synthetischen Aufnahmen. SDK-Tests verwenden die bereitgestellten Spiel-/Unity-/Loader-DLLs. Native Decoder/Fuzz-/Routenprüfungen nutzen AddressSanitizer/UndefinedBehaviorSanitizer.

Replay-Speicherung, Kategorien/Notizen, Backups, ABORTED, obligatorischer HUD-Status, Koordinaten, Deckkraft, Overlay-Exklusivität und Einstellungen bleiben geprüft. Eine garantierte FPS-Steigerung, eine bereits live bestätigte Lichterkennung oder eine fälschungssichere Anticheat-Bestätigung wird nicht behauptet. DLL und EXE gemeinsam ersetzen, beide Hide-&-Seek-Level neu laden und neue Aufnahmen für die neue Ereigniskette verwenden.
