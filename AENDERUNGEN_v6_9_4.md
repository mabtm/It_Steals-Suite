# 6.9.4

- Hide & Seek verwendet dieselben zusammenhängenden Boden-Dreiecke für Routing, Minimap und Replay. Mesh-Begrenzungsrechtecke schließen keine Durchgänge mehr. Isolierte Dächer werden ausgeschlossen; Rampen bleiben erhalten. Outside erreicht das Knotenlimit nicht mehr durch redundante Übergangspunkte.
- Hide & Seek Outside ist in Speedrun/Practice, Seed Map, Seed-Suche, HUD-Modusnamen und Replay integriert. Bestehende Level-Indizes bleiben unverändert.
- Run review heißt Replay. Abgeschlossene und abgebrochene Versuche lassen sich speichern, umbenennen, kategorisieren und mit Notizen beliebig wiedergeben. Die Liste zeigt Status, Modus, Zeit und Seed; Filter und getrennte gespeicherte Kopien erleichtern das Wiederfinden.
- Spielerroute und Vorschlagsroute bleiben unabhängig animiert. Monsterpositionen, dünn gestrichelte geplante Wege und ausblendende weiße Licht-/Kamera-Ringe ergänzen neue Aufnahmen. Alte Aufnahmen bleiben lesbar.
- Zeitprofile nutzen beobachtete regionale Geschwindigkeiten, Kurven, Anlaufphasen und Einsammelpausen. Die gemessene Run-Zeit wird rekonstruiert. Vorschlagsroute und Seed Map verwenden passende Profile; unzureichende Daten fallen sichtbar auf 8,1–10 u/s zurück.
- Begrenzte Aufzeichnungsarbeit, geprüfte lineare Monster-Schlüsselbilder und Hintergrund-IO schonen Spiel und GUI. Format 8 schützt Dateien gegen unbemerkte Beschädigung; lokale Validierung bleibt am Run und kann im Editor nicht umgeschaltet werden.

DLL und EXE gemeinsam ersetzen. Neue Monster-/Lichtereignisse werden ab dieser Version aufgenommen. Beide Hide-&-Seek-Level einmal neu laden. Einzelheiten und Live-Test-Grenzen: README.md und PRUEFUNG.md.
