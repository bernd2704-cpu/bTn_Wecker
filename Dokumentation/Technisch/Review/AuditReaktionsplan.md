# Audit-Reaktionsplan Firmware 13v00

Quelle: [2026-08-13-Audit-Firmware-13v00.md](2026-08-13-Audit-Firmware-13v00.md)
Zweck: die dortige „Empfohlene Reihenfolge" als abarbeitbare Checkliste mit Status.

## Status-Legende

`[ ]` offen · `[~]` in Arbeit · `[x]` erledigt · `[-]` zurückgestellt

## Zwischenstand 2026-08-18 (Gültigkeitsprüfung gegen 20v03)

Firmware ist ohne Bezug auf diesen Plan direkt von der Audit-Basis 13v00 auf 20v00
(Hardware 2v0, DFPlayer-BUSY-Signal GPIO34) gesprungen. Die 20v00–20v03-Änderungen
dienten der Hardware-Anbindung, haben aber als Nebeneffekt drei Befunde berührt:

- **C1 erledigt** – S1-Handler nutzt bei UART-Timeout jetzt `dfPlayerBusy()` statt
  blind `st = 0` zu setzen (`Wecker_20v03.ino:1752-1755`, seit 20v02). Laufender
  Alarm/Sound wird zuverlässig gestoppt statt fälschlich Kuckuck auszulösen.
- **A3 teilweise entschärft** – `runAlarmMachine()`/`ALARM_RUNNING` beendet den Alarm
  jetzt auch bei dauerhaftem `st == -1`, wenn `dfPlayerIdleDebounced()` das über den
  BUSY-Pin bestätigt (Z. 1474-1475, seit 20v01/20v03). Deckt den Audit-Hauptfall
  (Modul tot/TX-Leitung lose) ab. Der harte Zeit-Deckel `ALARM_MAX_RUN_MS` aus dem
  Fix-Vorschlag fehlt weiterhin – bleibt BUSY selbst dauerhaft LOW hängen, entwaffnet
  sich das Gerät wie beschrieben. Schritt 2 unten daher nur noch mit reduziertem Risiko,
  nicht als erledigt zu werten.
- **C3 nur am Rand berührt** – `verifyPlayStarted()` nutzt BUSY zusätzlich (Z. 1354),
  das hilft aber nur gegen „läuft eigentlich, UART lügt". Der Audit-Kernfall (Datei
  fehlt auf SD, Modul meldet `st == 0`, BUSY bestätigt korrekt „nicht busy" → trotzdem
  als Absturz behandelt, 2 Reboots, danach still) besteht unverändert. Schritt 6 bleibt offen.

Alle übrigen Befunde (A1, A2, A4–A6, B1, B2, C2, C4–C8, D1, D2, E1, E2, E4, E5) wurden
1:1 gegen `Wecker_20v03.ino`/`SysConf_20v03.h` verifiziert und sind unverändert gültig.

## Nicht antasten (Gesamtbild-Fazit des Audits)

Diese fünf Mechanismen funktionieren bereits korrekt und dürfen bei der Umsetzung nicht wegoptimiert werden:

- kein `vTaskDelay()` unter gehaltenem Mutex (Projektregel wird eingehalten)
- Zeitschnappschuss per `localtime_r()` je Task
- `rtosPanic()` bei halb gestartetem Systemzustand
- `ALARM_MAX_RESTARTS` (12v10) / `SERIAL2_DRAIN_MAX_BYTES` (12v11) als Deckel gegen Endlosschleifen
- TWDT-Init vor Task-Start (12v15/12v16)
- ACK-Modus (`player.begin(…, true, true)`) – Deaktivierung war bereits in 13v00 eine Regression

## Umsetzungsreihenfolge

