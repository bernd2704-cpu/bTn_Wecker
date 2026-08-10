**bTn Wecker**

Änderungshistorie

Basis 4v1  →  12v13

## Kategorien

| Bugfix | Stabilität | Funktion | Refactoring | Qualität |
|---|---|---|---|---|

## Versionen 4v1 – 5v4  (Arduino-Basis)

| Version | Kategorie | Änderung |
|---|---|---|
| 4v2 | Bugfix | Touch T3/T4 Lautstärke vertauscht – korrigiert |
| 4v2 | Bugfix | Maximale Lautstärke auf 20 begrenzt (max_vol) |
| 4v2 | Bugfix | NVS-Lautstärke-Clamp beim Laden auf max_vol |
| 4v3 | Stabilität | Touch-Schwellwert: Fallback auf 80% der Baseline wenn Baseline < TOUCH_DROP |
| 4v4 | Bugfix | Alarm 2 auf else-if: Alarm 1 hat Vorrang bei gleicher Weckzeit |
| 4v5 | Bugfix | readFileCounts() Unterlauf-Fix: c==0 verhindert uint8_t-Überlauf auf 255 |
| 5v0 | Stabilität | readState()-Schleife: Timeout 200 ms verhindert Hänger bei DFPlayer-Ausfall |
| 5v1 | Qualität | Alle sprintf() durch snprintf() ersetzt |
| 5v2 | Funktion | Reset-Zähler im NVS; Werksreset via T4 auf Info-Seite |
| 5v2 | Funktion | Neue Funktion zeigeZ10R() für rechtsbündige OLED-Ausgabe |
| 5v3 | Qualität | Layout-Anpassungen Info-Seite |
| 5v4 | Funktion | Kuckuck unterdrückt wenn Alarm auf gleiche volle Stunde eingestellt |

## Versionen 6v0 – 7v2

| Version | Kategorie | Änderung |
|---|---|---|
| 6v0 | Funktion | Neue Menüseite 6: Kuckuck-Aktivzeit (Von/Bis-Stunde), NVS-gespeichert |
| 6v2 | Bugfix | Kuckuck-Zeitfenster: Mitternacht-Überlauf korrekt behandelt |
| 6v3 | Bugfix | lastCuckooMin nie zurückgesetzt – Kuckuck löste nur einmal aus |
| 6v4 | Stabilität | WiFi-Konfig- und Werksreset-Meldung unter displayMutex |
| 6v5 | Stabilität | wifiTask: lokaler tm-Snapshot via localtime_r() – kein Torn-Read |
| 6v6 | Qualität | zeigeZ-Parameter von String auf const char* – kein Heap-Overhead |
| 6v8 | Qualität | display.display() aus zeigeZ-Funktionen entfernt – zentraler Flush |
| 7v0 | Qualität | max_vol als constexpr MAX_VOL; varState lokal in setup() |
| 7v2 | Stabilität | alarmTask/runAlarmMachine/runCuckooMachine: eigener localtime_r()-Snapshot – Race Condition mit displayTask beseitigt |

## Versionen 7v3 – 7v9

| Version | Kategorie | Änderung |
|---|---|---|
| 7v3 | Stabilität | datum_WiFi / zeit_WiFi Double-Buffer – wifiTask schreibt in tmp-Puffer |
| 7v4 | Stabilität | readNVR(): alle Werte nach Laden geclampt |
| 7v5 | Qualität | goto wifi_skip durch bool wifiConnected ersetzt |
| 7v6 | Qualität | delay1–delay7 durch sprechende Konstantennamen ersetzt |
| 7v7 | Qualität | sound1/sound2 → sound1_selected/sound2_selected; sound1_play/sound2_play → sound1_assigned/sound2_assigned |
| 7v8 | Bugfix | setup(): display.display() an allen Stellen ergänzt – OLED blieb beim Start dunkel |
| 7v9 | Stabilität | Zwei-Stufen-Debouncing: ISR BTN_DEBOUNCE_MS=30ms, inputTask BTN_LOCKOUT_MS=1000ms |

## Versionen 8v0 – 8v2

| Version | Kategorie | Änderung |
|---|---|---|
| 8v0 | Bugfix | vTaskDelay(1ms) unter gehaltenem playerMutex → Freeze bei Alarm+Kuckuck 10:00 – Mutex vor Delay freigeben |
| 8v1 | Stabilität | alarmTask Core 1 → Core 0: physische Trennung von inputTask eliminiert CPU-Scheduling-Konflikte |
| 8v2 | Stabilität | Anwendungs-Watchdog (watchdogTask): überwacht inputTask/displayTask/alarmTask via Alive-Timestamps (30s/5s) |
| 8v2 | Bugfix | wdg_displayTask fehlte in displayTask – Timestamp vor Mutex-Versuch gesetzt |

