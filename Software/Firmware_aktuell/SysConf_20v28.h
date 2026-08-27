#pragma once
// SysConf_20v25.h – Konfigurationskonstanten für bTn Wecker
// Firmware-Version : 20v25
// Datei-Version    : 20v25
// Boardverwalter   : esp32 3.3.11 von Espressif Systems
// Änderungshistorie: siehe CHANGELOG.md
// 20v00: Basis 13v00, Hardware ab 2v0 (DFPlayer BUSY-Signal an GPIO34)
// 20v01: dfPlayerBusy() im Alarm-Polling – löst UART-Timeout-Fallback ab
// 20v02: dfPlayerBusy() auch in verifyPlayStarted() und S1-Handling
// 20v03: dfPlayerIdleDebounced() (3× Sampling) in ALARM_RUNNING – Absicherung
//        gegen Störimpulse, die trotz RC-Filter durchschlagen
// 20v04: Audit-Reaktionsplan Schritt 1 (A1+A5): tagesbezogene, pegelbasierte
//        Alarmfälligkeit mit Nachholfenster (ALARM_CATCHUP_MIN) statt
//        sec==0-Flanke; timeValid()-Gate vor jeder Alarm-/Kuckuck-Auswertung;
//        Kuckuck-Sperre auf Tag+Stunde umgestellt (A6, Oktober-Zeitumstellung)
// 20v05: Audit-Reaktionsplan Schritt 2, Restarbeit A3: harte Obergrenze
//        ALARM_MAX_RUN_MS für ALARM_RUNNING (eigener Zeitstempel
//        alarmRunStart) – Rückfallebene, falls der BUSY-Pin dauerhaft LOW
//        hängt und dfPlayerIdleDebounced() nie "idle" meldet
// 20v06: Audit-Reaktionsplan Schritt 3 (B2): RTC-Retry-Merker wird in
//        triggerAlarm() jetzt VOR dem riskanten Abschnitt geschrieben statt
//        erst im expliziten Fehlerpfad – gelöscht erst nach bestätigter
//        Wiedergabe bzw. regulärem Alarmende in runAlarmMachine()
// 20v07: Audit-Reaktionsplan Schritt 4 (C5+D1): Sound-Zuordnungs-Clamp nur
//        bei bekannter mp3Count; NVS-Rückgabewerte (data.begin()) an allen
//        drei Stellen geprüft, nvrTask() setzt bei Fehlschlag safeChange
//        erneut; readNVR() läuft unabhängig vom state-Flag
// 20v08: Audit-Reaktionsplan Schritt 5, B1 sekundär: WDG_TIMEOUT_MS 30000→
//        10000, WDG_CHECK_MS 5000→1000 – Wert abweichend vom Audit-Vorschlag
//        (6000/1000) auf Basis einer eigenen Worst-Case-Rechnung gewählt,
//        siehe Kommentar bei den Konstanten unten
// 20v09: Audit-Reaktionsplan Schritt 6 (C3): st==0 (Modul lebt, Datei startet
//        nicht) von st==-1 (keine Antwort) getrennt – nur -1 rechtfertigt
//        ESP.restart(). Bei bestätigtem st==0 läuft der Alarm stumm weiter
//        (Motor/Licht) bis ALARM_MAX_RUN_MS (A3) statt zwei sinnlose Reboots
//        auszulösen. readStateDrained() wertet DFPlayerError-Frames vor dem
//        Verwerfen aus und loggt den Fehlercode. S1 stoppt jetzt zusätzlich
//        anhand von alarmState, nicht mehr nur anhand des Playerstatus.
// 20v10: Audit-Reaktionsplan Schritt 7 (C4): player.readFileCountsInFolder(1)
//        statt player.readFileCounts()-1 (ordnerbezogene statt geratene
//        Zählung); kein geratener 99er-Fallback mehr bei Timeout – mp3Count
//        bleibt 0, Sound-Auswahl bleibt gesperrt statt falsche Dateizahlen
//        vorzugaukeln.
// 20v11: Audit-Reaktionsplan Schritt 8 (C2, isoliert – 3× Regressionen in
//        12v09–12v14 an dieser Stelle): drainSerial2Pre()/readStateDrained()
//        auf gemeinsame fortschrittsbasierte Drain-Schleife umgestellt statt
//        Abbruch am Rückgabewert von player.available() (der auch nach
//        vollständig konsumiertem ACK-Frame false liefert). Rollback-Tag
//        "vor-C2-20v10" markiert den Stand direkt davor.
// 20v12: Audit-Reaktionsplan Schritt 10 (E1, C6, C7, D2). E1: erzwungener
//        Touch-Reset nach TOUCH_MAX_HOLD_MS + player.volume() mit
//        ALARM_MIN_VOL vor jedem Alarm. C6: alarmCancelRequested verhindert
//        Neustart/alarmSilentFallback, wenn S1/Sound-Vorschau/Funktionswahl
//        einen laufenden Alarmversuch stoppen. C7: vol/sound*_assigned erst
//        nach erfolgreichem Mutex-Take ändern. D2: html.reserve() angehoben,
//        /log-Handler reserviert jetzt ebenfalls. E2 und E4 auf Nutzerwunsch
//        bewusst zurückgestellt (Boot-Taster des DevKit gewollt genutzt
//        bzw. Bestätigungsabfrage bewusst nicht gewünscht).
// 20v13: Audit-Reaktionsplan Schritt 11 (zwei Zweizeiler): Rückgabewerte von
//        esp_task_wdt_init()/reconfigure()/esp_task_wdt_add(NULL) jetzt
//        ausgewertet und bei Fehlschlag geloggt, statt "[TWDT] Hardware
//        Watchdog aktiv" blind zu behaupten. NTP-Erfolgsmeldung und
//        snapNtpTime nur noch bei tatsächlichem Sync gesetzt, nicht mehr
//        unbedingt nach Timeout.
// 20v14: Compile-Fix PlayVerifyResult.
// 20v15: Alarm-Abbruch durch Menü-Navigation und stale alarmCancelRequested
//        behoben.
// 20v16: Alarm-Tages-Sperre (lastA1Day/lastA2Day) zusätzlich über
//        RTC_NOINIT_ATTR gespiegelt, damit sie Reset/Neuflashen übersteht –
//        Annahme über RTC-Speicherverhalten war falsch, siehe 20v17.
// 20v17: Bugfix – RTC_NOINIT_ATTR aus 20v16 übersteht keinen EN-Pin-Reset
//        (Reset-Taster, Neuflashen): der EN-Pin schaltet den internen
//        Spannungsregler kurz ab, das entspricht elektrisch einem echten
//        Power-On und löscht auch RTC_NOINIT-Speicher. Fix griff dadurch
//        nicht (Bug weiterhin reproduzierbar, gemeldet 2026-08-19). Tages-
//        Sperre jetzt stattdessen im Flash-NVR persistiert (writeNVR()/
//        readNVR(), lockAlarmDayGuard()) – übersteht zuverlässig jeden
//        Neustart-Typ.
// 20v18: Web-Log-Fix – fehlende Leerzeile vor Überschrift "Verbindung –
//        letzter WiFi Reconnect / NTP Sync": #log hatte anders als #dflog
//        kein margin-bottom.
// 20v19: TOUCH_REPEAT_RATE_MS entfernt – ungenutztes Duplikat von
//        TOUCH_REPEAT_MS (identischer Wert 250, nirgends referenziert).
// 20v20: Hardware-Änderung Motor-Treiber: LDO MCP1700T-3302E/TO regelt die
//        5V-Rail vor dem MOSFET auf 3,3V, PWM schaltet diese geregelte
//        Spannung statt der rohen 5V. Der Motor sieht dadurch bei JEDEM
//        Duty-Wert maximal ~3,3V (statt bisher bis zu 5V bei Duty 255) –
//        die bisherige Überspannungs-Prüffrage beim Kickstart-Vollgasimpuls
//        (siehe Motor-LED-Treiber_Bauteilnotizen.md) ist damit gegenstandslos.
//        Voller PWM-Regelumfang 0..255 ist jetzt ohne Begrenzung nutzbar.
//        Freilaufdiode auf 1N4448 korrigiert (realer Bauteilwert).
// 20v21: Zeitumstellung Wecker (A1-Folgearbeit, Auftrag 21.08.2026): Weckzeit
//        zwischen 02:00:00–02:59:59 wahrt jetzt den geplanten zeitlichen
//        Abstand zum nachfolgenden Termin. Frühjahr: um 1h vorgezogen (die
//        Stunde existiert nicht als Wanduhrzeit). Herbst: löst erst beim
//        zweiten Durchlauf durch die doppelte Stunde aus (Normalzeit statt
//        Sommerzeit). Siehe updateDstDayFlags()/alarmDue() im .ino.
// 20v22: Bugfix Weckzeit-Verstellung: Tages-Sperre (lastA1Day/lastA2Day)
//        wurde bisher bei JEDEM Stunde+/Minute+-Tastendruck aufgehoben (nicht
//        erst beim Verlassen der Alarm-Seite) – beim Durchscrollen einer neuen
//        Weckzeit (z.B. 10:00 → 14:30) lag der Zeitwert dadurch kurzzeitig auf
//        Zwischenwerten, von denen manche sofort als "fällig" erkannt wurden
//        und den Alarm ungewollt auslösten. Aufhebung jetzt erst in
//        uiTransition() beim tatsächlichen Seitenwechsel. Siehe onAlarm1()/
//        onAlarm2()/uiTransition() im .ino.
// 20v23: Bugfix Sound-Vorschau: Beim Betreten der Seite "Sound 2 wählen"
//        wurde sound2_on nicht zurückgesetzt (anders als sound1_on/sound2_on
//        beim Betreten von "Sound 1 wählen") – war die Vorschau vom letzten
//        Besuch der Seite noch aktiv, spielte checkboxSound() beim Neuzeichnen
//        sofort wieder Sound 2 ab. Siehe menu()/Case 4 im .ino.
// 20v24: Stack-Größen erhöht (Sicherheitsmarge auf Basis Stack High-Water
//        Marks): STACK_INPUT 2240→2704, STACK_ALARM 2128→2524,
//        STACK_DISPLAY 2176→2276.
// 20v25: Bugfix Sound-Vorschau: player.stop() in checkboxSound() (Sound 1/2
//        Vorschau ausschalten) schlug bei belegtem playerMutex (>50 ms,
//        z.B. während laufendem Alarm-Poll) folgenlos fehl – Checkbox zeigte
//        "aus", DFPlayer spielte aber unbemerkt weiter. Neues Flag
//        pendingPlayerStopRetry: inputTask holt den Stop danach außerhalb
//        von displayMutex mit größerem Timeout (300 ms) nach.

