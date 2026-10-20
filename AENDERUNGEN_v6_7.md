# PracticeSuite 6.7.0

- Die automatische Lastregelung interpretiert normale Paket-ACK-Wartezeit nicht mehr als CPU-Überlast. Start mit 1 ms, Anpassung an mehrere echte Frames, Erholung bei moderaten externen Änderungen statt dauerhaftem 0,25-ms-Minimum. Die begrenzte Queue stoppt weiterhin jede Überschreibung.
- Klarer Hinweis: Camera Bright (18) und Timer (19) verwenden in beiden Genauigkeitsmodi eine Heuristik; exakt gilt weiterhin nur bis 15 Kristalle.
- Gegner im Timer sind auch sitzend/ruhend rot. Zusätzliche Ringe unterscheiden spotted/angry.
- Bevorzugte Navigation aus Unitys eigenen vollständigen Pfadabfragen, Agent-Typ und bidirektional geprüften NavMesh-Wegstücken. Kein künstliches Überbrücken von Wänden, Löchern oder Stockwerken. Abfragen einmalig, schrittweise und ausschließlich im Game-Thread.
- Capture-Fortschritt mit automatischem Suchstart, F5-Neuerfassung, begrenzte Ressourcen, saubere Aufhebung wartender Zustände. Layout-Scans benötigen keinen fertigen Navigationsgraph und pausieren Capture-Arbeit.
- Distanzen aus dem Captured-Graph ohne temporäre Linien pro Paar; Suchthreads behalten Matrix-/DP-/Cache-Reuse, MST-Untergrenze und Kontext-/Abbruch-Gates.
- Laufend sichtbare Unerreichbar-Zähler und Navigationsabdeckung; Agent-/Anker-/Abfrage-/Kanten-Details auch im TSV. Keine falschen Routenkandidaten, wenn die Daten keinen vollständigen Weg liefern.
- Hauptprotokoll 10; Kartenformat 3 mit weiter lesbaren alten Formaten 1/2. Config 11 unverändert. DLL und EXE gemeinsam ersetzen und Timer neu laden.

Quellen, Windows-EXE/DLL und Regressionen enthalten. Reale Unity-Pfade und Seeds/s müssen im Spiel geprüft werden; Host-Tests werden nicht als Live-Abnahme ausgegeben.