## Versionen 9v0 – 9v2

| Version | Kategorie | Änderung |
|---|---|---|
| 9v0 | Stabilität | Hardware-TWDT (esp_task_wdt): inputTask, displayTask, alarmTask angemeldet; Timeout 15 s, trigger_panic=true |
| 9v0 | Qualität | Stack-Größen als STACK_*-Konstanten in SysConf; SystemConfig.h → SysConf_9v0.h mit Versionshistorie |
| 9v0 | Qualität | Stack-Größen angepasst: touchTask 3072, alarmTask 2048, inputTask 3072, displayTask 3072 |
| 9v1/9v2 | Funktion | Web-Logger: webLog()/webLogf() mit Thread-sicherem Ring-Puffer (Mutex-geschützt) |
| 9v1/9v2 | Funktion | webLogTask (Core 0, Pri 1): HTTP-Server Port 8080, GET / (HTML+Auto-Refresh), GET /log (plain text) |
| 9v1/9v2 | Funktion | Alle Serial.*-Ausgaben nach WiFi-Connect → webLog/webLogf (stackMon, watchdog, touch, wifi, setup) |
| 9v1/9v2 | Funktion | Letzte Serial-Ausgabe: IP-Adresse + Log-URL nach WiFi-Connect |
| 9v1/9v2 | Funktion | UI_INFO (Seite 7): Zeile 54 zeigt jetzt IP:Port des Web-Log-Servers statt SSID |
| 9v2 | Bugfix | webLogMutex fehlte in setup() → assert NULL pxQueue bei erstem Client-Zugriff – Mutex jetzt korrekt initialisiert |
| 9v2 | Bugfix | Lambda-Handler ohne Null-Prüfung auf webLogMutex – if(webLogMutex && ...) ergänzt |
| 9v2 | Qualität | WEBLOG_LINES 80 → 40 (Heap-Ersparnis ~5 KB) |
| 9v2 | Qualität | Auto-Refresh HTML 20 s → 10 s |
| 9v3 | Funktion | Web-Log-Seite: Touch-Baseline und Stack-HWM als dedizierte Snapshot-Sektionen – nur jeweils letzter Wert + Timestamp, kein Spam im Ring-Puffer |
| 9v4 | Bugfix | playerStatus == 0 statt < 1 in runAlarmMachine(): readState() gibt bei UART-Timeout -1 zurück – mit < 1 wurde fälschlich Alarm-Abbruch ausgelöst während webLogTask auf Serial2 zugreift |
| 9v4 | Qualität | SysConf bleibt bei 9v3-Stand – keine Konstanten-Änderungen in 9v4 |
| 9v4 | Bugfix | KRITISCH: vTaskDelay(1ms) unter playerMutex im S1-Handler (inputTask) – Mutex sofort freigeben, dann vTaskDelay(), dann neu nehmen (analog Fix 8v0 in alarmTask) |
| 9v4 | Bugfix | webLogMutex-Initialisierung vor ersten webLogf()-Aufrufen in setup() verschoben – setup()-Meldungen gingen bisher verloren |
| 9v4 | Qualität | webLogReady-Flag entfernt (toter Code – Deklaration und Zuweisung) |
| 9v4 | Qualität | WEB.h Footer-Versionsstring 4v0 → 9v4 korrigiert |
| 9v4 | Qualität | Web-Log-Seitentitel 9v3 → 9v4, TWDT-Kommentar 9v3 → 9v4 korrigiert |
| 9v5 | Bugfix | esp_task_wdt_init() → esp_task_wdt_reconfigure(): TWDT ist beim Arduino-Start bereits initialisiert – Doppel-Init vermieden, alle drei Parameter (timeout_ms, idle_core_mask=0, trigger_panic=true) werden korrekt übernommen |
| 9v5 | Qualität | Alle internen 9v4-Referenzen in .ino und SysConf ersetzt; WEB.h Footer-Version aktualisiert |
| 9v6 | Funktion | FW_VERSION-Makro eingeführt – Versionsnummer zentral in SysConf_9v6.h definiert, nicht mehr verteilt im Code |
| 9v6 | Funktion | Auto-Rückkehr zu UI_CLOCK jetzt auch von UI_INFO (Seite 7) – bisher war UI_INFO ausgenommen |
| 9v6 | Bugfix | Web-Log Versionsstring korrigiert; mp3Count-Clamp auf gültigen Bereich ergänzt |
| 9v6 | Qualität | PGMInfo und WEB.h Footer auf bTn_Alarm_9v6 aktualisiert; Kommentare korrigiert |
| 9v7 | Bugfix | EVT_S3 aktualisiert lastTouchMs – ohne Fix wurde nach S3 (Info-Toggle) die Auto-Rückkehr-Zeit nicht zurückgesetzt, Seite kehrte zu früh zurück |
| 9v8 | Qualität | bTn_Alarm_ → bTn_Wecker_ in PGMInfo, Kommentaren und WEB.h-Footer – konsistente Projektbezeichnung |
| 9v8 | Funktion | Web-Log: Reihenfolge der Sektionen – Allg. Log zuerst, dann Touch-Baseline, zuletzt Stack-HWM |
| 9v8 | Funktion | Web-Log: Allg. Log-Titel zeigt NTP-Sync-Zeitstempel nach Reset |
| 9v8 | Qualität | Web-Log: Schriftgröße 13 → 19 px; Zeilen nicht umbrechen (overflow-x: auto) |
| 9v8 | Qualität | Web-Log: Auto-Refresh 10 → 20 s |
| 9v9 | Qualität | Web-Log: Schriftgröße der Überschriften vergrößert (h2 → 1.6 rem, h3/.sec-title/.snap-title → 1 rem) |
| 9v10 | Bugfix | DFPlayer Start-Sound: readFileCounts() wird vor playFolder() aufgerufen – verhindert Abspielversuch bevor Dateianzahl bekannt ist |
| 9v11 | Qualität | Stack-Vorgaben reduziert: wifiTask/nvrTask/inputTask/displayTask 2560, watchdogTask 1536 – basierend auf gemessenen High-Water Marks |
| 9v12 | Bugfix | Bugfixes aus Code-Review (kritische Issues) |
| 9v13 | Bugfix | Mittlere Issues aus Code-Review behoben |
| 9v14 | Qualität | Wartungsqualität: Kosmetik aus Code-Review (Kommentare, Formatierung, Namensgebung) |
| 9v14 | Qualität | Versionsangaben in allen Dateien auf 9v14 synchronisiert |

