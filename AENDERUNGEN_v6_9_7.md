# It Steals PracticeSuite 6.9.7

Fortsetzung der 6.9-Linie auf Basis 6.9.6; die verworfene 6.10-Rasterlösung ist nicht enthalten.

- Hide & Seek / Outside: orange WIP-Markierung für Minimap, Seed-Karte/Replay-Karte und Seed-Suche mit kurzer Erklärung im Tooltip. Die gemeinsame funktionierende Routenpipeline bleibt unverändert.
- Passive HUDs: nur tatsächlich belegte Bildregionen übertragen, höchstens vier überlappungsfreie Ausschnitte, zwei nicht blockierende Readbacks je Ausschnitt. Getrennte Panels bleiben an das Spiel gebunden, ohne Eingabefokus. Alpha und der obligatorische VALID/UNVALID-Status bleiben erhalten. Ein frischer Minimap-Frame bestätigt keinen älteren Status-Badge.
- Seed-Suche: vorbereitete Navigationsdistanzen über Suchläufe hinweg wiederverwenden. Zwei Kataloge im Cache; Änderung von Modus, Build, Mesh, Katalogpunkten, Navigationsstatus oder Spielerprofil bereitet neu vor. Suche, Ranking und RNG bleiben gleich. Koordinator und Arbeiter haben niedrigere Thread-Priorität.
- Automatische Zeitschätzung: ursprüngliche Eingaben und tatsächliche Physikbewegung während Speedruns erfassen, fünf getrennte Bewegungsarten lernen und begrenzt speichern. Keine Referenz-Replays oder ortsabhängigen Replay-Zellen. Neue Karten und Replays können den kumulativ gelernten Stand enthalten. Klare Modellspanne und messwertbasierte Abdeckung; unbekannte Klassen bleiben unsicher.
- Nur tatsächliche Collider: die zweite/ungefähre Bounding-Box-Option und ihr Fallback sind entfernt. Box, Sphere, Capsule und CharacterController; nicht unterstützte Collider werden ausgelassen.
- Replay-Darstellung vermeidet die bisherige vollständige Kopie des Analyseobjekts pro Frame. Geschätzte Route/Zeitleiste wird nur bei neuem Analyse- oder Messstand berechnet.
- Diagnostics: separate CPU-Mittelwerte/p95 für Menü, HUD und Helper Update/LateUpdate, übertragene HUD-Bytes, belegte Regionen sowie Messklassen und Speicherstatus.

Hauptprotokoll 15: 128 Bytes Commands, 16276 Bytes Telemetrie. Kartenkanal 9 unverändert; neue Karten/Replays Format 11. Alte unterstützte Formate bleiben lesbar. DLL und EXE gemeinsam ersetzen.

Alle 16 Build-/Prüfgruppen bestanden; Details in PRUEFUNG.md. Ein Live-Test im Windows-/Unity-Spiel bleibt nötig, um Auswirkungen auf echte Spiel-FPS, D3D-Treiber und Fensterfokus zu messen. Hide-&-Seek-Geometrie bleibt ausdrücklich WIP. Keine neuen, noch nicht freigegebenen Ausbauideen enthalten.
