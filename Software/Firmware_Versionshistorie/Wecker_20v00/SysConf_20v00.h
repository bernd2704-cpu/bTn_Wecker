#pragma once
// SysConf_20v00.h – Konfigurationskonstanten für bTn Wecker
// Firmware-Version : 20v00
// Datei-Version    : 20v00
// Boardverwalter   : esp32 3.3.11 von Espressif Systems
// Änderungshistorie: siehe CHANGELOG.md
// 20v00: Basis 13v00, Hardware ab 2v0 (DFPlayer BUSY-Signal an GPIO34)

// ── Firmware-Version ─────────────────────────────────────────
#define FW_VERSION "20v00"                                                     // Versionsnummer (als String in PGMInfo, Web-Log, WEB.h)

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

// ── Eingabe-Event-IDs (inputQueue) ────────────────────────────
#define EVT_T0  0                                                              // Touch T0 – GPIO4
#define EVT_T2  1                                                              // Touch T2 – GPIO2
#define EVT_T3  2                                                              // Touch T3 – GPIO15
#define EVT_T4  3                                                              // Touch T4 – GPIO13
#define EVT_S1  4                                                              // Taster S1 – GPIO32
#define EVT_S2  5                                                              // Taster S2 – GPIO33
#define EVT_S3  6                                                              // Taster S3 – GPIO0

// ── Setup-Timeouts (ms) ──────────────────────────────────────
#define SETUP_WIFI_TIMEOUT_MS 30000                                            // max. Wartezeit auf WiFi-Verbindung
#define SETUP_NTP_TIMEOUT_MS  30000                                            // max. Wartezeit auf erste NTP-Synchronisation
#define SETUP_MP3_TIMEOUT_MS   5000                                            // max. Wartezeit auf DFPlayer Dateianzahl

// ── Diagnose ─────────────────────────────────────────────────
#define STACK_MON_INTERVAL_MS 60000UL                                          // Ausgabe-Intervall Stack-Überwachung (60 s)
#define WDG_TIMEOUT_MS        30000UL                                          // Watchdog: maximale Zeit ohne Lebenszeichen (30 s)
#define WDG_CHECK_MS           5000UL                                          // Watchdog: Prüfintervall (5 s)
#define WDT_HARDWARE_MS       15000UL                                          // Hardware-TWDT: Timeout (15 s) – kürzer als WDG_TIMEOUT_MS

// ── Verzögerungskonstanten (ms) ───────────────────────────────
const uint32_t TOUCH_REPEAT_RATE_MS =  250;                                    // Touch-Wiederholrate
const uint32_t DISPLAY_UPDATE_MS    =  300;                                    // Zeitanzeige Seite 0
const uint32_t BTN_DEBOUNCE_MS      =   30;                                    // ISR-Entprellung: filtert Hardware-Prellen (typ. 5–50 ms)
const uint32_t BTN_LOCKOUT_MS       = 1000;                                    // Aktionssperre in inputTask: verhindert bewusste Doppeldrücke
const uint32_t CUCKOO_DURATION_MS   = 7500;                                    // Kuckuck-Laufzeit
const uint32_t AUTO_RETURN_MS       = 20000;                                   // Auto-Rückkehr zu Seite 0
const uint32_t DISPLAY_TIMEOUT_MS   = 300000UL;                                // OLED aus nach 5 min ohne Touch-Event
const uint32_t S2_TIMEOUT_MS        = 1800000UL;                               // 12v02: Licht/Mühlrad (Zugschalter S2) aus nach 30 min – analog AUTO_RETURN_MS
const uint32_t ALARM_POLL_MS        = 5000;                                    // Alarm-Nachlauf Prüfintervall
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
const uint8_t S1 = 32;                                                         // GPIO32 – Alarm aus / Kuckuck einmalig
const uint8_t S2 = 33;                                                         // GPIO33 – Zugschalter Licht + Mühlrad
const uint8_t S3 = 0;                                                          // GPIO0  – Info-Seite ein/aus

// ── Ausgangs-Pins ────────────────────────────────────────────
const uint8_t E1 = 25;                                                         // GPIO25 – Kuckuck (digital, MOSFET)
const uint8_t E2 = 26;                                                         // GPIO26 – Mühlrad / DC-Motor 3 V (PWM via LEDC, MOSFET + Freilaufdiode 1N4148)
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
#define MOTOR_PWM_FREQ 20000UL                                                 // 20 kHz Trägerfrequenz (über Hörschwelle)
#define MOTOR_PWM_RES      8                                                   // 8-Bit Auflösung → Duty-Bereich 0..255
#define MOTOR_PWM_DUTY   153                                                   // Default-Sollwert beim ersten Boot: ~60 % Duty ≙ 3 V aus 5 V
#define MOTOR_PWM_KICK_THRESHOLD  89                                           // < ~35 % Duty (89/255) → Anlauf-Kickstart nötig
#define MOTOR_PWM_KICK_DUTY      255                                           // Vollgas-Impuls beim Kickstart
#define MOTOR_PWM_KICK_MS        150                                           // Dauer des Kickstart-Impulses [ms]

// ── Stack-Größen (Bytes) ──────────────────────────────────────
// Angepasst auf Basis der Stack High-Water Marks aus stackMonTask.
// setup() verwendet diese Konstanten direkt – Änderungen hier wirken sofort.
#define STACK_TOUCH     2880                                                   // touchTask
#define STACK_ALARM     2128                                                   // alarmTask
#define STACK_WIFI      2240                                                   // wifiTask
#define STACK_NVR       2304                                                   // nvrTask
#define STACK_STACKMON  2912                                                   // stackMonTask
#define STACK_WATCHDOG  1344                                                   // watchdogTask
#define STACK_INPUT     2240                                                   // inputTask
#define STACK_DISPLAY   2176                                                   // displayTask
#define STACK_WEBLOG    4096                                                   // webLogTask (HTTP-Server benötigt mehr Stack)

// ── Web-Logger ────────────────────────────────────────────────
#define WEBLOG_PORT      8080                                                  // HTTP-Port des Log-Servers (8080 ≠ 80 des WiFi-Konfigurators)
#define WEBLOG_LINES       40                                                  // Anzahl Zeilen auf der Seite
#define WEBLOG_LINE_LEN   128                                                  // maximale Zeichenanzahl je Zeile
#define WEBLOG_TAG_WIDTH   12                                                  // [xxx]-Tag im "Allgemeines Log" auf feste Spaltenbreite auffüllen