## Versionen 9v15 – 10v06

| Version | Kategorie | Änderung |
|---|---|---|
| 9v15 | Qualität | UI: Checkboxen von 7×7 auf 8×8 vergrößert; Checked-Darstellung als Rahmen (drawRect 8×8) plus innere Füllung (fillRect 6×6) |
| 9v16 | Stabilität | DFPlayer-Kaltstart robuster: player.begin() in Retry-Schleife mit DFP_INIT_TIMEOUT_MS / DFP_INIT_RETRY_MS; SETUP_MP3_TIMEOUT_MS 5000 → 10000 ms |
| 9v17 | Qualität | UI: Checkbox-Rahmen von 1 px auf 2 px verdickt (zwei verschachtelte drawRect); alle Checkboxen um 1 px nach oben verschoben |
| 9v18 | Qualität | UI: Checkboxen auf 10×10 vergrößert (2 px Rahmen + 1 px Abstand + 4×4 Füllung); Checkboxen auf Seite Funktion um 2 px nach links verschoben |
| 10v00 | Funktion | Display-Abschaltung nach 10 min ohne Touch (DISPLAY_TIMEOUT_MS); Berührung eines beliebigen Touchpads weckt Display wieder für 10 min, das auslösende Touch-Event wird verworfen; Checkbox-Rahmen vereinfacht (nur äußerer 10×10 drawRect) |
| 10v01 | Qualität | DFPlayer-Init: 9v16-Retry-Logik zurückgenommen (player.begin() wieder einmaliger Aufruf); DFP_INIT_TIMEOUT_MS / DFP_INIT_RETRY_MS entfernt; SETUP_MP3_TIMEOUT_MS 10000 → 5000 ms |
| 10v02 | Qualität | Display-Ein-Zeit DISPLAY_TIMEOUT_MS von 10 min auf 5 min reduziert |
| 10v03 | Funktion | Display wird bei Alarm-Start automatisch eingeschaltet (analog Touch-Wake); Helper wakeDisplay() in alarmTask |
| 10v04 | Qualität | Web-Log: Zeile „[Reset] Anzahl: N" → „[RESET] resetCount: N" (einheitlicher Stil) |
| 10v05 | Stabilität | DFPlayer TX-Pin (GPIO17) vor Serial2.begin() 3 s LOW halten – verhindert Fehlinterpretation der ersten UART-Bytes beim Kaltstart |
| 10v06 | Bugfix | wakeDisplay(): TOCTOU und Race auf lastTouchMs behoben (displayBlanked-Check und lastTouchMs=millis() atomar unter displayMutex); lastTouchMs als volatile deklariert (Cross-Core-Sichtbarkeit) |

## Version 11v00