// ── Firmware-Version ─────────────────────────────────────────
#define FW_VERSION "20v27"                                                     // Versionsnummer (als String in PGMInfo, Web-Log, WEB.h)

// ── WiFi ─────────────────────────────────────────────────────
// STA_SSID / STA_PSK werden nicht mehr direkt genutzt.
// WLAN-Zugangsdaten werden per WebKonfigurator eingerichtet und
// im NVR-Namespace "wifiCfg" gespeichert (ab Version 4v0).
#define STA_SSID  "my_ssid"                                                    // nur als Referenz – nicht mehr in WiFi.begin() genutzt
#define STA_PSK   "my_passwrd"                                                 // nur als Referenz – nicht mehr in WiFi.begin() genutzt

// Access-Point-Konfiguration für den WiFi-Konfigurator
#define WIFI_AP_SSID    "bTn-Wecker"                                           // SSID des Konfigurations-Access-Points
#define WIFI_AP_CHANNEL 1                                                      // WiFi-Kanal des Access-Points

// ── NTP ──────────────────────────────────────────────────────
#define MY_NTP_SERVER "pool.ntp.org"                                           // NTP-Serveradresse
#define MY_TZ         "CET-1CEST,M3.5.0/02,M10.5.0/03"                         // Zeitzone (POSIX-Format)