- [x] **1. A5 + A1 gemeinsam** – Umgesetzt in **20v04** (`Wecker_20v04.ino`/`SysConf_20v04.h`).
  `timeValid()`-Gate vor jeder Alarm-/Kuckuck-Auswertung in `alarmTask` (A5); `alarmDue()` ersetzt
  die `sec == 0`-Flanke durch eine tagesbezogene, pegelbasierte Prüfung mit Nachholfenster
  `ALARM_CATCHUP_MIN = 60` (SysConf) – wie in der Review-Notiz gefordert auf 60 statt der
  ursprünglich vorgeschlagenen 15 gesetzt, deckt damit auch die März-DST-Lücke ab (A1). Die
  Fälligkeitsprüfung läuft weiterhin nur in `case ALARM_IDLE`, holt A2 aber automatisch nach, sobald
  `ALARM_RUNNING` endet, da die Tages-Sperre (nicht die Sekunde) das Kriterium ist (A4). Die
  Oktober-Doppelauslösung ist durch `lastCuckooDay`/`lastCuckooHour` statt `lastCuckooMin`
  ausgeschlossen (A6). `lastA1Min`/`lastA2Min` → `lastA1Day`/`lastA2Day`, ebenso in `inputTask`
  (S1-Handler) umgestellt – dort wurde die bisherige Sperren-Rücksetzung nach manuellem Stopp
  **entfernt** (offene Semantikfrage aus der Review-Notiz entschieden: Tages-Sperre bleibt nach
  manuellem Stopp bestehen, sonst würde die pegelbasierte Prüfung im nächsten 500-ms-Tick sofort
  wieder auslösen). `triggerAlarm()` setzt die Tages-Sperre über einen eigenen Zeitstempel erst nach
  erfolgreichem Start – macht A2 wie vorgesehen selbstheilend, ohne zusätzlichen Codepfad.
  Alte Version als `Software/Firmware_Versionshistorie/Wecker_20v03/` archiviert.
  **Noch nicht getestet:** Systemzeit-Sprung-Test (NTP-Attrappe), realer Alarm, DST-Übergänge.
- [x] **2. A3** – Umgesetzt in **20v05** (`Wecker_20v05.ino`/`SysConf_20v05.h`). Neue Konstante
  `ALARM_MAX_RUN_MS` (15 min, SysConf) und eigener Zeitstempel `alarmRunStart` (im Gegensatz zu
  `t_start6`, das als Poll-Timer bei jedem Poll zurückgesetzt wird) begrenzen `ALARM_RUNNING` hart.
  Deckt den Restfall aus der A3-Notiz ab: BUSY-Pin hängt selbst dauerhaft LOW,
  `dfPlayerIdleDebounced()` meldet nie „idle". Bei Auslösung durch die Obergrenze (nicht durch
  reguläres Alarmende) schreibt `runAlarmMachine()` einen `[FEHLER]`-Eintrag mit Laufzeit und
  letztem `playerStatus` ins Web-Log.
  **[x] C1 seit 20v02 erledigt** (`dfPlayerBusy()` statt blindem `st=0` im S1-Handler, Z. 1752-1755).
  Damit sind sowohl C1 als auch A3 vollständig abgedeckt.
  **Noch nicht getestet:** realer Alarm mit simuliertem BUSY-Hänger (Pin dauerhaft LOW erzwingen),
  Web-Log-Eintrag im Fehlerfall.
- [x] **3. B2** – Umgesetzt in **20v06** (`Wecker_20v06.ino`/`SysConf_20v06.h`). `triggerAlarm()`
  setzt den RTC-Merker jetzt direkt nach dem `playerMutex`-Take, vor `playFolder()`/
  `verifyPlayStarted()`, statt erst im bestätigten Fehlerpfad. `runAlarmMachine()` löscht ihn an
  zwei Stellen: sobald der erste Poll `playerStatus > 0` bestätigt (frühestmöglich), zusätzlich
  beim Alarmende (`mp3Finished`/`runTimeExceeded`) als Sicherheitsnetz für einen sehr kurzen Sound,
  der schon vor dem ersten Poll beendet war – ohne diese zweite Stelle bliebe der Merker sonst über
  das Alarmende hinaus gesetzt. Der watchdogTask-Freeze-Fallback (12v12) bleibt unverändert
  bestehen, ist jetzt aber redundant (Sicherheitsnetz für den Fall eines TWDT-Panics vor der
  30-s-Schwelle). Nach Schritt 1 (A1) umgesetzt wie gefordert – der Tages-Guard verhindert den in
  der Review-Notiz beschriebenen verspäteten Zweitalarm.
  **Noch nicht getestet:** gezielter Reset zwischen `playFolder()` und erstem Poll (z.B. `EN`-Taster
  während `ALARM_POLL_MS`-Fenster), Kontrolle dass `setup()` den Alarm danach korrekt nachholt.