| Version | Kategorie | Änderung |
|---|---|---|
| 11v00 | Stabilität | NVR-Flash-Wear-Schutz: nvrSemaphore wird erst nach NVR_COMMIT_DELAY_MS (2 s) Ruhezeit ohne weiteres Event freigegeben; verhindert Flash-Writes bei gehaltener Einstelltaste im Touch-REPEAT-Modus |
| 11v00 | Bugfix | wifiTask Double-Buffer Race: snprintf in datum_WiFi_tmp/zeit_WiFi_tmp nur wenn kein altes Paar pending (wifiSyncPending-Guard); schließt Torn-Read-Fenster gegen displayTask |
| 11v00 | Qualität | Log-Regel: Serial.printf mit Web-Log-URL bleibt bewusst nach WiFi-Connect im Serial-Monitor – die URL steht sonst nur im Web-Log selbst und wäre unerreichbar |

## Version 11v01

| Version | Kategorie | Änderung |
|---|---|---|
| 11v01 | Funktion | Web-Log: neue Rubrik „Status – Letzter Start" zeigt die Zeilen WiFi und NTP analog zur Info-Seite (datum_WiFi/zeit_WiFi, datum_sync/zeit_sync); platziert oberhalb der Touch-Baseline-Sektion |

## Version 11v02

| Version | Kategorie | Änderung |
|---|---|---|
| 11v02 | Stabilität | inputTask: S3 wird bei `displayBlanked` zum reinen Wake+Discard-Event (analog T0–T4). Verhindert, dass ein blind gedrückter Taster auf der dunklen Info-Seite den Info-Toggle oder nachfolgende T0/T4-Touches (WLAN-Reset / Werksreset) versehentlich auslöst |
| 11v02 | Funktion | UI_INFO: WiFi-/NTP-Zeilen entfernt, stattdessen explizite Warnhinweise „T0: WLAN RESET SSID PW" und „T4: WERKSRESET" angezeigt – gefährliche Aktionen werden dem Nutzer direkt benannt |
| 11v02 | Funktion | Web-Log: Rubrik „Status – Letzter Start" umbenannt in „Verbindung – letzter WiFi Reconnect / NTP Sync" und unter die Ring-Puffer-Sektion verschoben |
| 11v02 | Qualität | UI_INFO-Zeile T0 von „WLAN RESET SSID PW" auf „RESET SSID PW" gekürzt (besserer Zeilenfit auf 128 px OLED) |

## Version 11v03

| Version | Kategorie | Änderung |
|---|---|---|
| 11v03 | Bugfix | resetCount zeigte nach Werksreset 2 statt 1: `bumpResetCount()` in `setup()` wurde vor `loadWifiCredentials()` aufgerufen – Konfigurator-Boot nach NVS-Erase zählte ebenfalls mit. Aufruf jetzt hinter `loadWifiCredentials()`; `bumpResetCount()` öffnet NVR-Namespace selbst (begin/end). |

## Version 11v04

| Version | Kategorie | Änderung |
|---|---|---|
| 11v04 | Funktion | S3 bei dunklem Display: Display einschalten UND Info-Seite öffnen (vorher seit 11v02 reines Wake+Discard). Implementiert in `inputTask` – Wake-Pfad für EVT_S3 setzt kein `continue`, Event fließt weiter zu `uiDispatch()`. Auto-Return (20 s) garantiert deterministisches Landen auf UI_INFO. Touch T0–T4 bleiben Wake+Discard (Reset-Schutz). |

## Version 11v05

| Version | Kategorie | Änderung |
|---|---|---|
| 11v05 | Funktion | UI_INFO: WLAN-Reset von T0 auf T3 verlegt – einheitliche Bedienung „Taste +" (T3) und „Taste –" (T4) analog zur Kuckuck-Seite. `onInfo()` reagiert jetzt auf EVT_T3 statt EVT_T0; EVT_T4 (Werksreset) unverändert. Der bestehende `s != UI_INFO`-Guard für T0 in `uiDispatch()` bleibt als Out-of-Bounds-Schutz für `cycle[]` (UI_INFO=7 wäre sonst außerhalb des 7-Einträge-Arrays) – T0 auf INFO ist seit 11v05 ohne Funktion. |
| 11v05 | Funktion | UI_INFO-Layout neu: Z1 Firmware-Kennung, Z2 Web-Log-Adresse (vorher Z5), Z3 MP3-/Reset-Zähler (vorher Z4), Z4 „Taste +  WiFi Reset", Z5 „ Taste –  Full Reset". Die früheren Hinweiszeilen „T0: RESET SSID PW" und „T4: WERKSRESET" (seit 11v02) entfallen zugunsten der symmetrischen +/–-Darstellung. |
| 11v05 | Qualität | Wake-Discard-Kommentar (`inputTask` bei `displayBlanked`) auf T3/T4 als gefährliche Aktionen angepasst. State-Machine-Diagramm, `runWifiConfigServer()`-Kommentar und `setup()`-Block-Kommentar zur WiFi-Konfig-Anforderung auf T3 nachgezogen. |

