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
- [~] **2. A3** – Maximallaufzeit für `ALARM_RUNNING` (`ALARM_MAX_RUN_MS`, eigener Zeitstempel
  `alarmRunStart`). Zusammen mit **C1** (S1 wirkt bei `ALARM_RUNNING` unabhängig vom Playerstatus,
  inkl. drittem Zweig „Status unbekannt bei `ALARM_IDLE`" laut Review-Notiz). Reine Anwendungslogik,
  keine Bibliotheksberührung.
  **[x] C1 seit 20v02 erledigt** (`dfPlayerBusy()` statt blindem `st=0` im S1-Handler, Z. 1752-1755).
  **A3 seit 20v01/20v03 teilweise entschärft** (`dfPlayerIdleDebounced()` beendet `ALARM_RUNNING`
  auch bei totem UART, Z. 1474-1475) – der harte `ALARM_MAX_RUN_MS`-Deckel als Rückfallebene gegen
  dauerhaft LOW hängendes BUSY-Signal ist trotzdem noch offen, siehe Zwischenstand oben.
- [ ] **3. B2** – RTC-Merker (`rtcRetryMagic` u.a.) vor dem riskanten Abschnitt in `triggerAlarm()`
  setzen, erst nach erstem erfolgreichem Poll (`playerStatus > 0`) bzw. finalem Abbruch löschen.
  **Nicht vor Schritt 1** – ohne Nachholfenster kann ein spät im Alarm liegender Reboot sonst einen
  verspäteten Zweitalarm auslösen (siehe Review-Notiz zu B2).
- [ ] **4. C5 + D1** – Clamp von `sound1_assigned`/`sound2_assigned` nur bei `mp3Count > 0`;
  NVS-Rückgabewerte (`data.begin()`, `putBool`/`putInt`) prüfen, bei Fehlschlag `safeChange`
  erneut setzen + `[FEHLER]`-Log. Zwei kleine isolierte Änderungen, kein Regressionsrisiko.
- [ ] **5. B1 sekundär** – `WDG_TIMEOUT_MS` auf 6000, `WDG_CHECK_MS` auf 1000 (SysConf). Erst
  nach Schritt 3, damit sich am Feldverhalten zeigt, ob der neue Schwellwert Fehlalarme erzeugt.
  Vorher realen Worst-Case von `verifyPlayStarted()` + `readStateDrained()`-Gnadenfrist
  nachrechnen (Marge zum 6-7-s-Fenster ist knapp, siehe Review-Notiz).
- [ ] **6. C3** – `st == 0` (Modul lebt, gestoppt) von `st == -1` (keine Antwort) trennen; nur
  `-1` rechtfertigt Neustart. Bei wiederholtem `st == 0` Alarm trotzdem als laufend behandeln
  (Motor+Licht an). **Zusammen mit A3 verifizieren** – sonst läuft ein Wecker ohne Ton bis zum
  A3-Timeout, statt zeitnah in einen erkennbaren Fehlerzustand zu wechseln. Getrennt committen.
  BUSY-Zusatzkriterium in `verifyPlayStarted()` (seit 20v02, Z. 1354) deckt diesen Fall nicht ab –
  bei echtem `st==0` (Datei fehlt) meldet BUSY ebenfalls „nicht busy", Reboot-Kaskade bleibt. BUSY
  ist binär (spielt/spielt nicht) und liefert keine Fehlerursache – daher zusätzlich zur
  `st==0`/`st==-1`-Trennung: `readStateDrained()` den Frame-Typ vor dem Verwerfen auswerten und
  `DFPlayerError` (0x40) mit Fehlercode ins Web-Log schreiben (statt wie bisher stillschweigend
  verworfen, siehe Ursachengruppe-C-Intro im Audit). Erst diese UART-Error-Frame-Auswertung macht
  „Datei fehlt" von „Modul abgestürzt" im Log unterscheidbar – BUSY kann das nicht leisten.
- [ ] **7. C4** – `readFileCountsInFolder(1)` statt `readFileCounts() - 1`, kein 99-Fallback bei
  Timeout (`mp3Count` bei 0 lassen). Beseitigt eine Ursache von C3. Neuer Bibliotheksaufruf →
  nach C3, einzeln testen.
- [ ] **8. C2** – `drainSerial2Pre()` auf Fortschrittsprüfung statt Rückgabewert umstellen,
  `readStateDrained()` vor jedem `player.readState()` zusätzlich drainen. **Zuletzt und
  ausdrücklich isoliert** – dieser Bereich hat in 12v09–12v14 dreimal Regressionen erzeugt.
  Vorher Schritte 1–3 als stabilen, rückbaubaren Stand festhalten (Tag/Branch). Testprotokoll:
  Kaltstart, Startsound, Sound-Vorschau beider Alarme, Lautstärkeänderung, echter Alarm,
  S1-Stopp, Alarm mit gezogener SD-Karte – jeweils mit offenem Web-Log, Blick auf
  „Serial2 Restbytes".
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
