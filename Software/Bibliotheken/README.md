# Bibliotheken

Hinweis: Bibliotheken über den Arduino Library Manager installieren.
Siehe README.md im Root für Versionsangaben.

## Pflicht-Patch: DFRobotDFPlayerMini::available()

**Nach jeder (Neu-)Installation der Bibliothek DFRobotDFPlayerMini erneut
anwenden** – der Library-Manager überschreibt lokale Änderungen.

`DFRobotDFPlayerMini.cpp`, Funktion `available()`: die innere
`while (_serial->available())`-Schleife hat im Original keine
Zeitbegrenzung. Liefert der DFPlayer (z.B. bei getrenntem/defektem Modul,
floatende RX-Leitung) anhaltend Bytes/Rauschen, blockiert diese Schleife
unbegrenzt – `waitAvailable()`s eigener 500-ms-Timeout greift nicht, da er
nur zwischen Aufrufen von `available()` geprüft wird. Führte am 10.08.2026
zu einem Hardware-Watchdog-Reset (TWDT) mitten in `triggerAlarm()` →
`playFolder()` → `sendStack()`, ohne dass die Firmware (auch mit
`SERIAL2_DRAIN_MAX_BYTES`-Begrenzung, Firmware 12v11) reagieren konnte,
weil der Hänger bereits in der Bibliothek selbst passierte.

Fix: harte Obergrenze von 100 ms pro `available()`-Aufruf ergänzt
(`unsigned long _availableStartMs = millis();` vor der Schleife,
`if (millis() - _availableStartMs > 100) { break; }` als erste Anweisung
in der Schleife). `_receivedIndex` bleibt dabei für den nächsten Aufruf
erhalten – kein Datenverlust bei bereits vollständig empfangenen Frames.

Installationspfad auf diesem Rechner: `D:\Arduino\libraries\DFRobotDFPlayerMini\`