## Version 12v00

| Version | Kategorie | Änderung |
|---|---|---|
| 12v00 | Funktion | Hardware-Erweiterung „Motor + LED-Streifen": E2 (GPIO26, Wasserrad-Motor) wird statt digital jetzt per PWM über LEDC angesteuert – 20 kHz / 8 Bit / 60 % Duty (`MOTOR_PWM_DUTY`=153) treibt den 3-V-Motor an 5 V mit Mittelwert ~3 V ohne hörbares Schaltgeräusch. Ansteuerung in `alarmTask` und am S2-Zugschalter auf `ledcWrite(E2, …)` umgestellt. |
| 12v00 | Funktion | Neue zentrale Konstanten `MOTOR_PWM_FREQ` / `MOTOR_PWM_RES` / `MOTOR_PWM_DUTY` in SysConf (analog zu den `STACK_*`-Konstanten). |
| 12v00 | Refactoring | `pinMode(E2, OUTPUT)` in `setup()` entfällt – `ledcAttach()` übernimmt die Pinkonfiguration. E3 (GPIO27, LED-Streifen) bleibt `digitalWrite(HIGH/LOW)` (ohmsche Last mit 47 Ω Vorwiderstand, kein PWM erforderlich). |

## Version 12v01

| Version | Kategorie | Änderung |
|---|---|---|
| 12v01 | Qualität | Stack-Größen neu vorgegeben (Bytes): touchTask 2880, wifiTask 2000, nvrTask 2304, inputTask 2240, displayTask 2176, alarmTask 2128, watchdogTask 1344, stackMonTask 2912. webLogTask (4096) unverändert. Werte direkt als `STACK_*`-Konstanten in SysConf gesetzt; `setup()` übernimmt sie ohne weitere Codeänderung. |

## Version 12v02

| Version | Kategorie | Änderung |
|---|---|---|
| 12v02 | Funktion | Max. Einschaltzeit Licht/Mühlrad (Zugschalter S2) auf 30 min begrenzt – analog Auto-Rückkehr der Menü-Seiten. Neue Konstante `S2_TIMEOUT_MS` (1800000 ms). `displayTask` schaltet E2 (Motor-PWM) und E3 (Licht) nach Ablauf ab und setzt `S2_SW` zurück; Zeitstempel `t_start_S2` wird im S2-Handler beim Einschalten gesetzt. |
| 12v02 | Qualität | Web-Log „Allgemeines Log": `[xxx]`-Tag wird mit Leerzeichen auf feste Breite `WEBLOG_TAG_WIDTH` (12 Zeichen) aufgefüllt, damit der Text dahinter immer in derselben Spalte beginnt. |

## Version 12v03

| Version | Kategorie | Änderung |
|---|---|---|
| 12v03 | Funktion | Mühlrad-Motor-Pulsweite (Drehzahl) zur Laufzeit über Web-Slider verstellbar: Web-Log-Server erhält `GET /motor?duty=NN` (0–100 %) und einen Slider auf der Seite. `MOTOR_PWM_DUTY` ist nur noch der Default-Sollwert beim ersten Boot; der wirksame Wert liegt in der Laufzeit-Variable `motor_duty` (0–255) und wird in NVS persistiert (Schlüssel `motor_duty`). Live-Übernahme falls der Motor gerade läuft. |
| 12v03 | Funktion | Kickstart: liegt der Sollwert unter `MOTOR_PWM_KICK_THRESHOLD` (~35 % Duty), läuft der 3-V-Motor evtl. nicht aus dem Stand an → kurzer Vollgas-Anlaufimpuls (`MOTOR_PWM_KICK_DUTY` für `MOTOR_PWM_KICK_MS`), dann Sollwert. Neue zentrale Konstanten `MOTOR_PWM_KICK_THRESHOLD` / `_KICK_DUTY` / `_KICK_MS`. |
| 12v03 | Refactoring | Zentrale Helfer `motorStart()` / `motorStop()` ersetzen die verstreuten `ledcWrite(E2, …)`-Aufrufe in `runAlarmMachine`, am S2-Zugschalter und beim S2-Timeout. Persistenz über die bestehende `safeChange`→`nvrSemaphore`→`nvrTask`-Kette (kein Flash-Zugriff im HTTP-Handler). |

## Version 12v04

