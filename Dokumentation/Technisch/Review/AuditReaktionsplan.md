# Audit-Reaktionsplan Firmware 13v00

Quelle: [2026-08-13-Audit-Firmware-13v00.md](2026-08-13-Audit-Firmware-13v00.md)
Zweck: die dortige „Empfohlene Reihenfolge" als abarbeitbare Checkliste mit Status.

## Status-Legende

`[ ]` offen · `[~]` in Arbeit · `[x]` erledigt · `[-]` zurückgestellt

## Nicht antasten (Gesamtbild-Fazit des Audits)

Diese fünf Mechanismen funktionieren bereits korrekt und dürfen bei der Umsetzung nicht wegoptimiert werden:

- kein `vTaskDelay()` unter gehaltenem Mutex (Projektregel wird eingehalten)
- Zeitschnappschuss per `localtime_r()` je Task
- `rtosPanic()` bei halb gestartetem Systemzustand
- `ALARM_MAX_RESTARTS` (12v10) / `SERIAL2_DRAIN_MAX_BYTES` (12v11) als Deckel gegen Endlosschleifen
- TWDT-Init vor Task-Start (12v15/12v16)
- ACK-Modus (`player.begin(…, true, true)`) – Deaktivierung war bereits in 13v00 eine Regression

## Umsetzungsreihenfolge

- [ ] **1. A5 + A1 gemeinsam** – `timeValid()`-Gate + tagesbezogene, pegelbasierte Alarmfälligkeit
  statt `sec == 0`-Flanke. Höchste Wirkung, kein Kontakt zur DFPlayer-Logik. Gate muss zuerst
  da sein (sonst löst die Pegelprüfung auf der 1970-Uhr sofort nach Boot aus). Erledigt A1, A4,
  A5, A6 in einem Zug, macht A2 selbstheilend. `ALARM_CATCHUP_MIN` laut Review-Notiz auf ≥60
  setzen oder DST-Vorwärtslücke separat behandeln; `lastA1Min`/`lastA2Min`-Nutzung in `inputTask`
  (S1-Handler) und `lastCuckooMin` im selben Zug auf Tages-Logik umstellen (überschneidet sich
  mit C1, nicht getrennt umsetzen). Test ohne Hardware: Systemzeit per NTP-Attrappe springen lassen.
- [ ] **2. A3** – Maximallaufzeit für `ALARM_RUNNING` (`ALARM_MAX_RUN_MS`, eigener Zeitstempel
  `alarmRunStart`). Zusammen mit **C1** (S1 wirkt bei `ALARM_RUNNING` unabhängig vom Playerstatus,
  inkl. drittem Zweig „Status unbekannt bei `ALARM_IDLE`" laut Review-Notiz). Reine Anwendungslogik,
  keine Bibliotheksberührung.
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