// ── DFPlayer Serial-Pins ─────────────────────────────────────
#define RXD2 16                                                                // ESP32 GPIO16 → DFPlayer TX
#define TXD2 17                                                                // ESP32 GPIO17 → DFPlayer RX

// ── DFPlayer BUSY-Signal (ab Hardware 2v0) ────────────────────
// LOW = Wiedergabe läuft, HIGH = Pause/Idle (aktiv getrieben, kein Pull-up nötig).
// GPIO34 ist input-only, daher INPUT statt INPUT_PULLUP in pinMode().
const uint8_t DFPLAYER_BUSY = 34;                                              // GPIO34 ← DFPlayer BUSY (Pin 16)

// ── Touch-Sensor ─────────────────────────────────────────────
#define TOUCH_DROP      150                                                    // Mindest-Absenkung zur Touch-Erkennung (kalibrieren, ca. 50 % des Differenzwerts)
#define TOUCH_POLL_MS    50                                                    // Abtastrate des touchTask in ms
#define TOUCH_HOLD_MS   750                                                    // Haltezeit bis HOLD-Zustand und erster Wiederholungs-Event
#define TOUCH_REPEAT_MS 250                                                    // Wiederholrate im REPEAT-Zustand
#define TOUCH_RECAL_MS  600000UL                                               // Baseline-Rekalibrierung Intervall (10 min, nur wenn TS_IDLE)
#define TOUCH_MAX_HOLD_MS 30000UL                                              // 20v12 (E1-Fix, Audit 2026-08-13): erzwungener Reset auf TS_IDLE + Baseline-Neuerfassung, wenn ein Pad laenger als 30 s ununterbrochen "gedrueckt" bleibt (Feuchtigkeit/Kabeldefekt) – ohne diesen Deckel laeuft z.B. T4 (Lautstaerke) unbegrenzt weiter und zaehlt vol bis 0 herunter