| Version | Kategorie | Änderung |
|---|---|---|
| 12v04 | Bugfix | Web-Log „Allgemeines Log – letzter Reset" zeigte nach einem Stromausfall kein Datum/keine Uhrzeit mehr (nur „–"), während der Reset-Zähler korrekt um 1 hochzählte. Ursache: Der Zeitstempel `snapNtpTime` wird nur in `setup()` nach erfolgreicher NTP-Synchronisation gesetzt; bootet der ESP32 nach Stromausfall schneller als der Router, scheitert die WLAN-Verbindung beim Start und der gesamte NTP-Block (inkl. Befüllung von `snapNtpTime`) wird übersprungen. `displayTask` trägt den Reset-Zeitstempel nun beim ersten NTP-Sync nach dem Boot nach (über `ntpSyncPending`) und rekonstruiert den tatsächlichen Reset-Zeitpunkt aus aktueller Zeit minus Uptime (`millis()`). Der Reset-Zähler war nie betroffen (NVS, unabhängig von WLAN/NTP). |

## Version 12v05

| Version | Kategorie | Änderung |
|---|---|---|
| 12v05 | Stabilität | DFPlayer Start-Check: Neue Funktion `verifyPlayStarted()` fragt direkt nach `player.playFolder()` (Alarm 1 + Alarm 2 in `runAlarmMachine`) per `readState()`-Doppel-Poll (analog `ALARM_RUNNING`) ab, ob der DFPlayer den Play-Befehl tatsächlich angenommen hat. Bisher wurde weder der ACK-Modus aus `player.begin()` noch der Player-Status nach dem Play-Befehl ausgewertet – ein nicht reagierender DFPlayer (Kabel ab, Modul defekt, SD-Karte fehlt) blieb unbemerkt, und der Alarm (Motor/Licht) lief mangels Zeitbegrenzung in `ALARM_RUNNING` potenziell unbegrenzt weiter. Bei `st<=0` (0=idle, -1=UART-Timeout) jetzt `webLogf()`-Fehlermeldung mit Alarm-Label und Dateinummer. |
| 12v05 | Stabilität | DFPlayer-Absturzerkennung: Bestätigt `verifyPlayStarted()` den Play-Befehl nicht, gilt der DFPlayer als abgestürzt – die neue Funktion `triggerAlarm()` (löst jetzt beide Alarme aus, ersetzt den bisherigen Inline-Code in `runAlarmMachine`) löst dann unmittelbar `ESP.restart()` aus (einzige verlässliche Wiederherstellung für ein hängendes DFPlayer/UART). |
| 12v05 | Stabilität | Verpasster-Alarm-Retry nach Absturz-Neustart: `triggerAlarm()` hinterlegt Alarmnummer, Dateinummer und Minute in `RTC_NOINIT_ATTR`-Variablen (`rtcRetryMagic`/`-Alarm`/`-FileNo`/`-Min`), bevor es `ESP.restart()` auslöst – diese überstehen den Software-Reset. `setup()` prüft `rtcRetryMagic` nach der DFPlayer-Initialisierung und ruft `triggerAlarm()` mit den gemerkten Werten erneut auf, sodass der durch den Absturz verpasste Alarm nach dem Neustart doch noch abgespielt wird. |

## Version 12v06

| Version | Kategorie | Änderung |
|---|---|---|
| 12v06 | Bugfix | DFPlayer Start-Check löste Fehlalarm-Neustarts aus, obwohl der DFPlayer korrekt spielte: `verifyPlayStarted()` fragte `readState()` bisher nur 1 ms nach `player.playFolder()` ab – der DFPlayer braucht nach dem Play-Befehl jedoch Zeit, um die Datei von der SD-Karte zu laden und den Status auf „playing" zu setzen, sodass die Abfrage noch den alten Zustand (`st<=0`) auslas und `ESP.restart()` fälschlich ausgelöst wurde. `verifyPlayStarted()` wartet jetzt vor jeder Abfrage `VERIFY_PLAY_DELAY_MS` (500 ms) und erlaubt bis zu `VERIFY_PLAY_RETRIES` (3) Versuche – bei anhaltendem `st<=0` steht das Ergebnis spätestens nach 1500 ms fest, bevor der Neustart folgt. |

## Version 12v07

