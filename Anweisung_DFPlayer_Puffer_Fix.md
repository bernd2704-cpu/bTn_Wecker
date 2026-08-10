# Anweisung für Claude Code: DFPlayer Mini Puffer-Desync beheben

**Projekt:** bTn_Wecker_Projekt (Version 10v05)
**Betroffene Hardware:** DFPlayer Mini an Serial2 (RX=16, TX=17)

## Kontext / Diagnosebefund

Logauswertung vom 10.08.2026 zeigt: Vor `readState()`-Aufrufen im ALARM_RUNNING-Poll liegt konstant ein Rückstand von 20 Byte (= 2 vollständige 10-Byte-Frames) im Serial2-Puffer, gelegentlich 30 Byte. `readState()` liest offenbar pro Aufruf nur einen Frame, während pro Poll-Zyklus mindestens ein neuer Frame nachkommt — der Rückstand wird nie abgebaut. Dadurch liest `verifyPlayStarted()` nach `triggerAlarm()` einen veralteten Frame statt der aktuellen Antwort auf den gerade gesendeten `playFolder`-Befehl, was zu Fehlschlägen führt ("kein Start-Status", Versuch 1/3 und 2/3). Die bestehende Retry-Logik (3 Versuche) hat dies bisher überdeckt, ist aber kein verlässlicher Fix — bei einem Rückstand von 3+ Frames würde auch der letzte Versuch fehlschlagen.

## Umzusetzende Schritte

Bitte in der angegebenen Reihenfolge umsetzen, nach jedem Schritt kurz anhalten zur Rückmeldung.

### Schritt 1: Puffer-Drain vor zeitkritischen Befehlen
In `triggerAlarm()` unmittelbar vor dem Aufruf von `playFolder()`: alle noch im Serial2-Puffer stehenden Bytes verwerfen (`while(Serial2.available()) Serial2.read();`). Ziel: `verifyPlayStarted()` sieht garantiert nur die Antwort auf den gerade gesendeten Befehl.

### Schritt 2: `readState()` vollständig drainieren lassen
`readState()` so anpassen, dass bei jedem Aufruf **alle** verfügbaren vollständigen 10-Byte-Frames aus dem Puffer gelesen werden, nicht nur einer. Von den gelesenen Frames nur den zuletzt empfangenen (neuesten) als aktuellen Status werten, ältere verwerfen. Ziel: struktureller Rückstand wird an der Wurzel beseitigt, nicht nur punktuell vor dem Alarm.

### Schritt 3: Ursache der Frame-Häufung prüfen
Prüfen, ob der DFPlayer bei aktivem ACK-Modus zusätzlich zur angeforderten Statusantwort automatisch einen Bestätigungs-Frame pro Befehl sendet, und ob dies zusammen mit dem regulären Poll-Request zum Überschuss von einem Frame pro Zyklus führt. Falls ja: entweder ACK-Modus deaktivieren oder Poll-Routine so anpassen, dass beide Antworten pro Zyklus konsequent abgeholt werden.

### Schritt 4: Verifikationslogging
Bestehendes `webLogf()`-Logging der Serial2-Restbytes vor `readState()` beibehalten. Nach Umsetzung der Schritte 1–3 soll der Wert dauerhaft bei 0–10 Byte (0–1 Frame) liegen, nicht mehr stabil bei 20.

## Randbedingungen

- Kein `vTaskDelay()` unter Mutex (Projektregel). Das reine Verwerfen von Bytes via `Serial2.read()` ist nicht blockierend und daher unkritisch, auch innerhalb eines gehaltenen Mutex.
- Retry-Logik in `triggerAlarm()`/`verifyPlayStarted()` als Sicherheitsnetz belassen — soll nach dem Fix im Normalfall nicht mehr greifen.
- Alle Änderungen über `webLogf()` protokollieren, kein `Serial.print()`.
- Stackgrößen unverändert lassen, sofern nicht explizit erforderlich (nur über `STACK_*`-Konstanten in SysConf, falls doch nötig).