// ── Eingabe-Event-IDs (inputQueue) ────────────────────────────
#define EVT_T0  0                                                              // Touch T0 – GPIO4
#define EVT_T2  1                                                              // Touch T2 – GPIO2
#define EVT_T3  2                                                              // Touch T3 – GPIO15
#define EVT_T4  3                                                              // Touch T4 – GPIO13
#define EVT_S1  4                                                              // Taster S1 – GPIO33 (20v28)
#define EVT_S2  5                                                              // Taster S2 – GPIO32 (20v28)
#define EVT_S3  6                                                              // Taster S3 – GPIO0

// ── Setup-Timeouts (ms) ──────────────────────────────────────
#define SETUP_WIFI_TIMEOUT_MS 30000                                            // max. Wartezeit auf WiFi-Verbindung
#define SETUP_NTP_TIMEOUT_MS  30000                                            // max. Wartezeit auf erste NTP-Synchronisation
#define SETUP_MP3_TIMEOUT_MS   5000                                            // max. Wartezeit auf DFPlayer Dateianzahl

// ── Diagnose ─────────────────────────────────────────────────
#define STACK_MON_INTERVAL_MS 60000UL                                          // Ausgabe-Intervall Stack-Überwachung (60 s)
// 20v08 (B1 sekundär, Audit 2026-08-13): Bei 30000/5000 gewinnt der
// Hardware-TWDT (15 s, WDT_HARDWARE_MS) bei jedem Freeze immer zuerst – der
// Software-Watchdog kommt nie zum Zug (siehe Audit-Befund B1). Der im Audit
// vorgeschlagene Wert 6000/1000 wurde bewusst NICHT übernommen: die
// Review-Notiz zu B1 verlangt, vorher den realen Worst-Case von
// verifyPlayStarted() nachzurechnen statt den Schätzwert zu übernehmen.
// Eigene Rechnung (worst case, dauerhaft nicht antwortender DFPlayer):
//   3× VERIFY_PLAY_RETRIES-Versuch in verifyPlayStarted(), je Versuch
//   VERIFY_PLAY_DELAY_MS (500 ms) + Mutex-Take bis 200 ms + readStateDrained()
//   mit bis zu SERIAL2_DRAIN_MAX_BYTES/DFPLAYER_RECEIVED_LENGTH (20) Zyklen
//   SERIAL2_FEEDBACK_GRACE_MS (100 ms) bei kontinuierlichem UART-Rauschen.
//   Deckt sich mit der im Audit selbst hergeleiteten Gesamtsumme von
//   "rund 5,5–6 s" für einen toten DFPlayer in einem alarmTask-Durchlauf
//   (siehe Audit, Abschnitt "Nicht bestätigt"). Bei WDG_TIMEOUT_MS=6000 wäre
//   die Marge zu diesem Worst-Case praktisch null – ein regulärer, nur
//   ungewöhnlich langsamer Alarmversuch könnte den Watchdog fälschlich als
//   Freeze werten (genau das "Fehlalarme erzeugen", vor dem die Review-Notiz
//   warnt). Gewählt: WDG_TIMEOUT_MS=10000/WDG_CHECK_MS=1000 → Erkennung
//   spätestens nach ~11 s: rund 5 s Marge über dem berechneten 6-s-Worst-Case
//   (legitimer Betrieb löst nicht fälschlich aus) und weiterhin rund 4 s
//   Marge unter den 15 s des TWDT (Software-Watchdog gewinnt jetzt wieder
//   zuerst, kann den 12v12-Freeze-Fallback also tatsächlich erreichen).
#define WDG_TIMEOUT_MS        10000UL                                          // Watchdog: maximale Zeit ohne Lebenszeichen (10 s, war 30 s)
#define WDG_CHECK_MS           1000UL                                          // Watchdog: Prüfintervall (1 s, war 5 s)
#define WDT_HARDWARE_MS       15000UL                                          // Hardware-TWDT: Timeout (15 s) – kürzer als WDG_TIMEOUT_MS