| Version | Kategorie | Änderung |
|---|---|---|
| 12v07 | Stabilität | DFPlayer-Absturz-Neustart war unbegrenzt: Reagierte der DFPlayer dauerhaft nicht (z. B. Kabel ab, Modul defekt), löste `triggerAlarm()` bei jedem Fehlversuch erneut `ESP.restart()` aus – der ESP32 startete potenziell endlos neu. Neuer Parameter `failCount` in `triggerAlarm()` sowie `RTC_NOINIT_ATTR`-Zähler `rtcRetryCount` (übersteht `ESP.restart()` wie die bestehenden `rtcRetry*`-Variablen) zählen die Fehlversuche über die Neustarts hinweg mit. |
| 12v07 | Stabilität | Neue Konstante `ALARM_MAX_RESTARTS` (10) in SysConf begrenzt die Anzahl der Versuche: Ab dem 10. erfolglosen Versuch löst `triggerAlarm()` keinen weiteren `ESP.restart()` mehr aus, sondern bricht den Alarm für diesen Tag endgültig ab (Motor/Licht bleiben aus, `rtcRetryMagic` wird gelöscht). Am nächsten Tag wird der Alarm zur regulären Zeit wieder frisch versucht (`failCount` startet dann bei 0). |
| 12v07 | Funktion | Neuer Fehlereintrag im Web-Log beim endgültigen Abbruch: `webLogf()` schreibt eine `[FEHLER]`-Zeile mit Alarm-Label, Dateinummer, Versuchsanzahl und Datum/Uhrzeit (`snapTimeStr()`). Die Log-Seite (`webLogTask()`) färbt Zeilen mit `[FEHLER]`-Tag rot (analog `[WATCHDOG]`/`[PANIC]`). |

## Version 12v08

