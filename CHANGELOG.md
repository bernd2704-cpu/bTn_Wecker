**bTn Wecker**

Änderungshistorie

Basis 4v1  →  13v00 (Hardware 1v0, eingefroren)  →  20v03 (Hardware 2v0, DFPlayer-BUSY an GPIO34)

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
| 9v0 | Qualität | Stack-Größen als STACK_*-Konstanten ausgelagert |
| 9v1 | Qualität | SystemConfig.h → SysConf_9v1.h umbenannt, Versionshistorie im Datei-Header eingeführt |
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

## Version 12v14

| Version | Kategorie | Änderung |
|---|---|---|
| 12v14 | Bugfix | 12v13-Diagnose widerlegt und Bibliotheks-Patch zurückgenommen: Hardware wurde mit 12v08 als voll funktionsfähig verifiziert – dort existierte weder Patch noch Bibliotheksänderung, der Hang war also firmwareseitig verursacht, nicht in `DFRobotDFPlayerMini::available()` selbst. `DFRobotDFPlayerMini.cpp` bleibt jetzt unverändert im Originalzustand. |
| 12v14 | Bugfix | Root Cause tatsächlich gefunden: Der in 12v11 eingeführte Puffer-Drain in `triggerAlarm()` vor `player.playFolder()` las rohe Bytes direkt per `Serial2.read()` – vorbei an `DFRobotDFPlayerMini`s eigenem Frame-Parser. Riss dieser Discard mitten in einem noch eintreffenden Frame ab (z.B. verspätete Antwort auf das vorherige Kommando), desynchronisierte er `_receivedIndex`/`_isSending` der Bibliothek und erzeugte dadurch genau das anhaltende Byte-Durcheinander auf der UART, das dann in `available()` zum 15-Sekunden-TWDT-Hänger führte – ein in 12v08 unmögliches Szenario, da dort kein roher Drain existierte. |
| 12v14 | Bugfix | Fix: Drain in `triggerAlarm()` läuft jetzt über `player.available()`/`player.read()` statt über rohe Bytes (gleiches Muster wie `readStateDrained()`, 12v09/12v11) – hält den Puffer sauber, ohne den Parser-Zustand der Bibliothek zu zerstören. |

## Version 12v15

| Version | Kategorie | Änderung |
|---|---|---|
| 12v15 | Bugfix | Laufzeitfehler `E (…) task_wdt: esp_task_wdt_reset(707): task not found` behoben: `esp_task_wdt_reconfigure()` setzt laut ESP-IDF ein bereits initialisiertes TWDT voraus, wurde in `setup()` aber erst **nach** dem Start von `inputTask`/`displayTask`/`alarmTask` aufgerufen. Die Annahme „Core 3.x initialisiert das TWDT bereits beim Boot" traf hier nicht zu (oder war zeitlich nicht garantiert vor dem ersten `esp_task_wdt_add()` der neu erzeugten Tasks) – die Anmeldung lief ins Leere, jeder folgende `esp_task_wdt_reset()` brach ab. Damit hätte ein echter Task-Hang nie einen Hardware-Watchdog-Reset ausgelöst, das TWDT überwachte de facto nichts. |
| 12v15 | Bugfix | Fix: `esp_task_wdt_init()` wird jetzt **vor** dem Start der FreeRTOS-Tasks aufgerufen; ist das TWDT bereits initialisiert (`ESP_ERR_INVALID_STATE`), erfolgt stattdessen `esp_task_wdt_reconfigure()`. `esp_task_wdt_add()` in den Task-Funktionen trifft dadurch garantiert auf ein existierendes TWDT. |

## Version 12v16

| Version | Kategorie | Änderung |
|---|---|---|
| 12v16 | Bugfix | Laufzeitfehler `E (…) task_wdt: esp_task_wdt_init(517): TWDT already initialized` behoben: Der 12v15-Fix rief `esp_task_wdt_init()` blind auf und fing nur den Rückgabewert `ESP_ERR_INVALID_STATE` ab – die Bibliotheksfunktion loggt den Fehler aber unabhängig davon per `ESP_LOGE()` selbst, sobald das TWDT (wie sich auf dieser Hardware zeigte) bereits vom Boot-Vorgang existiert. Damit war die ursprüngliche Annahme „Core 3.x initialisiert das TWDT bereits beim Boot" korrekt – das eigentliche 12v14/12v15-Problem lag ausschließlich in der Reihenfolge relativ zum Task-Start, nicht im Fehlen der Initialisierung. |
| 12v16 | Bugfix | Fix: `esp_task_wdt_status(NULL)` fragt jetzt vorher ab, ob das TWDT existiert (`ESP_ERR_INVALID_STATE` = noch nicht initialisiert) – nur dann `esp_task_wdt_init()`, andernfalls weiterhin nur `esp_task_wdt_reconfigure()`. Kein `ESP_LOGE`-Seiteneffekt mehr bei jedem Boot. |

## Version 12v17