// ── Verzögerungskonstanten (ms) ───────────────────────────────
const uint32_t DISPLAY_UPDATE_MS    =  300;                                    // Zeitanzeige Seite 0
const uint32_t BTN_DEBOUNCE_MS      =   30;                                    // ISR-Entprellung: filtert Hardware-Prellen (typ. 5–50 ms)
const uint32_t BTN_LOCKOUT_MS       = 1000;                                    // Aktionssperre in inputTask: verhindert bewusste Doppeldrücke
const uint32_t CUCKOO_DURATION_MS   = 7500;                                    // Kuckuck-Laufzeit
const uint32_t AUTO_RETURN_MS       = 20000;                                   // Auto-Rückkehr zu Seite 0
const uint32_t DISPLAY_TIMEOUT_MS   = 300000UL;                                // OLED aus nach 5 min ohne Touch-Event
const uint32_t S2_TIMEOUT_MS        = 1800000UL;                               // 12v02: Licht/Mühlrad (Zugschalter S2) aus nach 30 min – analog AUTO_RETURN_MS
const uint32_t ALARM_POLL_MS        = 5000;                                    // Alarm-Nachlauf Prüfintervall
const uint32_t ALARM_MAX_RUN_MS     = 900000UL;                                // 20v05 (A3-Restarbeit, Audit 2026-08-13): harte Obergrenze fuer ALARM_RUNNING (15 min) – Rueckfallebene, falls BUSY dauerhaft LOW haengt und dfPlayerIdleDebounced() nie "idle" meldet
const uint8_t  ALARM_MIN_VOL        =   10;                                    // 20v12 (E1-Fix, Audit 2026-08-13): Lautstaerken-Untergrenze fuer triggerAlarm() – schuetzt gegen einen durch klemmendes Touch-Pad (T4) auf 0 heruntergezaehlten vol, ohne die persistierte Einstellung selbst zu veraendern
const uint16_t ALARM_CATCHUP_MIN    =   60;                                    // 20v04 (A1-Fix, Audit 2026-08-13): Nachholfenster in Minuten – faengt Zeitspruenge (NTP-Sync nach Stromausfall), Reboots im Alarmfenster und die Maerz-Zeitumstellung ab, siehe alarmDue() in Wecker_20v04.ino
const uint32_t VERIFY_PLAY_DELAY_MS =  500;                                    // 12v06: Start-Check – Wartezeit je Versuch (DFPlayer braucht Zeit zum Laden)
const uint8_t  VERIFY_PLAY_RETRIES  =    3;                                    // 12v06: Start-Check – Versuche (1 initial + 2 Retries), Reset spätestens nach 1500 ms
const uint8_t  ALARM_MAX_RESTARTS   =    3;                                    // 12v10: max. ESP.restart()-Versuche je Alarm, danach Abbruch statt Endlos-Neustart (10 → 3)
const uint16_t SERIAL2_DRAIN_MAX_BYTES = 200;                                  // 12v11: Obergrenze für Puffer-Drain-Schleifen – verhindert Endlosschleife bei dauerhaftem UART-Rauschen (floatende RX-Leitung bei getrenntem/defektem DFPlayer)
const uint32_t SERIAL2_FEEDBACK_GRACE_MS = 100;                                // 13v00: readStateDrained() – Gnadenfrist nach dem letzten empfangenen Frame, bis eine echte DFPlayerFeedBack-Antwort eintrifft (DFRobotDFPlayerMini::readState() gibt bei JEDER anderen Frame-Art, z.B. unaufgeforderter PlayFinished-Meldung, sofort -1 zurück, ohne weiter zu warten)
const uint32_t WIFI_RECONNECT_MS    = 3000;                                    // WiFi-Reconnect Wiederholrate
const uint32_t NVR_COMMIT_DELAY_MS  = 2000;                                    // 11v00: Ruhezeit nach letztem Event vor NVR-Commit (Flash-Wear-Schutz)

// ── NVR-Zugriffsmodus ────────────────────────────────────────
const bool ReadWrite = false;                                                  // Preferences: Lesen + Schreiben
const bool ReadOnly  = true;                                                   // Preferences: nur Lesen

// ── Taster-Pins ──────────────────────────────────────────────
const uint8_t S1 = 33;                                                         // 20v28: GPIO33 (vormals GPIO32) – Alarm aus / Kuckuck einmalig
const uint8_t S2 = 32;                                                         // 20v28: GPIO32 (vormals GPIO33) – Zugschalter Licht + Mühlrad
const uint8_t S3 = 0;                                                          // GPIO0  – Info-Seite ein/aus