- [x] **4. C5 + D1** – Umgesetzt in **20v07** (`Wecker_20v07.ino`/`SysConf_20v07.h`). Clamp von
  `sound1_assigned`/`sound2_assigned` läuft nur noch bei `mp3Count > 0`, sonst `[FEHLER]`-Log statt
  stillem Überschreiben mit 1 (C5). `data.begin()`-Rückgabewert an allen drei Stellen geprüft:
  `setup()` (NVR-Laden), `nvrTask()`, `bumpResetCount()` (D1). `nvrTask()` ruft bei Fehlschlag
  `markSafeChange()` statt den Commit stillschweigend zu verwerfen – nächster Versuch nach
  `NVR_COMMIT_DELAY_MS`. `readNVR()` läuft jetzt unabhängig vom `state`-Flag (die `getX()`-
  Fallbacks liefern beim ersten Boot ohnehin die Compile-Defaults). `webLogMutex`-Initialisierung
  vor das NVR-Laden gezogen, damit ein `data.begin()`-Fehlschlag dort sofort geloggt werden kann.
  **Noch nicht getestet:** `data.begin()`-Fehlschlag lässt sich ohne beschädigten NVS/Flash-Fehler
  kaum real provozieren – Codepfad nur durch Lesen verifiziert, nicht auf Hardware ausgelöst.
- [x] **5. B1 sekundär** – Umgesetzt in **20v08** (`Wecker_20v08.ino`/`SysConf_20v08.h`), abweichend
  vom Audit-Vorschlag. Wie von der Review-Notiz verlangt wurde der reale Worst-Case vorher
  nachgerechnet statt der Schätzwert übernommen: `verifyPlayStarted()` kann bei einem dauerhaft
  nicht antwortenden DFPlayer bis zu ~5,5–6 s blockieren (3× `VERIFY_PLAY_RETRIES`, je
  `VERIFY_PLAY_DELAY_MS` + Mutex-Take bis 200 ms + `readStateDrained()`-Gnadenfrist bei UART-
  Rauschen) – deckt sich mit der im Audit selbst hergeleiteten Summe ("Nicht bestätigt"-Abschnitt).
  Bei den vorgeschlagenen 6000/1000 wäre die Marge dazu praktisch null gewesen: ein nur
  ungewöhnlich langsamer, aber regulärer Alarmversuch hätte den Watchdog fälschlich als Freeze
  werten können – genau das „Fehlalarme erzeugen", vor dem die Notiz warnt. Stattdessen
  `WDG_TIMEOUT_MS`=10000/`WDG_CHECK_MS`=1000 gesetzt: Erkennung spätestens nach ~11 s, ca. 5 s
  Marge über dem berechneten Worst-Case, weiterhin ca. 4 s Marge unter den 15 s des TWDT (Software-
  Watchdog gewinnt jetzt wieder zuerst). Rechnung als Kommentar direkt bei den Konstanten in
  SysConf dokumentiert.
  **Noch nicht getestet:** Feldverhalten mit den neuen Schwellwerten (wie ursprünglich vom Audit
  vorgesehen) – insbesondere ob 10 s/1 s im Alltag Fehlalarme erzeugt oder echte Freezes zuverlässig
  erkennt, lässt sich nur auf der realen Hardware über längere Zeit beobachten.