| Version | Kategorie | Änderung |
|---|---|---|
| 12v08 | Funktion | Web-Log neu organisiert: DFPlayer-Meldungen (alle Zeilen mit „DFPlayer" im Text) erscheinen jetzt in einem eigenen Abschnitt `#dflog` statt im „Allgemeinen Log". Titel lautet „DFPlayer – letzter erfolgreicher Alarm: *&lt;Zeitstempel&gt;*" – neuer Snapshot `snapAlarmTime` wird in `triggerAlarm()` direkt nach erfolgreichem `playFolder()` gesetzt (analog `snapTouchTime`/`snapStackTime`). |
| 12v08 | Funktion | Neue Funktion `checkSerial2Leftover(label)` prüft vor jedem an den DFPlayer gesendeten Kommando `Serial2.available()`. Sind noch Bytes im Empfangspuffer (Hinweis auf verspätete/verlorene Antworten bzw. UART-Desync), schreibt `webLogf()` eine `[DFPlayer]`-Zeile mit Kommando-Label, Byteanzahl, Zeitstempel und einem seit Boot mitlaufenden Zähler `serial2LeftoverCount`. Wird vor allen `player.*()`-Aufrufen aufgerufen, die ein Kommando an den DFPlayer senden. |

## Version 12v09

| Version | Kategorie | Änderung |
|---|---|---|
| 12v09 | Bugfix | DFPlayer Serial2-Puffer-Desync behoben: Logauswertung vom 10.08.2026 zeigte konstant 20 Byte (2 Frames) Rückstand vor `readState()`-Aufrufen im ALARM_RUNNING-Poll. Ursache: `DFRobotDFPlayerMini::available()` verarbeitet pro Aufruf nur einen vollständigen 10-Byte-Frame und kehrt danach sofort zurück, selbst wenn im Puffer bereits weitere vollständige Frames warten (z. B. das interne 0x41-ACK zusätzlich zur eigentlichen Feedback-Antwort im ACK-Modus). `verifyPlayStarted()` las dadurch nach `triggerAlarm()` teils einen veralteten Frame statt der aktuellen Antwort auf den gerade gesendeten `playFolder`-Befehl. |
| 12v09 | Bugfix | Neue Funktion `readStateDrained()` liest bei jedem Aufruf so lange weiter, bis kein vollständiger 10-Byte-Frame mehr im Puffer steht, und wertet nur den zuletzt empfangenen Feedback-Wert als aktuellen Status (ältere Frames werden verworfen). Ersetzt `player.readState()` an allen Aufrufstellen (`verifyPlayStarted()`, ALARM_RUNNING-Poll, S1-Handler). Beseitigt den Rückstand strukturell statt nur punktuell vor dem Alarm. |
| 12v09 | Stabilität | `triggerAlarm()` verwirft unmittelbar vor `playFolder()` alle noch im Serial2-Puffer stehenden Bytes (`while(Serial2.available()) Serial2.read();`) – `verifyPlayStarted()` sieht dadurch garantiert nur die Antwort auf den gerade gesendeten Befehl. Nicht blockierend, daher unkritisch auch unter gehaltenem `playerMutex`. |

## Version 12v10

| Version | Kategorie | Änderung |
|---|---|---|
| 12v10 | Funktion | `ALARM_MAX_RESTARTS` 10 → 3: reagiert der DFPlayer nach einem Alarm-Start dauerhaft nicht (Kabel ab, Modul defekt, SD-Karte fehlt), bricht `triggerAlarm()` jetzt schon nach 3 statt 10 `ESP.restart()`-Versuchen endgültig ab (Motor/Licht bleiben aus, roter `[FEHLER]`-Eintrag im Web-Log). |

## Version 12v11

| Version | Kategorie | Änderung |
|---|---|---|
| 12v11 | Bugfix | Reboot statt Alarm ohne Ersatzalarm behoben: Der in 12v09 eingeführte Puffer-Drain in `triggerAlarm()` (`while(Serial2.available()) Serial2.read();`) hatte keine Obergrenze. Bei getrenntem/defektem DFPlayer kann die floatende RX-Leitung (GPIO16) dauerhaft Rauschen liefern – `Serial2.available()` wird dann nie 0, die Schleife lief endlos und hielt dabei `playerMutex`. `alarmTask` aktualisierte dadurch `wdg_alarmTask` nicht mehr; `watchdogTask` erkannte nach `WDG_TIMEOUT_MS` (30 s) einen Task-Freeze und rief `ESP.restart()` auf – ohne `rtcRetryMagic` zu setzen, da dieser Pfad unabhängig vom regulären DFPlayer-Absturz-Neustart in `triggerAlarm()` ist. Ergebnis: Reboot, aber kein Ersatzalarm nach dem Neustart. |
| 12v11 | Bugfix | Neue Konstante `SERIAL2_DRAIN_MAX_BYTES` (200) begrenzt sowohl den Drain in `triggerAlarm()` als auch die Drain-Schleife in `readStateDrained()` (12v09) auf eine feste Obergrenze – beide Schleifen terminieren jetzt garantiert, auch bei dauerhaftem UART-Rauschen. |

## Version 12v12

| Version | Kategorie | Änderung |
|---|---|---|
| 12v12 | Stabilität | `watchdogTask` als zusätzlicher Fallback für `rtcRetryMagic`: Fror `alarmTask` während eines laufenden `triggerAlarm()`-Versuchs ein (egal aus welchem Grund), fand bisher kein Ersatzalarm-Retry nach dem `watchdogTask`-Neustart statt – dieser Pfad war komplett unabhängig vom regulären DFPlayer-Absturz-Retry in `triggerAlarm()` selbst. Neuer In-Flight-Marker `alarmTriggerInFlight` (+ Snapshot `alarmTriggerNum`/`-FileNo`/`-Min`/`-FailCount`) wird zu Beginn des riskanten Abschnitts in `triggerAlarm()` gesetzt und nach jedem Ausgang (Erfolg, regulärer Retry, endgültiger Abbruch) wieder gelöscht. |
| 12v12 | Stabilität | `watchdogTask` prüft bei einem `alarmTask`-Freeze gezielt diesen Marker (nicht bei input-/displayTask-Freezes, die stehen in keinem Zusammenhang mit einem laufenden Alarm) und setzt vor dem eigenen `ESP.restart()` ebenfalls `rtcRetryMagic`, sodass `setup()` den Alarm nach dem Neustart genauso erneut auslöst wie beim regulären Retry-Pfad. |

## Version 12v13

| Version | Kategorie | Änderung |
|---|---|---|
| 12v13 | Bugfix | Root Cause des 12v12-Vorfalls gefunden (Reboot ohne Ersatzalarm trotz 12v11/12v12-Fixes): Hardware-TWDT-Backtrace zeigte den Hang in `triggerAlarm()` → `player.playFolder()` → `sendStack()` → `waitAvailable()` → `DFRobotDFPlayerMini::available()`. Die innere `while(_serial->available())`-Schleife dieser Bibliotheksfunktion hatte im Original keine Zeitbegrenzung – bei anhaltendem Byte-Zustrom (floatende/gestörte RX-Leitung bei getrenntem/defektem DFPlayer) blockierte sie unbegrenzt, noch bevor `waitAvailable()` seinen eigenen 500-ms-Timeout prüfen konnte. Erklärt auch `CPU 1: IDLE1` im TWDT-Log: echter Systemstillstand, kein reiner `alarmTask`-Hänger. Die 12v09–12v12-Fixes griffen nicht, weil der Hänger bereits innerhalb der Bibliothek passierte. |
| 12v13 | Bugfix | Fix direkt in `DFRobotDFPlayerMini.cpp` (lokale Installation `D:\Arduino\libraries\DFRobotDFPlayerMini\`, außerhalb dieses Repos): `available()` bricht jetzt nach 100 ms auch bei anhaltendem Byte-Zustrom ab, `_receivedIndex` bleibt für den nächsten Aufruf erhalten. Muss nach jeder Neuinstallation der Bibliothek erneut angewendet werden – dokumentiert in `Software/Bibliotheken/README.md`. Kein Code in der Firmware selbst geändert, Version nur für Nachvollziehbarkeit auf dem Gerät hochgezählt. |

bTn Wecker  ·  Änderungshistorie  ·  Stand 12v13