// ── Ausgangs-Pins ────────────────────────────────────────────
const uint8_t E1 = 25;                                                         // GPIO25 – Kuckuck (digital, MOSFET)
const uint8_t E2 = 26;                                                         // GPIO26 – Mühlrad / DC-Motor 3 V (PWM via LEDC, MOSFET + Freilaufdiode 1N4448, Rail über MCP1700T-3302E/TO auf 3,3V geregelt)
const uint8_t E3 = 27;                                                         // GPIO27 – LED-Streifen Licht (digital, MOSFET + 47 Ω Vorwiderstand High-Side)

// ── Motor-PWM (E2 / GPIO26) ───────────────────────────────────
// 12v00: DC-Motor 3 V an 5 V-Versorgung → PWM mit 60 % Duty ≙ ~3 V Mittelwert.
// 20 kHz liegt über der Hörschwelle → kein Surren; 8-Bit-Auflösung reicht.
// 12v03: MOTOR_PWM_DUTY ist nur noch der Default-Sollwert beim ersten Boot –
//        der wirksame Wert liegt in der Laufzeit-Variable motor_duty, ist
//        über den Web-Slider (/motor) zur Laufzeit verstellbar und wird in
//        NVS persistiert. Kickstart: bei Sollwert < MOTOR_PWM_KICK_THRESHOLD
//        läuft der 3-V-Motor evtl. nicht aus dem Stand an → kurzer Vollgas-
//        Impuls (MOTOR_PWM_KICK_DUTY für MOTOR_PWM_KICK_MS), dann Sollwert.
// 20v20: LDO MCP1700T-3302E/TO regelt die Rail vor dem MOSFET jetzt fest auf
//        3,3V – PWM schaltet diese geregelte Spannung, nicht mehr die rohen
//        5V. Duty 255 bedeutet damit ~3,3V statt vormals 5V: voller Regel-
//        umfang 0..255 ist ohne Überspannungsrisiko für den 3-V-Motor nutzbar,
//        auch der Kickstart-Vollgasimpuls bleibt innerhalb der Rail-Spannung.
#define MOTOR_PWM_FREQ 20000UL                                                 // 20 kHz Trägerfrequenz (über Hörschwelle)
#define MOTOR_PWM_RES      8                                                   // 8-Bit Auflösung → Duty-Bereich 0..255
#define MOTOR_PWM_DUTY   153                                                   // Default-Sollwert beim ersten Boot: ~60 % Duty ≙ ~2 V aus geregelten 3,3V
#define MOTOR_PWM_KICK_THRESHOLD  89                                           // < ~35 % Duty (89/255) → Anlauf-Kickstart nötig
#define MOTOR_PWM_KICK_DUTY      255                                           // Vollgas-Impuls beim Kickstart (≙ geregelte 3,3V, unkritisch)
#define MOTOR_PWM_KICK_MS        150                                           // Dauer des Kickstart-Impulses [ms]

// ── Stack-Größen (Bytes) ──────────────────────────────────────
// Angepasst auf Basis der Stack High-Water Marks aus stackMonTask.
// setup() verwendet diese Konstanten direkt – Änderungen hier wirken sofort.
#define STACK_TOUCH     2880                                                   // touchTask
#define STACK_ALARM     2524                                                   // alarmTask
#define STACK_WIFI      2240                                                   // wifiTask
#define STACK_NVR       2304                                                   // nvrTask
#define STACK_STACKMON  2912                                                   // stackMonTask
#define STACK_WATCHDOG  1344                                                   // watchdogTask
#define STACK_INPUT     2704                                                   // inputTask
#define STACK_DISPLAY   2276                                                   // displayTask
#define STACK_WEBLOG    4096                                                   // webLogTask (HTTP-Server benötigt mehr Stack)

// ── Web-Logger ────────────────────────────────────────────────
#define WEBLOG_PORT      8080                                                  // HTTP-Port des Log-Servers (8080 ≠ 80 des WiFi-Konfigurators)
#define WEBLOG_LINES       40                                                  // Anzahl Zeilen auf der Seite
#define WEBLOG_LINE_LEN   128                                                  // maximale Zeichenanzahl je Zeile
#define WEBLOG_TAG_WIDTH   12                                                  // [xxx]-Tag im "Allgemeines Log" auf feste Spaltenbreite auffüllen
