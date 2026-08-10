# Bibliotheken

Hinweis: Bibliotheken über den Arduino Library Manager installieren.
Siehe README.md im Root für Versionsangaben.

## DFRobotDFPlayerMini: kein Patch (Stand 12v14)

**Firmware 12v13 hatte hier fälschlich einen Patch dokumentiert** – nach dem
Hardware-Watchdog-Reset (TWDT) vom 10.08.2026 mitten in `triggerAlarm()` →
`playFolder()` → `sendStack()` wurde `available()`s innere
`while (_serial->available())`-Schleife als Ursache vermutet und dort eine
100-ms-Obergrenze ergänzt.

Der Patch wurde in 12v14 **zurückgenommen**: die Hardware wurde mit 12v08
als voll funktionsfähig verifiziert, dort existierte weder der Patch noch
war die Bibliothek verändert. Der tatsächliche Fehler steckte in der
Firmware, nicht in der Bibliothek – siehe `triggerAlarm()` in
`Wecker_12v14.ino`: ein roher `Serial2.read()`-Discard vor `playFolder()`
(seit 12v11) griff an der Bibliothek vorbei in deren Frame-Puffer und
desynchronisierte dabei `_receivedIndex`/`_isSending`. Das erzeugte das
anhaltende Byte-Durcheinander auf der UART, das dann in `available()` zum
15-Sekunden-Hänger führte. Fix: Drain in `triggerAlarm()` läuft jetzt über
`player.available()`/`player.read()` statt über rohe Bytes (analog zu
`readStateDrained()`).

**DFRobotDFPlayerMini.cpp bleibt daher unverändert im Originalzustand.**
Kein Patch nach (Neu-)Installation nötig.
