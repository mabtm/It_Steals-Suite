# Änderungen in 6.0.1

Korrekturpaket auf Grundlage der ausgelieferten V6. Der frühere Release wurde separat erhalten.

## Menüabbruch und Timer

Das Verlassen eines laufenden Levels über das Spielmenü speicherte bereits einen Interrupted-Run, ließ aber den Live-Timer weiterlaufen. Das gemeinsame Run-Ende stoppt nun immer den Timer und übernimmt die identische Endzeit aus dem Run-Datensatz. Ein Menüabbruch erscheint als ABORTED statt FINISHED; nur abgeschlossene Runs erhalten die positive Completion-Darstellung.

Der Wechsel ins Spielmenü wird über den tatsächlichen Scene-Lifecycle erkannt. Im untersuchten Spiel lädt Escape den TitleScreen. Ein Escape, das nur das Radar schließt, und das Öffnen des Trainer-Menüs verursachen keinen Menüabbruch. Doppel-Callbacks erzeugen keine zusätzlichen History-Einträge. Nach Rückkehr in einen spielbaren Level ist der Timer bereit für einen neuen Run. Bereits abgeschlossene Runs bleiben abgeschlossen.

## Minimap

Die V6 nahm irrtümlich playerBody.forward als Vorwärtsrichtung an. Der tatsächliche MouseLook.FixedUpdate-Code bewegt den Spieler entlang playerBody.right und seitlich entlang -playerBody.forward. Der zentrale Kartenwinkel berücksichtigt wieder den nötigen 90°-Versatz.

In Player-Up verlaufen Vorwärts- und Rechtsbewegung nach oben bzw. rechts. Der North-Up-Spielerpfeil verwendet dieselbe korrigierte Richtung. Interpolation, Caches, Filter, Zoom, Follow, Trail und Practice-Marker sind erhalten. Das Telemetrieformat und der exportierte Body-Yaw bleiben kompatibel.

## Genau zwei editierbare Anzeigen

Alle Preset-Schaltflächen und die Preset-API wurden entfernt. Es gibt nur das normale Compact HUD und das Speedrun-Overlay, jeweils mit eigenem Editor unter Settings → Overlay configuration. Sichtbare Felder, Reihenfolge, Größe, Position, Skalierung, Opazität und Abstände bleiben persistent. Der Practice-Tab verweist für Segmentwerte auf den Compact-HUD-Editor.

Vorhandene Konfigurationen werden übernommen; frühere Feldkombinationen bleiben individuelle Einstellungen. Es wird nichts zurückgesetzt. Normal/Practice verwendet Compact; reiner Speedrun verwendet ausschließlich das Speedrun-Overlay.

## Auslieferung

Neue Helper-DLL und Trainer-EXE gemeinsam installieren. Shared-Memory-Protokoll v5, Config- und Stats-Format 6 bleiben erhalten. Tests und aktuelle Prüfergebnisse stehen in tests/ und PRUEFUNG.md. Ein laufendes Windows-/Unity-Spiel steht in der Build-Umgebung nicht zur Verfügung.
