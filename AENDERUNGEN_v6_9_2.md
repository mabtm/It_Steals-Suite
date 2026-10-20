# PracticeSuite 6.9.2

Weiterentwicklung der Version 6.9 einschließlich der Korrektur 6.9.1. Der verworfene Ansatz aus 6.10 wird nicht übernommen. Die schnelle CPU-Seed-Erzeugung und vollständige Seed-Zählung bleiben erhalten.

## Hide & Seek, Kamera und Eingabe

- Seed Map und Minimap verwenden dieselbe Wandquelle. HideAndSeekIt.OccludeWalls deaktiviert entfernte Wandobjekte; deren Collider werden nun über die Szenenwurzeln einschließlich inaktiver Objekte erfasst. Wand-Tags an übergeordneten Objekten werden berücksichtigt. Boden und Decke fallen anhand der Spielerhöhe aus der Wanddarstellung heraus.
- Gedrehte/scalierte Boxen erhalten ihre wirkliche horizontale Kontur. Geneigte Boxen und Mesh-Ersatzgrenzen bleiben ausdrücklich konservative Näherungen. Die Minimap zeichnet darüber keine zusätzlichen AABB- oder Gegner-NavMesh-Linien. Vollständige Konturen aus dem Kartenkanal haben Vorrang vor den begrenzten Telemetrieboxen.
- Geometrie wird erst nach dem Start-Frame erfasst. F5 setzt auch den gemeinsamen Geometriecache zurück. Das private Spielernetz behält seine Wandabstandsprüfung; auch die Projektion der Katalogpunkte darf keine Wanddurchquerung herstellen.
- Kamera und Körper werden über die tatsächlichen MouseLook-Referenzen gebunden. Der MainCamera-Tag ist nur ein Fallback. Beim Kamerawechsel wird der alte Hitbox-Renderer deaktiviert. Freecam löst die richtige Kamera vom Parent, stoppt den Head-Bob-Animator vorübergehend und stellt dessen Zustand wieder her.
- Während Freecam folgt der playerBody-Abstand in der Hide-&-Seek-Wand-Ausblendung der Kamera. Wandpositionen und Gegnerabstände bleiben unverändert. Der Hook wurde gegen die tatsächliche Coroutine geprüft und umschließt genau eine Positionsabfrage.
- Menübesitz hängt vom angeforderten Eingabemodus ab, nicht mehr von einem verspäteten Application.isFocused-Wechsel. Update und LateUpdate geben den Cursor frei; beim Schließen wird die gespeicherte Spielabsicht wiederhergestellt.

## HUD und Performance

- Optionale Spielerkoordinaten X/Y/Z in beiden HUD-Konfigurationen. Veraltete oder fehlende Positionsdaten erscheinen als --. Alte Segment-IDs aktivieren die neuen Koordinaten nicht versehentlich.
- Passive Fenster verwenden eine premultiplizierte Alpha-Bitmap. Die Panel-Opazität bleibt damit nach dem Schließen des Menüs erhalten. Ein begrenzter Readback mit zwei Puffern wartet nicht auf die GPU; bei noch laufenden Kopien wird ein Frame ausgelassen.
- Passives HUD: 30 FPS. Menü: separate 30/60/120-Einstellung. Versteckte Fenster rendern nicht. Keine neue Messung der realen Spiel-FPS wird behauptet.
- Settings bietet separate, vorübergehende Unity-FPS-/VSync-Overrides mit Anzeige von targetFrameRate, vSyncCount und Display-Hz. Die Spielpräferenzen werden nicht gespeichert/überschrieben; Verbindungsabbruch und Deinitialisierung stellen die übernommenen Werte wieder her.
- Diagnostics gruppiert Verbindung/Build, Fenster/Eingabe, Rendering/Performance, Spieler/Kamera/Geometrie, Run-Prüfung und Aufzeichnung/Speicher.

## Run-Analyse