- [x] **6. C3** – Umgesetzt in **20v09** (`Wecker_20v09.ino`/`SysConf_20v09.h`). `verifyPlayStarted()`
  liefert jetzt `PLAY_OK`/`PLAY_NO_SOUND`/`PLAY_CRASHED` statt `bool` – nur wenn nie eine Antwort
  ankam (`PLAY_CRASHED`), löst `triggerAlarm()` noch `ESP.restart()` aus. Kam mindestens einmal
  `st==0` an (`PLAY_NO_SOUND`), läuft der Alarm in denselben Erfolgspfad wie `PLAY_OK` (Motor/Licht
  an), statt zwei sinnlose Reboots auszulösen und danach still zu bleiben.
  **Zusammen mit A3 verifiziert** wie gefordert: neues Flag `alarmSilentFallback` hält
  `ALARM_RUNNING` bei `PLAY_NO_SOUND` am Laufen, obwohl `playerStatus` dauerhaft `0` meldet – ohne
  dieses Gate hätte `mp3Finished` den Alarm schon beim ersten Poll (5 s) sofort wieder beendet.
  Einziger Ausstieg dann `ALARM_MAX_RUN_MS` (A3) oder S1.
  **Zusätzlich beim Umsetzen entdeckt und mitbehoben:** der S1-Handler entschied bisher
  ausschließlich anhand des (möglicherweise stillen) Playerstatus, ob gestoppt oder Kuckuck
  ausgelöst wird – ein `alarmSilentFallback`-Alarm (playerStatus==0, Motor/Licht laufen aber
  tatsächlich) wäre von S1 NICHT gestoppt worden, sondern hätte versehentlich den Kuckuck
  ausgelöst. Jetzt `if (alarmState == ALARM_RUNNING || st > 0)` – genau die im ursprünglichen
  C1-Fix-Vorschlag des Audits genannte, aber in 20v02 nicht übernommene Bedingung.
  `readStateDrained()` wertet `DFPlayerError`-Frames vor dem Verwerfen aus und loggt den
  Fehlercode – macht „Datei fehlt" von „Modul abgestürzt" im Log erstmals unterscheidbar.
  „Optional auf Datei 1 zurückfallen" aus dem Audit-Fix-Vorschlag bewusst NICHT umgesetzt – hätte
  eine weitere blockierende playFolder()/verify-Runde eingeführt, genau zu der Zeit, in der Schritt 5
  die Watchdog-Marge knapp kalkuliert hat.
  **Noch nicht getestet:** realer Alarm mit absichtlich fehlender Sounddatei (auf Datei-Nummer
  zeigen, die nicht auf der SD-Karte existiert) – prüfen, dass Motor/Licht bis `ALARM_MAX_RUN_MS`
  laufen und S1 den Alarm zuverlässig stoppt.
- [x] **7. C4** – Umgesetzt in **20v10** (`Wecker_20v10.ino`/`SysConf_20v10.h`).
  `player.readFileCountsInFolder(1)` (0x4E) statt `player.readFileCounts()` (0x48) `- 1` – liefert
  die Dateizahl direkt für Ordner 01, kein Rechnen/Raten mehr über die Gesamtzahl aller Ordner.
  Bei Timeout bleibt `mp3Count` auf 0 statt auf den geratenen Fallback 99 – C5 (20v07) sperrt die
  Sound-Auswahl dann korrekt. UI-Auswahl war bereits vorher implizit gesperrt, solange
  `mp3Count == 0` (9v13-Fallback in den Touch-Handlern), keine Änderung dort nötig.
  **Noch nicht getestet:** neuer Bibliotheksaufruf (0x4E) auf realer Hardware – wie vom Reaktionsplan
  vorgesehen einzeln testen (mp3Count-Anzeige nach Boot mit bekannter SD-Karten-Dateizahl in
  Ordner 01 vergleichen).
- [x] **8. C2** – Umgesetzt in **20v11** (`Wecker_20v11.ino`/`SysConf_20v11.h`), zuletzt und
  ausdrücklich isoliert wie gefordert. Vor der Änderung Rollback-Punkt als Git-Tag `vor-C2-20v10`
  auf dem Stand nach Schritt 7 gesetzt (statt der ursprünglich vorgesehenen „Schritte 1–3" – zum
  Zeitpunkt der Umsetzung waren bereits alle Schritte 1–7 erledigt, der Tag markiert den
  tatsächlich unmittelbar vorherigen stabilen Stand). Gemeinsame fortschrittsbasierte Drain-
  Schleife `drainSerial2Progress()` ersetzt sowohl den Rückgabewert-Abbruch in `drainSerial2Pre()`
  als auch das Fehlen eines Vorab-Drains in `readStateDrained()` (jetzt VOR der eigenen
  `player.readState()`-Abfrage aufgerufen). Endlosschleife laut Review-Notiz ausgeschlossen: der
  äußere Schleifenkopf verlangt bereits `Serial2.available() >= DFPLAYER_RECEIVED_LENGTH`,
  `SERIAL2_DRAIN_MAX_BYTES` bleibt zusätzlich als harte Grenze.
  **Noch nicht getestet – das im Audit vorgesehene Testprotokoll steht komplett aus:** Kaltstart,
  Startsound, Sound-Vorschau beider Alarme, Lautstärkeänderung, echter Alarm, S1-Stopp, Alarm mit
  gezogener SD-Karte – jeweils mit offenem Web-Log, Blick auf „Serial2 Restbytes". Dieser Bereich
  hat bereits dreimal (12v09–12v14) Regressionen erzeugt – vor dem Einsatz als Wecker unbedingt
  auf echter Hardware durchspielen; bei Auffälligkeiten steht `git checkout vor-C2-20v10` als
  Rückfallebene bereit.