| Version | Kategorie | Änderung |
|---|---|---|
| 12v17 | Qualität | Web-Log-Rauschen reduziert: `checkSerial2Leftover()` meldete bisher bei jeder DFPlayer-Abfrage mit Restbytes eine eigene `[DFPlayer]`-Zeile, auch wenn die Byteanzahl über viele Aufrufe hinweg unverändert blieb (z.B. bei dauerhaftem UART-Rauschen). Neue statische `serial2LeftoverLastLogged` merkt sich den zuletzt gemeldeten Wert; `webLogf()` schreibt jetzt nur noch, wenn sich die Restbyte-Anzahl gegenüber der letzten Meldung ändert. `serial2LeftoverCount` (der „seit Boot"-Wert in der Log-Zeile) zählt weiterhin jede Abfrage mit Restbytes, unabhängig vom Log. |

## Version 12v18

| Version | Kategorie | Änderung |
|---|---|---|
| 12v18 | Bugfix | 12v17-Fix griff während `ALARM_RUNNING` nicht: `checkSerial2Leftover()` setzte `serial2LeftoverLastLogged` bei `avail==0` auf 0 zurück – der Puffer wird aber zwischen den beiden `readState`-Abfragen pro Poll-Zyklus durch `readStateDrained()` leergezogen, sodass derselbe wiederkehrende Restbyte-Wert (Poll → Restbytes → auf 0 gedraint → nächster Poll dieselbe Anzahl) bei jedem Zyklus erneut als „neue" Änderung gewertet und geloggt wurde. |
| 12v18 | Bugfix | Fix: Der zuletzt gemeldete Wert bleibt jetzt über Nullstände hinweg erhalten (Startwert `-1` statt `0`, kein Reset mehr im `avail==0`-Zweig) – nur ein tatsächlich anderer Restbyte-Wert löst noch eine neue Web-Log-Zeile aus. |

## Version 12v19

| Version | Kategorie | Änderung |
|---|---|---|
| 12v19 | Bugfix | Log-Analyse vom 10.08.2026 zeigte trotz 12v09–12v18-Fixes weiterhin wiederkehrende Restbytes von 20–30 (2–3 Frames) sowie einen erneuten `verifyPlayStarted()`-Fehlschlag (Versuch 1/3, st=-1). Ursache: Der Vorab-Drain aus 12v14 lief nur unmittelbar vor dem alarmspezifischen `playFolder()` in `triggerAlarm()`. `volume()`, `stop()` und die Testsound-`playFolder()`-Aufrufe (Sound1/Sound2 an/aus, Lautstärke +/−, Funktionswahl-Stop) sendeten dagegen ungedraint – dort entstehende Restframes (z.B. ACK je Befehl) blieben liegen, bis ein zufälliger späterer `readStateDrained()`-Aufruf sie mit abräumte, im ungünstigen Fall genau während der nächsten Alarm-Verifikation. |
| 12v19 | Bugfix | Fix: Inline-Drain aus `triggerAlarm()` in neue Funktion `drainSerial2Pre()` ausgelagert und zusätzlich vor allen `volume()`-, `stop()`- und Testsound-`playFolder()`-Aufrufen aufgerufen (`onClock()` Lautstärke +/−, `checkboxSound()` Sound1/Sound2 an/aus, Funktionswahl-Stop, S1-Stop). |

## Version 13v00

| Version | Kategorie | Änderung |
|---|---|---|
| 13v00 | Bugfix | Root Cause von `kein Start-Status nach playFolder` (st=-1) gefunden: `DFRobotDFPlayerMini::readState()` gibt bei JEDER Nicht-Feedback-Frame-Art (z.B. unaufgeforderter Play-Finished-Meldung 0x3C/0x3D vom vorherigen Titel) sofort -1 zurück, ohne weiter auf die tatsächliche Antwort zu warten. Der in 12v19 vermutete Zusammenhang mit dem ACK-Modus (0x41-Frame) traf **nicht** zu – `parseStack()` der Bibliothek verschluckt 0x41 bereits intern, unabhängig vom Fund, und setzt dabei nie `_isAvailable`. |
| 13v00 | Bugfix | Fix 1: `readStateDrained()` wartet nach einem `-1` von `readState()` noch bis zu `SERIAL2_FEEDBACK_GRACE_MS` (100 ms) lang aktiv auf einen echten Feedback-Frame (Timer wird bei jedem weiteren Störframe zurückgesetzt), statt sofort aufzugeben. |
| 13v00 | Qualität | Fix 2 (zurückgenommen, siehe unten): ACK-Modus deaktiviert (`player.begin(Serial2, false, true)`) – entfernt die blockierende Wartelogik in `sendStack()` der Bibliothek (wartet bei aktivem ACK vor jedem neuen Befehl auf das ACK des vorherigen). War nicht die Ursache des st=-1-Problems, aber unnötige Latenzquelle; der Erfolg eines Befehls wird ohnehin über die tatsächliche Statusabfrage verifiziert. |
| 13v00 | Refactoring | Nebeneffekt von Fix 2: `player.begin()` prüft ohne ACK nicht mehr auf Card-/USB-Online, liefert unbedingt `true` – das `if(player.begin(...)) {…} else { webLog("Verbindung fehlgeschlagen!"); }` in `setup()` war dadurch unreachable code. `else`-Zweig entfernt, `if` in unbedingten Ablauf aufgelöst; die Verbindungsprüfung übernimmt jetzt allein die bereits vorhandene `readFileCounts()`-Schleife mit `SETUP_MP3_TIMEOUT_MS`-Fallback ("Timeout – mp3Count unbekannt"). |
| 13v00 | Bugfix | Regression durch Fix 2 entdeckt: Startsound blieb nach Boot aus. Ursache: Ohne die ACK-Wartelogik sendet `sendStack()` `volume()`/`EQ()`/`playFolder()` (Startsound) in `setup()` nur noch mit einem pauschalen 10-ms-Delay hintereinander, statt auf die tatsächliche Verarbeitung des jeweils vorherigen Befehls zu warten – direkt nach `reset()` kam der DFPlayer damit nicht mehr mit, `playFolder(2, 1)` ging unterwegs verloren. |
| 13v00 | Bugfix | Fix 2 zurückgenommen: ACK-Modus bleibt aktiv (`player.begin(Serial2, true, true)`), `if/else`-Verbindungscheck in `setup()` wiederhergestellt. Der 0x41-ACK-Frame war ohnehin nie die Ursache des st=-1-Problems (siehe oben) – Fix 1 bleibt unverändert wirksam. |

Firmware 13v00 eingefroren, verknüpft mit Hardware 1v0 (kein BUSY-Signal).
Ab hier neue Basis Hardware 2v0 (DFPlayer-BUSY-Signal, GPIO34) / Firmware 20v00.

## Version 20v00

| Version | Kategorie | Änderung |
|---|---|---|
| 20v00 | Funktion | Basis 13v00, Hardware ab 2v0: `SysConf_20v00.h` – Pin-Konstante `DFPLAYER_BUSY` (GPIO34, DFPlayer BUSY-Pin) ergänzt. |
| 20v00 | Funktion | `pinMode(DFPLAYER_BUSY, INPUT)` in `setup()`; neue Hilfsfunktion `dfPlayerBusy()` (direkter GPIO-Read, LOW=Wiedergabe läuft) – zunächst ungenutzt bereitgestellt. |

## Version 20v01

| Version | Kategorie | Änderung |
|---|---|---|
| 20v01 | Stabilität | `runAlarmMachine()`/`ALARM_RUNNING`: `dfPlayerBusy()` löst den bisherigen "-1=UART-Timeout → Alarm läuft sicherheitshalber weiter"-Fallback ab. Serial2-Timeouts von `readStateDrained()` gelten jetzt nur noch als "MP3 beendet", wenn der BUSY-Pin (direktes Hardware-Signal, kein UART) das bestätigt. |

## Version 20v02

| Version | Kategorie | Änderung |
|---|---|---|
| 20v02 | Stabilität | `verifyPlayStarted()`: Erfolgskriterium von `st > 0` auf `st > 0 \|\| dfPlayerBusy()` erweitert – vermeidet einen unnötigen `ESP.restart()`, wenn der DFPlayer tatsächlich läuft, aber `readStateDrained()` (UART) trotz Gnadenfrist weiter `st<=0` liefert. |
| 20v02 | Stabilität | S1-Handling (`inputTask`): 200-ms-Timeout-Fallback bei anhaltendem `st==-1` fragt jetzt `dfPlayerBusy()` statt blind `st=0` ("idle") zu setzen – verhindert, dass bei einem UART-Timeout fälschlich der Kuckuck statt `stop()` ausgelöst wird, obwohl noch ein Alarm/Sound läuft. |

## Version 20v03

| Version | Kategorie | Änderung |
|---|---|---|
| 20v03 | Stabilität | QA-Review (Code-Review-Agent) identifizierte `dfPlayerBusy()` als ungefilterten Einzel-`digitalRead()` ohne Software-Debounce, anders als Touch-Pads/Taster. Für die einzige unwiderrufliche Entscheidungsstelle (`ALARM_RUNNING`: Motor/Licht aus, State-Reset bei jedem Poll) neue Funktion `dfPlayerIdleDebounced()` – 3 Abtastungen im Abstand von 5 ms müssen übereinstimmend "idle" melden, sonst gilt der Alarm als weiterhin laufend. Ergänzt den bereits vorhandenen Hardware-RC-Filter (100µs Zeitkonstante, siehe `Hardware/Schaltplan/DFPlayer-BUSY.md`) um Software-seitige Absicherung gegen Störimpulse, die den Filter überdauern. `verifyPlayStarted()` und S1-Handling bleiben bei einfachem `dfPlayerBusy()` (geringere Konsequenz eines Fehlreads: Retry-Schleife bzw. einmaliger Tastendruck). |

## Version 20v04

Audit-Reaktionsplan (`Dokumentation/Technisch/Review/AuditReaktionsplan.md`) Schritt 1: A1 + A5 gemeinsam umgesetzt, erledigt dabei A4 und A6 mit.

| Version | Kategorie | Änderung |
|---|---|---|
| 20v04 | Stabilität | Neue Funktion `timeValid()` (A5-Fix): `alarmTask` wertet Alarm- und Kuckuck-State-Machine nur noch bei gültiger Systemzeit (`time(nullptr) > 1700000000UL`) aus – verhindert Fehlalarme auf der ungestellten 1970-Uhr (Stromausfall ohne WLAN-Wiederverbindung). |
| 20v04 | Bugfix | `runAlarmMachine()`: Alarmfälligkeit von `sec==0`-Flanke auf tagesbezogene, pegelbasierte Prüfung (`alarmDue()`) mit Nachholfenster `ALARM_CATCHUP_MIN` (60 min, SysConf) umgestellt (A1). Fängt Zeitsprünge nach Stromausfall (NTP-Hardsync), Reboots im Alarmfenster und die März-Zeitumstellung ab, die zuvor den Alarm des Tages ersatzlos gelöscht haben. `lastA1Min`/`lastA2Min` durch `lastA1Day`/`lastA2Day` (tm_yday) ersetzt; die Sperre wird nach erfolgreichem Alarmstart bewusst bis Tagesende gehalten (auch nach manuellem Stopp über S1) statt wie bisher sofort wieder aufgehoben – vermeidet ein sofortiges Neuauslösen durch die pegelbasierte Prüfung. |
| 20v04 | Bugfix | `runAlarmMachine()`: Fälligkeitsprüfung läuft jetzt bei jedem 500-ms-Tick, nicht mehr nur exklusiv im `case ALARM_IDLE` zur exakten Sekunde – Alarm 2 wird dadurch nicht mehr übersprungen, wenn er während eines laufenden Alarm 1 fällig wird (A4), da die Prüfung beim Rückkehren nach `ALARM_IDLE` automatisch nachzieht. |
| 20v04 | Bugfix | `runCuckooMachine()`: Sperre von reiner Minute (`lastCuckooMin`) auf Tag+Stunde (`lastCuckooDay`/`lastCuckooHour`) umgestellt (A6) – verhindert einen doppelten Kuckuck bei der Oktober-Zeitumstellung, wenn die Ortszeit 02:00–02:59 zweimal durchläuft. Manueller Einmal-Kuckuck über S1 setzt dieselbe Sperre über einen eigenen `localtime_r()`-Schnappschuss. |
| 20v04 | Refactoring | `triggerAlarm()` setzt `lastA1Day`/`lastA2Day` über einen eigenen Zeitstempel statt des `min`-Parameters. Fehlgeschlagener `playerMutex`-Take beim Alarmversuch (A2) heilt sich dadurch im nächsten 500-ms-Tick automatisch aus, solange das Nachholfenster nicht abgelaufen ist – kein Codepfad mehr nötig, der den Fehlschlag gesondert behandelt. |

## Version 20v05

Audit-Reaktionsplan Schritt 2, Restarbeit A3 (C1 bereits seit 20v02 erledigt, siehe dort).

| Version | Kategorie | Änderung |
|---|---|---|
| 20v05 | Stabilität | Neue Konstante `ALARM_MAX_RUN_MS` (15 min, SysConf) und eigener Zeitstempel `alarmRunStart` (A3): harte Obergrenze für `ALARM_RUNNING` als Rückfallebene, falls der DFPlayer-BUSY-Pin selbst dauerhaft LOW hängt (Modul-Fehlfunktion, Leitungsfehler) und `dfPlayerIdleDebounced()` dadurch nie "idle" meldet – ohne diesen Deckel bliebe das Gerät in diesem Fall für immer entwaffnet (Motor/Licht dauerhaft an, kein weiterer Alarm möglich). Anders als `t_start6` (Poll-Timer, wird bei jedem Poll zurückgesetzt) bleibt `alarmRunStart` bis zum Alarmende unverändert. Bei Auslösung durch die Obergrenze (nicht durch reguläres Alarmende) schreibt `runAlarmMachine()` einen `[FEHLER]`-Eintrag mit Laufzeit und letztem `playerStatus` ins Web-Log. |

## Version 20v06

Audit-Reaktionsplan Schritt 3 (B2).

| Version | Kategorie | Änderung |
|---|---|---|
| 20v06 | Stabilität | `triggerAlarm()` schreibt den RTC-Retry-Merker (`rtcRetryMagic` u.a.) jetzt direkt nach dem `playerMutex`-Take, VOR `playFolder()`/`verifyPlayStarted()` – bisher wurde er nur im expliziten Fehlerpfad (bestätigter `ESP.restart()`) gesetzt. Ein Freeze/Reset irgendwo zwischen `playFolder()` und der ersten bestätigten Wiedergabe in `ALARM_RUNNING` (inkl. des `ALARM_POLL_MS`-Fensters vor dem ersten Poll) verlor den Alarm bisher ersatzlos, weil kein RTC-Merker existierte. Sicher erst durch den Tages-Guard aus A1 (20v04): ein später Reboot löst höchstens einen bereits gelaufenen Alarm erneut aus, nie einen verspäteten Zweitalarm (B2). |
| 20v06 | Stabilität | `runAlarmMachine()`/`ALARM_RUNNING` löscht den Merker an zwei Stellen: sobald der erste Poll `playerStatus > 0` bestätigt (frühestmöglich), und zusätzlich beim Alarmende (`mp3Finished`/`runTimeExceeded`) als Sicherheitsnetz für einen sehr kurzen Sound, der schon vor dem ersten Poll beendet war. |
| 20v06 | Refactoring | Der Fehlerpfad in `triggerAlarm()` (`ESP.restart()` nach gescheitertem `verifyPlayStarted()`) aktualisiert nur noch `rtcRetryCount` – die übrigen Felder sind bereits durch den frühen Schreibpunkt gesetzt. |

## Version 20v07

Audit-Reaktionsplan Schritt 4 (C5 + D1).

| Version | Kategorie | Änderung |
|---|---|---|
| 20v07 | Bugfix | `setup()`: Clamp von `sound1_assigned`/`sound2_assigned` läuft nur noch bei bekannter `mp3Count` (C5). Bei `mp3Count==0` (nach fehlgeschlagenem `player.begin()`) war die Clamp-Bedingung bisher immer wahr – `readNVR()` begrenzt die Zuordnung nur nach unten auf 1, nie nach oben – und überschrieb beide Sound-Zuordnungen mit 1. Verstellt der Nutzer danach irgendetwas, persistiert `markSafeChange()` diesen falschen Wert unwiederbringlich in NVS. Bei unbekannter Dateizahl jetzt `[FEHLER]`-Log statt stillem Überschreiben. |
| 20v07 | Bugfix | `data.begin()`-Rückgabewert wird jetzt an allen drei Stellen geprüft (D1): `setup()` (NVR-Laden), `nvrTask()` (periodischer Commit), `bumpResetCount()`. Schlägt das Öffnen fehl, liefen `putBool`/`putInt`-Aufrufe bisher als No-Op ins Leere – eine soeben geänderte Einstellung ging beim nächsten Stromausfall ersatzlos verloren, ohne Hinweis im Log. `nvrTask()` setzt bei Fehlschlag über `markSafeChange()` erneut `safeChange`, wodurch der Commit nach `NVR_COMMIT_DELAY_MS` automatisch wiederholt wird, statt endgültig verworfen zu werden. |
| 20v07 | Bugfix | `setup()`: `readNVR()` läuft jetzt unabhängig vom `state`-Flag (statt nur im `varState==true`-Zweig) – die `getX()`-Fallbacks liefern beim allerersten Boot ohnehin die Compile-Defaults zurück, ein bedingter Aufruf war unnötig und verdeckte zusätzlich den Fall „`data.begin()` schlägt fehl, `state` bleibt beim Default `false`". |
| 20v07 | Refactoring | `webLogMutex`-Initialisierung in `setup()` vor das NVR-Laden gezogen, damit ein `data.begin()`-Fehlschlag dort sofort als `[FEHLER]` ins Web-Log geschrieben werden kann statt lautlos verworfen zu werden. |

## Version 20v08

Audit-Reaktionsplan Schritt 5, B1 sekundär.

| Version | Kategorie | Änderung |
|---|---|---|
| 20v08 | Stabilität | `WDG_TIMEOUT_MS` 30000→10000, `WDG_CHECK_MS` 5000→1000 (SysConf). Bei den bisherigen Werten gewinnt der Hardware-TWDT (15 s) bei jedem Freeze immer zuerst – der Software-Watchdog kam nie zum Zug (Audit-Befund B1). Abweichend vom Audit-Vorschlag (6000/1000) wurde der Wert anhand einer eigenen Worst-Case-Rechnung gewählt: `verifyPlayStarted()` kann bei einem dauerhaft nicht antwortenden DFPlayer bis zu ~5,5–6 s blockieren (3× `VERIFY_PLAY_RETRIES`, je `VERIFY_PLAY_DELAY_MS` + Mutex-Take + `readStateDrained()`-Gnadenfrist bei UART-Rauschen, deckt sich mit der im Audit selbst hergeleiteten Summe). Bei 6000 wäre die Marge dazu praktisch null gewesen – ein nur ungewöhnlich langsamer, aber regulärer Alarmversuch hätte den Watchdog fälschlich als Freeze werten können. Mit 10000/1000 liegt die Erkennung spätestens bei ~11 s: rund 5 s Marge über dem berechneten Worst-Case, weiterhin rund 4 s Marge unter den 15 s des TWDT. Begründung als Kommentar direkt bei den Konstanten in SysConf dokumentiert. |

## Version 20v09

Audit-Reaktionsplan Schritt 6 (C3), zusammen mit A3 verifiziert.

| Version | Kategorie | Änderung |
|---|---|---|
| 20v09 | Bugfix | `verifyPlayStarted()` liefert jetzt `PLAY_OK`/`PLAY_NO_SOUND`/`PLAY_CRASHED` statt `bool` (C3). `st==0` (Modul antwortet, „gestoppt" – typisch für „Datei nicht gefunden") und `st==-1` (keine Antwort) wurden bisher identisch als Absturz behandelt und lösten beide `ESP.restart()` aus – ein Neustart behebt einen Datei-/Kartenfehler aber nicht. Nur wenn nie eine Antwort ankam (`PLAY_CRASHED`), rechtfertigt das noch einen Neustart. Kam mindestens einmal `st==0` an (`PLAY_NO_SOUND`), gilt das Modul als lebendig; `triggerAlarm()` läuft in denselben Erfolgspfad wie `PLAY_OK` (Motor/Licht), statt zwei sinnlose Reboots auszulösen und danach still zu bleiben. |
| 20v09 | Bugfix | Neues Flag `alarmSilentFallback`: hält `ALARM_RUNNING` bei `PLAY_NO_SOUND` am Laufen, obwohl `playerStatus` dauerhaft `0` meldet – ohne dieses Gate hätte `mp3Finished` den Alarm schon beim ersten Poll (5 s) sofort wieder beendet. Einziger Ausstieg dann `ALARM_MAX_RUN_MS` (A3, zusammen mit C3 verifiziert wie von der Review-Notiz gefordert) oder manueller Stopp über S1. |
| 20v09 | Bugfix | S1-Handler (`inputTask`) entscheidet jetzt zusätzlich anhand von `alarmState`, nicht mehr nur anhand des Playerstatus, ob gestoppt oder Kuckuck ausgelöst wird (`if (alarmState == ALARM_RUNNING \|\| st > 0)`). Ohne diese Ergänzung hätte S1 einen `alarmSilentFallback`-Alarm (playerStatus==0, aber Motor/Licht laufen) nicht gestoppt, sondern versehentlich den Kuckuck ausgelöst. |
| 20v09 | Funktion | `readStateDrained()` wertet `DFPlayerError`-Frames vor dem Verwerfen aus und schreibt den Fehlercode ins Web-Log, statt sie wie jeden anderen Nicht-Feedback-Frame kommentarlos zu verwerfen – macht „Datei fehlt" von „Modul abgestürzt" im Log erstmals unterscheidbar. |

## Version 20v10

Audit-Reaktionsplan Schritt 7 (C4).

| Version | Kategorie | Änderung |
|---|---|---|
| 20v10 | Bugfix | `setup()`: `player.readFileCountsInFolder(1)` (0x4E) statt `player.readFileCounts()` (0x48) - `-1`. Bisher wurde die Gesamtdateizahl über alle Ordner abgefragt und `mp3Count = c - 1` gerechnet, in der Annahme, genau eine Datei liege außerhalb von Ordner 01 (der Startsound in Ordner 02) – zusätzliche Dateien in Ordner 02 oder vom Betriebssystem beim Kopieren angelegte Extra-Dateien machten diese Zahl falsch und erlaubten die Auswahl einer nicht existierenden Alarmdatei (Zubringer zu C3s Reboot-Kaskade). Die ordnerbezogene Abfrage liefert die Anzahl direkt für Ordner 01, kein Rechnen/Raten mehr nötig. |
| 20v10 | Bugfix | Bei Timeout der Bereitschaftsprüfung bleibt `mp3Count` jetzt auf 0 statt auf den geratenen Fallback 99 gesetzt zu werden – C5 (20v07) sperrt die Sound-Auswahl dann korrekt, statt eine falsche, nicht verifizierte Dateizahl vorzugaukeln. `[FEHLER]`-Log statt der bisherigen informativen `[DFPlayer]`-Meldung. |

## Version 20v11

Audit-Reaktionsplan Schritt 8 (C2) – zuletzt und ausdrücklich isoliert (dieser Bereich hat in
12v09–12v14 dreimal Regressionen erzeugt). Rollback-Punkt vor dieser Änderung als Git-Tag
`vor-C2-20v10` festgehalten.

| Version | Kategorie | Änderung |
|---|---|---|
| 20v11 | Bugfix | `drainSerial2Pre()`/`readStateDrained()` auf gemeinsame fortschrittsbasierte Drain-Schleife (`drainSerial2Progress()`) umgestellt. Bisher brach `drainSerial2Pre()` am Rückgabewert von `player.available()` ab – der auch nach einem vollständig konsumierten 0x41-ACK-Frame `false` liefert (`parseStack()` der Bibliothek behandelt den ACK intern, ohne `_isAvailable` zu setzen). Bei Pufferinhalt `[ACK][Feedback]` brach der Drain dadurch nach dem ACK ab und ließ den Feedback-Frame stehen; ein späterer `readStateDrained()`-Aufruf konnte diesen Altframe als Antwort auf eine ganz andere, neue Abfrage werten (Motor/Licht liefen, obwohl kein Ton spielte, ohne Fehlerlog). Die neue Schleife macht Fortschritt am Rohpuffer selbst fest (`Serial2.available()` vor/nach dem Read vergleichen) statt am Parser-Rückgabewert. |
| 20v11 | Bugfix | `readStateDrained()` ruft `drainSerial2Progress()` jetzt zusätzlich VOR der eigenen `player.readState()`-Abfrage auf – verhindert, dass ein bereits vor diesem Aufruf im Puffer liegender Altframe als Antwort auf die gerade gesendete Abfrage gewertet wird. |

## Version 20v12

Audit-Reaktionsplan Schritt 10 (E1, C6, C7, D2). E2 und E4 auf Nutzerwunsch bewusst zurückgestellt
(Boot-Taster des DevKit gewollt genutzt bzw. Bestätigungsabfrage bewusst nicht gewünscht).

| Version | Kategorie | Änderung |
|---|---|---|
| 20v12 | Bugfix | `touchTask`: erzwungener Reset auf `TS_IDLE` inkl. Baseline-Neuerfassung, wenn ein Pad länger als `TOUCH_MAX_HOLD_MS` (30 s) ununterbrochen "gedrückt" bleibt (E1). Bisher hatten `TS_PRESSED`/`TS_REPEAT` keinen Ausstieg außer dem Loslassen selbst – ein klemmendes Pad (Feuchtigkeit, Kabeldefekt) sendete unbegrenzt alle `TOUCH_REPEAT_MS` ein Event, z.B. zählte T4 die Lautstärke bis 0 herunter. `[WARNUNG]`-Log bei Auslösung. |
| 20v12 | Bugfix | `triggerAlarm()` sendet vor jedem Alarm `player.volume()` mit Untergrenze `ALARM_MIN_VOL` (10), unabhängig vom zuletzt gesendeten Wert (E1). Schützt gegen einen durch ein klemmendes Touch-Pad auf 0 heruntergezählten `vol`, ohne die persistierte Einstellung selbst zu verändern. |
| 20v12 | Bugfix | Neues Flag `alarmCancelRequested` (C6): S1, Sound-Vorschau-Stopp und das Funktionswahl-Menü setzen es über `requestAlarmCancelIfActive()`, wenn ihr `player.stop()` einen laufenden oder gerade startenden Alarm treffen könnte. `verifyPlayStarted()` liefert dann `PLAY_CANCELLED` statt `PLAY_CRASHED` – `triggerAlarm()` bricht sauber ab (Tages-Sperre trotzdem gesetzt) statt neu zu starten oder `alarmSilentFallback` zu aktivieren (Motor/Licht wären sonst kurz nach dem Stopp wieder angegangen). |
| 20v12 | Refactoring | Die Tages-Sperren-Zuweisung in `triggerAlarm()` in die neue Funktion `lockAlarmDayGuard()` ausgelagert – wird jetzt sowohl vom Erfolgspfad als auch vom neuen `PLAY_CANCELLED`-Abbruchpfad genutzt. |
| 20v12 | Bugfix | `onClock()` (T3/T4, Lautstärke) und `checkboxSound()` (Sound-Vorschau) ändern `vol`/`sound*_assigned`, Anzeige und `markSafeChange()` jetzt erst nach erfolgreichem `playerMutex`-Take (C7). Bisher liefen Zustandsänderung und Anzeige unbedingt, auch wenn der Take fehlschlug (z.B. während `alarmTask` den Mutex im Poll hält) – Anzeige/NVS und tatsächliche Player-Lautstärke liefen dann auseinander. Fehlschlag jetzt als `[FEHLER]` geloggt. |
| 20v12 | Stabilität | `html.reserve()` in der Web-Log-Startseite von 8192 auf 12288 Byte angehoben – der bisherige Wert deckte den Worst Case (festes Markup + 40 Ringpufferzeilen + Snapshots, real bis ~9,8 kB) nicht (D2). `/log`-Handler reserviert jetzt ebenfalls (`out.reserve(6144)`), vorher gar nicht. |

## Version 20v13

Audit-Reaktionsplan Schritt 11 (zwei Zweizeiler) – letzter offener Punkt der Umsetzungsreihenfolge.

| Version | Kategorie | Änderung |
|---|---|---|
| 20v13 | Bugfix | `esp_task_wdt_add(NULL)` in `inputTask`/`displayTask`/`alarmTask` sowie `esp_task_wdt_init()`/`esp_task_wdt_reconfigure()` in `setup()` werten jetzt den Rückgabewert aus. Schlägt die Anmeldung fehl, wird `[FEHLER]` geloggt statt wie bisher unbedingt "[TWDT] Hardware Watchdog aktiv" zu behaupten – genau der 12v15-Fehler (TWDT nie wirklich initialisiert, `esp_task_wdt_reset()` läuft ins Leere) wäre bisher symptomfrei geblieben. |
| 20v13 | Bugfix | `webLog("[NTP] Synchronisation OK")` und `snapTimeStr(snapNtpTime, …)` laufen jetzt nur noch bei tatsächlichem NTP-Erfolg, nicht mehr unbedingt nach der Warteschleife (auch nach einem Timeout-`break`). Vorher hätte das Log nach einem ausgefallenen Alarm einen Uhrzeitfehler fälschlich ausgeschlossen – diagnostisch teuer im Zusammenspiel mit A1/A5. |

Damit ist die im Audit-Reaktionsplan vorgesehene Umsetzungsreihenfolge (Schritte 1–11) vollständig
abgearbeitet. Offen bleiben nur noch die im Audit selbst nie gegengeprüften Punkte (siehe
`AuditReaktionsplan.md`, Abschnitt „Offene Prüfpunkte").

## Version 20v14

| Version | Kategorie | Änderung |
|---|---|---|
| 20v14 | Bugfix | Compile-Fehler `'PlayVerifyResult' does not name a type` behoben: Die Arduino-IDE fügt automatisch generierte Funktionsprototypen an den Dateianfang ein, noch vor der bisherigen `enum PlayVerifyResult`-Definition bei `verifyPlayStarted()` weiter unten in der Datei. Enum-Definition direkt nach die Includes verschoben, damit sie vor dem generierten Prototyp sichtbar ist. |

## Version 20v15

| Version | Kategorie | Änderung |
|---|---|---|
| 20v15 | Bugfix | `requestAlarmCancelIfActive()` bekommt Parameter `includeStartup`: Sound-Vorschau-Stopp (aus) und der Einstieg ins Funktionswahl-Menü lösen den Alarmabbruch jetzt nur noch aus, wenn der Alarm bereits bestätigt hörbar läuft (`ALARM_RUNNING`), nicht mehr schon während der reinen ~1,5s-Anlaufverifikation (`alarmTriggerInFlight`). Bisher konnte reine Menü-Navigation ohne jeden Bezug zum Alarm einen zeitgleich gerade erst startenden Alarm lautlos abbrechen, weil deren `player.stop()` über `requestAlarmCancelIfActive()` denselben Ausschlag lieferte wie ein bewusster Stopp – beobachtet 19.08.2026: Alarm 2 blieb während Sound-Vorschau-Navigation komplett aus (kein Ton, kein Motor, kein Licht), im Log nur "Wiedergabe waehrend Verifikation manuell gestoppt". S1 (dedizierter Stopp-Taster) behält das bisherige Verhalten (`includeStartup=true`) und darf weiterhin auch während der Anlaufverifikation abbrechen. |
| 20v15 | Bugfix | `triggerAlarm()` setzt `alarmCancelRequested` jetzt defensiv auf `false`, bevor der neue Versuch `playFolder()` sendet. Ein S1-Stopp eines bereits `ALARM_RUNNING`-Alarms setzte das Flag über `requestAlarmCancelIfActive()`, gab es danach aber nie wieder frei – einziger Konsument war `verifyPlayStarted()`. Der stehen gebliebene Altwert hätte den nächsten, damit völlig unbeteiligten Alarmversuch beim allerersten Verifikations-Poll fälschlich als `PLAY_CANCELLED` beendet. |

## Version 20v16

| Version | Kategorie | Änderung |
|---|---|---|
| 20v16 | Bugfix | Tages-Sperre `lastA1Day`/`lastA2Day` zusätzlich im RTC-Speicher (`rtcLastA1Day`/`rtcLastA2Day`, Gültigkeits-Flag `rtcDayGuardMagic`) gespiegelt, nach demselben Muster wie `rtcRetryMagic`. Bisher lag die Sperre nur im normalen RAM und wurde bei JEDEM Neustart auf 0xFFFF zurückgesetzt – auch bei einem harmlosen Reset-Tasterdruck oder Neuflashen kurz nach einem bereits erfolgreich gespielten Alarm. Lag dieser Neustart innerhalb des Nachholfensters (`ALARM_CATCHUP_MIN`, 60 Minuten), hielt `alarmDue()` den Alarm für verpasst und löste ihn erneut aus – beobachtet: beide Alarme unmittelbar nacheinander nach einem Reset, obwohl die aktuelle Uhrzeit keiner der beiden Alarmzeiten entsprach, sondern nur bis zu 60 Minuten danach lag. RTC_NOINIT-Speicher übersteht Reset/Neuflashen, wird aber bei echtem Power-On zufällig befüllt – das Magic-Flag unterscheidet beide Fälle: bei Power-On bleibt das Nachholfenster (verpasster Alarm nach Stromausfall) wie bisher aktiv. |

## Version 20v17

| Version | Kategorie | Änderung |
|---|---|---|
| 20v17 | Bugfix | 20v16-Fix wirkungslos: `RTC_NOINIT_ATTR` übersteht keinen EN-Pin-Reset (Reset-Taster, aber auch der harte Reset, den esptool/Arduino-IDE beim Neuflashen per DTR/RTS auslöst) – der EN-Pin schaltet den internen Spannungsregler des ESP32 kurz komplett ab, das entspricht elektrisch einem echten Power-On und löscht damit auch RTC_NOINIT-Speicher. Nur `ESP.restart()` (reiner Software-Reset, siehe `rtcRetryMagic`) lässt die RTC-Domäne durchgehend mit Strom versorgt. Nutzer meldete 19.08.2026, dass der Alarm nach Neuflashen weiterhin erneut auslöste. Tages-Sperre `lastA1Day`/`lastA2Day` jetzt stattdessen im Flash-NVR persistiert (`writeNVR()`/`readNVR()`, Debounce über `markSafeChange()`/`nvrTask` wie bei allen anderen Einstellungen) – das übersteht zuverlässig jeden Neustart-Typ inklusive echtem Stromausfall. Der bisherige RTC_NOINIT-Ansatz (`rtcDayGuardMagic`, `rtcLastA1Day`, `rtcLastA2Day`) wurde vollständig entfernt. |
| 20v17 | Bugfix | Compile-Fix: `lastA1Day`/`lastA2Day` wurden in `writeNVR()`/`readNVR()` (Zeile ~840) verwendet, aber erst weiter unten im Code (Zeile 1370) deklariert – C++ verlangt die Deklaration vor der ersten Verwendung. Deklaration vor `writeNVR()` gezogen. |
| 20v17 | Bugfix | Tages-Sperre blockte nach dem Ändern der Alarmzeit im Menü (`onAlarm1()`/`onAlarm2()`, T3/T4) den Alarm für den Rest des Tages: `alarmDue()` prüft nur `yday == lastDay`, unabhängig von der eingestellten Uhrzeit. Hatte der Alarm an diesem Tag schon einmal ausgelöst (z.B. als Nachhol-Alarm direkt nach Boot), löste er unter der neuen Zeit gar nicht mehr aus – ohne Web-Log-Eintrag, da `triggerAlarm()` nie aufgerufen wurde. Fix: T3/T4 in `onAlarm1()`/`onAlarm2()` setzen `lastA1Day`/`lastA2Day` bei jeder Stunden-/Minutenänderung auf `0xFFFF` zurück. |

## Version 20v18

| Version | Kategorie | Änderung |
|---|---|---|
| 20v18 | Bugfix | Web-Log: Vor der Überschrift „Verbindung – letzter WiFi Reconnect / NTP Sync" fehlte die Leerzeile zum darüberliegenden Log-Block. Ursache: `#log,#dflog` in der CSS-Regel für Padding/Umbruch, aber nur `#dflog{margin-bottom:16px}` – `#log` hatte keinen Abstand nach unten. Fix: `margin-bottom:16px` auch auf `#log` angewendet. |

## Version 20v19

| Version | Kategorie | Änderung |
|---|---|---|
| 20v19 | Qualität | `TOUCH_REPEAT_RATE_MS` aus SysConf entfernt: ungenutztes Duplikat von `TOUCH_REPEAT_MS` (identischer Wert 250 ms, identischer Zweck), nirgends im Code referenziert. |

## Version 20v20

| Version | Kategorie | Änderung |
|---|---|---|
| 20v20 | Funktion | Hardware-Änderung Motor-Treiber: LDO `MCP1700T-3302E/TO` regelt die Rail vor dem MOSFET jetzt fest auf 3,3V statt den Motor direkt an 5V zu betreiben. PWM schaltet damit die geregelte Spannung – Duty 255 bedeutet nur noch ~3,3V statt vormals 5V. Löst die bei 20v19 offene Prüffrage zur Kickstart-Überspannung (`MOTOR_PWM_KICK_DUTY`=255 lag bisher ungebremst auf 5V, ~1,67× Motor-Nennspannung 3V): voller PWM-Regelumfang 0..255 ist jetzt ohne Begrenzung und ohne Überspannungsrisiko nutzbar. Freilaufdiode korrigiert auf `1N4448` (realer Bauteilwert statt bisher dokumentiertem `1N4148`). |

bTn Wecker  ·  Änderungshistorie  ·  Stand 20v20