- Analyze previous run beendet einen gegebenenfalls bereits begonnenen neuen Versuch und analysiert die vorher ausgewählte Aufnahme. Run-ID und Szene bleiben fest, auch wenn das Beenden des aktuellen Versuchs die letzte Aufnahme aktualisiert.
- Aufzeichnung des tatsächlichen Spawns, der Kristallpositionen, Spielerbewegung und Pickup-Reihenfolge mit Zeiten. Bis zu 50 Hz, zusätzlich Pickup-/Endpunkte; lange Aufnahmen werden auf maximal 32768 Positionspunkte reduziert und gekennzeichnet.
- Pink: gemessener Weg; teal: Modellwege; gestricheltes amber: unbestätigte Distanzschätzung. Vergleich von Vollroute sowie derselben bereits eingesammelten Kristalle, einschließlich Distanz bis zum letzten Pickup. Die größere Hide-&-Seek-Tour bleibt eine Heuristik; Pickup-Radien und Movement-Tricks sind nicht vollständig modelliert.
- Separate Dateien in UserData/ItStealsPracticeSuite/RunReviews, auch ohne aktivierte JSON-Reports. Die letzte intakte Aufnahme wird nach einem Neustart eingelesen. Aufnahme und Analyse überschreiben keine Live-Karten-Caches. Routing-Ansicht und private Navigationsberechnung sind im sauberen Speedrun gesperrt.

## Lokale Run-Prüfung

- Pflichtanzeige VALID / UNVALID bei jedem Speedrun einschließlich Practice; Metrik-Auswahl, F2 und Panelüberlappung können sie nicht ausblenden. Die kleine Plakette besitzt eigenen Kontrast. Fehlende/frische Verbindung und Practice werden konservativ als UNVALID angezeigt.
- Jede Feature-Aktivierung wird unmittelbar und dauerhaft erfasst. Kurz eingeschaltete Hitboxen, Fullbright, Freecam oder Seed-Eingriffe bleiben markiert. Verbotene Befehle werden zusätzlich vor der normalen Unterdrückung protokolliert. Such-Stopp bleibt erlaubt.
- Prüfung beim Run-Ende und während des Runs: unterstützter Build, RNG-/Gameplay-Hooks, Zeit-/Beobachtungslücken, Time Scale, auffällige Positions-/Spielerzustände und die zuletzt tatsächlich präsentierte Pflichtanzeige. Normale kurze Fensterübergänge erhalten eine begrenzte Toleranz.
- Hintergrundprüfung bekannter Speichereditor-Namen und angeschlossener Debugger. Windows-Prozessnamen werden ohne Öffnen geschützter Prozesse gelesen. Die Prüfung ist einmal pro Sekunde aktiv und kann umbenannte, injizierte oder kurzlebige Werkzeuge übersehen. Ein Treffer beweist keine tatsächliche Änderung dieses Spiels.
- Prüfergebnis und Gründe bleiben im Journal/Verlauf gespeichert und stehen in optionalen JSON-Reports. Ältere Datensätze erhalten kein nachträgliches VALID; bisherige Statistiken/PBs bleiben erhalten. VALID ist lokale Evidenz und keine Anti-Cheat- oder Leaderboard-Zertifizierung.

## Update

Spiel und Trainer schließen, DLL und EXE gemeinsam ersetzen und neu starten. Nur eine Helper-DLL im Mods-Ordner behalten. Hide & Seek bei geöffnetem Seed-Map-Tab neu laden und die Erfassung abwarten. Koordinaten unter Settings → Overlay configuration aktivieren; Analyse unter Speedrun → Analyze previous run.

Hauptprotokoll 14, 128-Byte-Befehle, 16016-Byte-Telemetrie, Kartenkanal 9 mit 4 MiB, Map-/Run-Format 7. Frühere Kartenformate 1–4 und 6 bleiben lesbar; das verworfene Format 5 wird nicht geladen. Einstellungsversion 12 und Journalversion 7 schützen vor Vermischung mit älteren Schreibern.

DLL/EXE und automatisierte Prüfungen sind gebaut/ausgeführt. Native Unity-Navigation, Windows-Cursor, D3D-Ausgabe und reale Spiel-FPS konnten hier nicht live geprüft werden. Details: PRUEFUNG.md.