- [ ] **9. E5** – Rail am Motor+ mit Multimeter messen (Duty 100 %), danach **eine** Quelle
  korrigieren (SysConf-Kommentar oder Hardware-Notiz auf 3,3 V, `MOTOR_PWM_DUTY`/
  `MOTOR_PWM_KICK_THRESHOLD` ggf. anpassen). Nebenbei Diodentyp-Widerspruch `1N4148`/`1N4448`
  zwischen SysConf und Schaltplan/Stückliste klären. Unabhängig von allem anderen, jederzeit machbar.
- [ ] **10. Kleinere Punkte** – Wirkung geringer oder Hardwareeingriff nötig:
  - [ ] E1: max. Zusammenhangsdauer für `TS_PRESSED`/`TS_REPEAT` (Touch-Rekalibrierung) +
    `player.volume(vol)` in `triggerAlarm()` vor `playFolder()` senden
  - [ ] E2: S3 von GPIO0 (Boot-Strapping-Pin) auf unkritischen GPIO verlegen – nur bei ohnehin
    anstehender Platinenänderung
  - [ ] C6: Abbruch-Flag `alarmCancelRequested`, damit S1/Vorschau-Stopp während
    `verifyPlayStarted()` keinen unnötigen Reboot auslöst
  - [ ] C7: Lautstärke-/Sound-Auswahl erst nach erfolgreichem Mutex-Take übernehmen (Anzeige,
    NVS), Fehlschlag loggen
  - [ ] D2: Web-Log-Seite streamen statt `String`-Aufbau, oder `reserve()` auf 12288 anheben
  - [ ] E4: Bestätigungsabfrage für T3 (WLAN-Zugangsdaten löschen), zweiter Druck nötig
- [ ] **11. Zweizeiler aus „Ungeprüft geblieben"** – sobald ohnehin an `setup()` gearbeitet wird:
  - [ ] Rückgabewerte von `esp_task_wdt_init()`/`reconfigure()`/`esp_task_wdt_add(NULL)` auswerten,
    bevor „[TWDT] Hardware Watchdog aktiv" geloggt wird
  - [ ] `webLog("[NTP] Synchronisation OK")` und `snapTimeStr(snapNtpTime, …)` nur bei
    tatsächlichem NTP-Erfolg ausführen, nicht unbedingt nach Timeout

## Bewusst nicht umgesetzt

- **DFPlayer-Library-Patch** (`sendStack()`/`available()`): Audit hat die vermutete Ursache
  eines 15-s-TWDT-Hängers widerlegt (rechnerisch max. ~5,5–6 s). 12v14-Rücknahme des Patches war
  richtig, kein erneuter Eingriff in `DFRobotDFPlayerMini.cpp`.
- **E3** (Kuckuck-Treiber ohne Freilaufdiode): Energie im Abschaltfall liegt laut Rechnung
  Größenordnungen unter der MOSFET-EAS-Grenze bei ~20 Schaltvorgängen/Tag. Diode als gute Praxis
  optional, nicht dringend.

## Offene Prüfpunkte (im Audit selbst ungeprüft)

Aus Kapazitätsgründen nicht gegengeprüft, bei Gelegenheit einordnen:

- `STACK_WATCHDOG = 1344` im Fehlerzweig (webLogf + vsnprintf + 3× SSD1306) – Stack-Reserve fraglich
- Race auf `motorRunning`/E2 zwischen Kickstart und `/motor`-HTTP-Handler
- ungeschützte Snapshot-Strings im HTTP-Handler
- `nvs_flash_erase()` beim Werksreset ohne Anhalten des `nvrTask`
- `WEB.h`/WiFi-Konfigurator: keine Eingabevalidierung der POST-Parameter geprüft
- UI-State-Machine (`uiDispatch`, `menu()`) nur dort gelesen, wo sie Player-/NVR-Zustand berührt
- Touch-Kalibrierung nicht auf Fehlauslösung durch Netzbrummen geprüft
- Heap-Fragmentierung über Wochen Laufzeit (WebServer/lwIP) nicht gemessen
