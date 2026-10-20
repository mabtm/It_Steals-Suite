# Prüfung 7.11.5 / mab

- Windows-Trainer: C++20, -O2, statisch gelinkt, -Wall -Wextra -Werror; Build ohne Warnungen.
- Vollständige produktive ImGui-Oberfläche: 168 Hauptansichts-, 216 HUD-, 128 Modus- und 288 Minimapfälle; vorhandene Replay-/Clip-/Finish-/Routenfälle bestanden.
- Inspector: Summary und fünf Detailansichten bei 480/1920 sowie 100/150 % DPI, Karte/Pin/Fullbright/Details, Property-IDs, Ebenenauswahl per Wheel/Tab, Collider-freie Ziele, Bestätigung mit tatsächlichem Klick-Token, Fehlversuche, Abbruch, Snapshots, Focus-ACK und Clean-/Replay-Sperren bestanden.
- Neue Style-Bedienung: reale ImGui-Buttons/Schalter, lokale Sound-/Effektabschaltung, Popup-Escape ohne gleichzeitiges Schließen der Karte, sichtbare Popup-Grenzen bei 480 × 360 / 150 % DPI bestanden.
- Präsentationsmodell: Konfigurationsmigration, Roundtrip, fehlerhafte Zahlen, Master-/lokales Mute, Run-Sperre, Fokus-Verwerfen, kurze verfallende Sound-Cues, PCM-Hüllkurven/Gain, Highlight nach neuer Detailveröffentlichung, Identitätswechsel und begrenzter Cache bestanden.
- Produktions-Audio-Worker mit kontrollierter WinMM-Schnittstelle: zusätzlicher Gain pro Ereignis, ungültige Gains, asynchrone Ausgabe, Fokus-/Mute-Abbruch, Buffer-Lebensdauer, Gerätefehler und Shutdown bestanden.
- Einstellungs-Worker und bestehendes Experience-Modell bestanden.
- Sämtliche C#-Quellen, Helper und Encoder mit 7.11.4 byteweise verglichen; unverändert. Ebenso die Routen-, Kontakt-, Seed-Such- und Timingquellen. Hauptprotokoll 29 und Inspector-Kanal 3 bleiben gleich.

Belege: tests/results-7.11.5. Abbildungen verwenden die echten produktiven ImGui-Zeichenlisten mit synthetischen QA-Objekten und Host-Schrift, keine laufende Unity-Szene.

Kein laufender Windows-/Unity-/Audiotreiber-Test und keine gemessene Spiel-FPS-Aussage. Der mitgelieferte Helper meldet weiterhin 7.11.4; Trainer 7.11.5 ist dazu absichtlich kompatibel.
