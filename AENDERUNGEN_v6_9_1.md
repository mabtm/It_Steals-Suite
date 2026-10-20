# PracticeSuite 6.9.1 – Hide & Seek

Dieses Update basiert auf der unveränderten Version 6.9.

## Korrektur

Die bisherige Entscheidung bevorzugte den rohen NavMesh, sobald er alle Spawn- und Kristallpunkte abdeckte. Das konnte in Hide & Seek bestätigte Linien durch physische Wände ergeben. Für diesen Modus ersetzt jetzt ein eigener Unity-Spielergraph diese Grundlage.

- Ein temporäres Spielernetz wird aus Boden und tatsächlichen Umgebungscollidern aufgebaut. Auch inaktive Objekte werden berücksichtigt; der Spieler-Kollisionsfilter entscheidet über relevante Hindernisse. Eine Auswahl allein anhand des Walls-Layers oder des Wall-Tags entfällt.
- Boxen behalten ihre Position, Orientierung und Skalierung; lesbare Meshes werden mit ihrer wirklichen Geometrie verwendet. Nicht lesbare oder konvexe Meshes erhalten konservative Grenzen, damit sie bei Unity nicht unbemerkt als Hindernis fehlen. Diese Ersatzgrenzen werden in der Diagnose gezählt.
- Radius, Höhe, Stufenhöhe und zulässige Neigung stammen aus dem CharacterController. Ein eigener Agententyp trennt die neue Navigation von den Gegnern. Das Tool ändert keine Collider, Objektaktivierung oder Gegner-Agenten.
- Unity baut das temporäre Netz asynchron mit höchstens einem Build-Worker. Nach der Wegerfassung werden die eigenen Ressourcen wieder entfernt. Die fertigen Wegdaten bleiben im Trainer; beim eigentlichen Seed-Bruteforce finden keine neuen Collider- oder Bake-Abfragen statt.
- Vor der Aufnahme eines vollständigen Weges wird jedes Segment gegen die Box-Hindernisse und die runde Spielerbreite geprüft. Ein ungültiger Weg hinterlässt keine Teilkanten. Gültige Abstände an Wandecken bleiben erhalten.
- Hide & Seek verwendet ausschließlich den ausdrücklich als geprüft erfassten Spielergraphen. Der vollständige rohe Gegner-NavMesh darf ihn nicht mehr ersetzen. Die übrigen Modi behalten das Routing aus 6.9.
- Fehlende Verbindungen bleiben ausdrücklich gekennzeichnete Schätzungen. Ohne Filter werden weiterhin alle Seeds berücksichtigt. Die CPU-Generatoren und die Worker-Schleife aus 6.9 bleiben unverändert.
- Diagnose unter Seed type & prediction details und im MelonLoader-Log: physische Boden-/Hindernisanzahl, inaktive Hindernisse, konservative Ersatzgrenzen, Spielermaße, Anker und geprüfte bzw. abgelehnte Abfragen.

## Installation

1. Spiel und Trainer schließen.
2. ItStealsHelper.dll im Mods-Ordner ersetzen und ausschließlich die mitgelieferte ItStealsTrainer.exe verwenden.
3. Seed Map öffnen, Hide & Seek einmal neu laden und die neue Kartenerfassung abwarten.

Version 6.9.1 verwendet Hauptprotokoll 13 und Kartenkanal 8, damit die Dateien nicht mit der verworfenen 6.10 vermischt werden. Map-Format 6 enthält eine eindeutige Navigationsherkunft. Alte Formate 1–4 bleiben lesbar; für bestätigte Hide-and-Seek-Wege ist die neue Erfassung nötig. Karten aus dem verworfenen Format 5 werden neu erfasst.

## Prüfung

DLL/EXE und automatisierte Regressionen sind erfolgreich gebaut bzw. ausgeführt. Enthalten sind der tatsächlich kodierte C#-Graph, eine unabhängige Wandabstandsprüfung und eine 22-Kristall-Suche mit 2.000 Seeds bei einem und sechs Workern. Einzelheiten und Aussagegrenzen stehen in PRUEFUNG.md.

Die native Unity-Bake-Erzeugung auf der konkreten Spielkarte konnte auf diesem Host nicht live geprüft werden. Konservative Ersatzgrenzen können schmale Öffnungen schließen; bei fehlender Navigation werden Schätzungen sichtbar gekennzeichnet. Die Route basiert auf dem erfassten Ausgangszustand und passt sich nicht automatisch an später zerstörte Wände an.
