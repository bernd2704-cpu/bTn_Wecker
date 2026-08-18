// bTn Wecker mit OLED-Anzeige und MP3-Player
// Basis: bTn_Wecker_9v18 – FreeRTOS + State Machine + WiFi-Konfigurator
// Boardverwalter: esp32 3.3.11 von Espressif Systems
//
// ─── State Machines ──────────────────────────────────────────
//
//  UI-State-Machine  (inputTask / displayTask)
//  ┌──────────┐  T0   ┌──────────┐  T0   ┌──────────┐
//  │ UI_CLOCK │──────▶│UI_ALARM1 │──────▶│UI_ALARM2 │
//  └────▲─────┘       └──────────┘       └──────────┘
//    T0 │ S3(off)          T0 ▼               T0 ▼
//  ┌────┴─────┐       ┌──────────┐       ┌──────────┐
//  │ UI_FUNCS │◀──────│UI_SOUND2 │◀──────│UI_SOUND1 │
//  └──────────┘  T0   └──────────┘  T0   └──────────┘
//  T0 (von UI_FUNCS) ──────────────────────▶ UI_CUCKOO_TIME
//  T0 (von UI_CUCKOO_TIME) ──────────────▶ UI_CLOCK
//  S3 (beliebig) ─────────────────────────▶ UI_INFO
//  S3 (von INFO) ──────────────────────────▶ UI_CLOCK
//  T3 (von INFO) ──────────────────────────▶ WiFi-Konfigurator  (11v05)
//  T4 (von INFO) ──────────────────────────▶ Werksreset
//
//  Alarm-State-Machine  (alarmTask)
//  ALARM_IDLE ──── Alarmzeit erreicht ──▶ ALARM_RUNNING
//  ALARM_RUNNING ── MP3 beendet (ALARM_POLL_MS) ─▶ ALARM_IDLE
//
//  Kuckuck-State-Machine  (alarmTask)
//  CUCKOO_IDLE ──── t_min==0, cuckoo_on ──▶ CUCKOO_RUNNING
//  CUCKOO_RUNNING ── CUCKOO_DURATION_MS abgelaufen ────▶ CUCKOO_IDLE
//
// ─── Task-Architektur ────────────────────────────────────────
//  touchTask   Core 0  Pri 2  ESP-IDF touch_pad_* → inputQueue
//                             State Machine: TS_IDLE → TS_PRESSED → TS_REPEAT
//                             Exklusiv: nur ein Pad aktiv, andere gesperrt
//  alarmTask   Core 0  Pri 2  Alarm- + Kuckuck-State-Machine
//                             Core 0: physisch getrennt von inputTask → kein CPU-Scheduling-Konflikt
//  wifiTask    Core 0  Pri 1  WiFi-Reconnect
//  nvrTask     Core 0  Pri 1  Flash-Sicherung bei Änderung
//  stackMonTask Core 0 Pri 1  Stack-HWM + Heap-Snapshot für Web-Log
//  watchdogTask Core 0 Pri 1  Anwendungs-Watchdog: inputTask / displayTask / alarmTask
//  webLogTask   Core 0 Pri 1  HTTP-Server Port WEBLOG_PORT → Ring-Puffer als Web-Seite
//
//  Hardware-TWDT (ESP32): inputTask, displayTask, alarmTask abonniert.
//  Timeout WDT_HARDWARE_MS – hardware-basiert, unabhängig vom FreeRTOS-Scheduler.
//  Verhindert CPU-Lock durch hängenden Task auf Hardware-Ebene.
//  inputTask   Core 1  Pri 2  Dispatch → UI-State-Machine
//  displayTask Core 1  Pri 1  Zeitanzeige, Auto-Rückkehr
// ─────────────────────────────────────────────────────────────

// ── Bibliotheken ─────────────────────────────────────────────
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <esp_sntp.h>
#include <SSD1306Wire.h>
#include <DFRobotDFPlayerMini.h>
#include <Preferences.h>
#include <nvs_flash.h>
#include <driver/touch_pad.h>
#include <freertos/semphr.h>          // FreeRTOS Semaphore / Mutex API
#include <esp_task_wdt.h>             // ESP32 Hardware Task Watchdog Timer (TWDT)

// ── Konfiguration ────────────────────────────────────────────
#include "SysConf_20v11.h"                                                               // Pin-Belegung, Timing-Konstanten, Touch-Schwellwerte
#include "WEB.h"

const char PGMInfo[] = "bTn_Wecker_" FW_VERSION;                                          // PROGMEM-fähig; kein String-Heap-Fragment

// ── WiFi-Laufzeit-Zugangsdaten (aus NVR, ab 4v0) ─────────────
// Werden in loadWifiCredentials() gefüllt und danach in
// WiFi.begin() sowie wifiTask verwendet.
static char sta_ssid[33] = "";                                                           // max. 32 Zeichen + '\0'
static char sta_psk [64] = "";                                                           // max. 63 Zeichen + '\0'
// ── Touch-Task ───────────────────────────────────────────────

static const touch_pad_t TOUCH_PADS[4] = {
  TOUCH_PAD_NUM0,   // T0 – GPIO4
  TOUCH_PAD_NUM2,   // T2 – GPIO2
  TOUCH_PAD_NUM3,   // T3 – GPIO15
  TOUCH_PAD_NUM4    // T4 – GPIO13
};

// ── State-Machine-Typen ──────────────────────────────────────
// Enum-Werte 0–7 entsprechen direkt den menu()-Seitennummern.
enum UiState : uint8_t {
  UI_CLOCK       = 0,   // Seite 0: Zeitanzeige
  UI_ALARM1      = 1,   // Seite 1: Alarm 1 einstellen
  UI_ALARM2      = 2,   // Seite 2: Alarm 2 einstellen
  UI_SOUND1      = 3,   // Seite 3: Sound 1 wählen
  UI_SOUND2      = 4,   // Seite 4: Sound 2 wählen
  UI_FUNCS       = 5,   // Seite 5: Funktionen wählen
  UI_CUCKOO_TIME = 6,   // Seite 6: Kuckuck-Aktivzeit einstellen
  UI_INFO        = 7    // Seite 7: Info (nur via S3 erreichbar)
};

enum AlarmState  : uint8_t { ALARM_IDLE,  ALARM_RUNNING  };
enum CuckooState : uint8_t { CUCKOO_IDLE, CUCKOO_RUNNING };

// Touch-Task State Machine
// TS_IDLE    – kein Touch aktiv
// TS_PRESSED – Touch erkannt, wartet auf HOLD-Schwelle (750 ms)
// TS_REPEAT  – HOLD erreicht, sendet EVT alle TOUCH_REPEAT_MS (250 ms)
enum TouchState  : uint8_t { TS_IDLE, TS_PRESSED, TS_REPEAT };

volatile UiState     uiState     = UI_CLOCK;
volatile AlarmState  alarmState  = ALARM_IDLE;
volatile CuckooState cuckooState = CUCKOO_IDLE;
// 20v09 (C3-Fix): true, solange ALARM_RUNNING ohne bestätigten Ton läuft
// (verifyPlayStarted() lieferte PLAY_NO_SOUND – Modul lebt, Datei startet
// aber nicht). In diesem Fall darf playerStatus==0 in runAlarmMachine() den
// Alarm NICHT beenden (das wäre sofort beim ersten Poll der Fall) – einziger
// Ausstieg ist dann ALARM_MAX_RUN_MS (A3) oder S1. Cross-core wie alarmState.
volatile bool alarmSilentFallback = false;

// ── FreeRTOS Objekte ─────────────────────────────────────────
static QueueHandle_t     inputQueue   = nullptr;
static SemaphoreHandle_t displayMutex = nullptr;                                         // Mutex: exklusiver Display-Zugriff
static SemaphoreHandle_t playerMutex  = nullptr;                                         // Mutex: exklusiver DFPlayer-Zugriff (thread-safe Serial2)
static SemaphoreHandle_t nvrSemaphore = nullptr;

// Task-Handles – werden in setup() befüllt, von stackMonTask gelesen
static TaskHandle_t hTouchTask   = nullptr;
static TaskHandle_t hWifiTask    = nullptr;
static TaskHandle_t hNvrTask     = nullptr;
static TaskHandle_t hInputTask   = nullptr;
static TaskHandle_t hDisplayTask = nullptr;
static TaskHandle_t hAlarmTask      = nullptr; // Handle für stackMonTask-Abfrage
static TaskHandle_t hWatchdogTask   = nullptr; // Handle für watchdogTask
static TaskHandle_t hStackMonTask   = nullptr; // 9v14: HWM-Abfrage von außerhalb
static TaskHandle_t hWebLogTask     = nullptr; // 9v14: HWM-Abfrage von außerhalb

// ── Hardware ─────────────────────────────────────────────────
SSD1306Wire         display(0x3C, SDA, SCL, GEOMETRY_128_64);
DFRobotDFPlayerMini player;
Preferences         data;

// ── Zeit / Datum ─────────────────────────────────────────────
time_t  now;
tm      timeinfo;
char    datum[11];
char    zeit[9];
char    datum_sync[9];
char    zeit_sync[9];
char    datum_WiFi[9];
char    zeit_WiFi[9];

// NTP-Callback schreibt in tmp-Puffer + setzt Flag.
// displayTask überträgt unter displayMutex in datum_sync/zeit_sync.
// Verhindert Race Condition zwischen SNTP-Task und menu(7).
static char          datum_sync_tmp[9];
static char          zeit_sync_tmp[9];
static volatile bool ntpSyncPending  = false; // true: NTP-Callback hat neue Daten, displayTask überträgt
static char          datum_WiFi_tmp[9];        // Double-Buffer: wifiTask schreibt nur bei !wifiSyncPending (Core 0)
static char          zeit_WiFi_tmp[9];         // displayTask überträgt unter displayMutex nach datum_WiFi/zeit_WiFi
static volatile bool wifiSyncPending = false;  // 11v00: dient zugleich als Schreibsperre für wifiTask (verhindert Torn-Read)
volatile uint8_t t_hour;
volatile uint8_t t_min;
volatile uint8_t t_sec;
// 9v14: t_sec_alt wanderte in displayTask als static – nur dieser Task
// liest/schreibt; file-scope suggerierte fälschlich Mehr-Task-Zugriff.

// ── Alarm ────────────────────────────────────────────────────
bool    a1_on   = true;
uint8_t a1_hour = 6;
uint8_t a1_min  = 0;
char    str_a1[6];
bool    a2_on   = true;
uint8_t a2_hour = 6;
uint8_t a2_min  = 0;
char    str_a2[6];

volatile uint32_t t_start4 = 0;
volatile uint32_t lastTouchMs = 0;                                                       // Zeitstempel letzter Touch-/Taster-Event (EVT_T0–T4, EVT_S3)
static volatile bool displayBlanked = false;                                             // 10v00: true wenn OLED nach DISPLAY_TIMEOUT_MS abgeschaltet wurde
volatile uint32_t t_start6 = 0;
volatile uint32_t alarmRunStart = 0;                                                     // 20v05 (A3-Restarbeit): Startzeitpunkt von ALARM_RUNNING – im Gegensatz zu t_start6 (Poll-Timer, wird bei jedem Poll zurückgesetzt) unverändert bis zum Alarmende, Basis für ALARM_MAX_RUN_MS
         uint32_t t_start7 = 0;
volatile uint32_t t_start_S2 = 0;                                                        // 12v02: Einschaltzeitpunkt Licht/Mühlrad via Zugschalter S2

// ── Sound ────────────────────────────────────────────────────
bool    sound1_on   = false;
uint8_t sound1_selected      = 1;
char    str_s1[4];
uint8_t sound1_assigned = 1;
char    str_s1_play[4];
bool    sound2_on   = false;
uint8_t sound2_selected      = 1;
char    str_s2[4];
uint8_t sound2_assigned = 1;
char    str_s2_play[4];
uint8_t vol         = 9;
uint8_t MAX_VOL     = 25;
char    str_vol[3];
int16_t playerStatus = 0;      // nur Core 0: alarmTask schreibt, alarmTask liest – kein volatile nötig
int16_t mp3Count    = 0;
char    str_mp3[4];
uint32_t resetCount = 0;
char     str_reset[5];                                                                   // "nnnn" + null (max 9999 Resets sichtbar)

volatile bool safeChange = false;
// 11v00: Zeitstempel des letzten Events, das safeChange gesetzt hat.
// inputTask gibt nvrSemaphore erst frei, wenn seit diesem Zeitpunkt
// mind. NVR_COMMIT_DELAY_MS ohne weiteres Event vergangen sind.
// Schützt Flash vor Writes bei gehaltener Einstelltaste (Touch-REPEAT).
static volatile uint32_t safeChangeMs = 0;

// 11v00: Markiert eine persistierbare Änderung – Timestamp wird bei jedem
// neuen Event aktualisiert, sodass der Debounce-Zeitraum neu beginnt.
static inline void markSafeChange() {
  safeChangeMs = millis();
  safeChange   = true;
}

// ── Taster Toggle-Status ─────────────────────────────────────
volatile bool S2_SW = false;                                                             // Toggle-Status Zugschalter

// ── Funktion-Vorwahl ─────────────────────────────────────────
bool    cuckoo_on      = false;
uint8_t cuckoo_onTime  = 6;             // erste Stunde (von hh), Default 06:00
uint8_t cuckoo_offTime = 22;            // letzte Stunde (bis hh), Default 22:00
char    str_cot[3];                     // "hh" von-Zeit
char    str_coff[3];                    // "hh" bis-Zeit
bool    light_on   = true;
bool    wheel_on   = false;

// 12v03: Mühlrad-Motor-Pulsweite zur Laufzeit verstellbar (Web-Slider /motor),
// NVS-persistiert. motor_duty = wirksamer LEDC-Sollwert (0..255), Default aus
// SysConf. motorRunning = läuft der Motor gerade? → Live-Übernahme einer
// Slider-Änderung ohne Neustart. Beide volatile: webLogTask (Core 0) schreibt,
// alarmTask/inputTask lesen – 8-/32-bit-Zugriff auf Xtensa atomar.
volatile uint8_t motor_duty   = MOTOR_PWM_DUTY;
volatile bool    motorRunning = false;

// pageselect: spiegelt (uint8_t)uiState – wird von checkboxAlarm/Sound genutzt
volatile uint8_t pageselect = 0;

// ── Taster-Debounce ──────────────────────────────────────────
// Zwei-Stufen-Debouncing:
//   isrBtnMs[]  – ISR-Ebene:      filtert Hardware-Prellen (BTN_DEBOUNCE_MS =  30 ms)
//   lastBtnMs[] – Task-Ebene:     Aktionssperre            (BTN_LOCKOUT_MS   = 1000 ms)
static volatile uint32_t isrBtnMs[3] = {};  // IRAM-zugänglich: von ISR gelesen/geschrieben
static uint32_t          lastBtnMs[3] = {}; // von inputTask gelesen/geschrieben

// ── Anwendungs-Watchdog Alive-Timestamps ─────────────────────
// Jeder überwachte Task setzt seinen Wert in jedem Zyklus.
// watchdogTask prüft alle WDG_CHECK_MS ob der Wert jünger als WDG_TIMEOUT_MS ist.
static volatile uint32_t wdg_inputTask   = 0; // gesetzt von inputTask   (alle ~50 ms)
static volatile uint32_t wdg_displayTask = 0; // gesetzt von displayTask (alle DISPLAY_UPDATE_MS)
static volatile uint32_t wdg_alarmTask   = 0; // gesetzt von alarmTask   (alle 500 ms)




// =============================================================
//  Web-Logger  (ab 9v1)
//
//  webLog(msg) ersetzt Serial.*-Ausgaben nach WiFi-Connect.
//  Schreibt in einen Ring-Puffer (WEBLOG_LINES Einträge).
//  webLogTask startet einen HTTP-Server auf Port WEBLOG_PORT.
//  Browser ruft / auf → HTML-Seite mit Auto-Refresh alle 20 s.
//  /log liefert den aktuellen Pufferinhalt als plain text.
//  webLogReady-Flag: webLog() puffert erst wenn Task gestartet.
// =============================================================

static SemaphoreHandle_t webLogMutex  = nullptr;          // schützt den Ring-Puffer

// Ring-Puffer
static char   webLogBuf[WEBLOG_LINES][WEBLOG_LINE_LEN];   // Zeilen-Array
static uint16_t webLogHead = 0;                            // nächste Schreibposition
static uint16_t webLogCount = 0;                           // bisher eingetragene Zeilen

// ── Snapshots: jeweils letzter Wert mit Timestamp ─────────────
// Werden unter webLogMutex geschrieben und in der Web-Seite
// als dedizierte Sektionen angezeigt (nicht im Ring-Puffer).
#define SNAP_BUF_LEN  480
static char snapTouchBuf[SNAP_BUF_LEN] = "(noch keine Daten)";
static char snapTouchTime[20]          = "";
static char snapStackBuf[SNAP_BUF_LEN] = "(noch keine Daten)";
static char snapStackTime[20]          = "";
static char snapNtpTime[20]            = "";
static char snapAlarmTime[20]          = "";              // letzter erfolgreich gestarteter Alarm (siehe triggerAlarm)

// Schreibt eine Nachricht in den Ring-Puffer (thread-safe).
// Bleibt still wenn Mutex noch nicht initialisiert.
void webLog(const char* msg) {
  if (!webLogMutex) return;
  if (xSemaphoreTake(webLogMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    snprintf(webLogBuf[webLogHead], WEBLOG_LINE_LEN, "%s", msg);
    webLogHead  = (webLogHead + 1) % WEBLOG_LINES;        // Überlauf: älteste Zeile überschreiben
    if (webLogCount < WEBLOG_LINES) webLogCount++;
    xSemaphoreGive(webLogMutex);
  }
}

// Printf-Variante für komfortablen Aufruf
void webLogf(const char* fmt, ...) {
  char buf[WEBLOG_LINE_LEN];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  webLog(buf);
}

// Schreibt den aktuellen Zeitstempel in buf (NTP-Uhrzeit oder Uptime)
static void snapTimeStr(char* buf, size_t len) {
  time_t t = time(nullptr);
  if (t > 1700000000UL) {
    struct tm tm_val;
    localtime_r(&t, &tm_val);
    strftime(buf, len, "%d.%m.%Y %H:%M:%S", &tm_val);
  } else {
    snprintf(buf, len, "+%lus", millis() / 1000UL);
  }
}

// 20v04 (A5-Fix, Audit 2026-08-13): Ohne gültige Systemzeit (kein NTP-Sync,
// Systemzeit auf Epoch 0 seit Boot) darf weder Alarm noch Kuckuck auslösen –
// die pegelbasierte Fälligkeitsprüfung (siehe alarmDue()) würde auf der
// 1970-Uhr sonst innerhalb weniger Stunden nach Boot feuern. Gate steht daher
// vor jeder Alarm-/Kuckuck-Auswertung in alarmTask, nicht nur als Zusatz.
static inline bool timeValid() { return time(nullptr) > 1700000000UL; }

// Zählt seit Boot, wie oft beim Start eines DFPlayer-Befehls noch
// unerwartete Bytes im Serial2-Empfangspuffer standen (Hinweis auf
// verspätete/verlorene Antworten bzw. UART-Desync). Vor jedem player.*-
// Aufruf zu rufen, der ein Kommando an den DFPlayer sendet.
// serial2LeftoverCount zählt JEDE Abfrage mit Restbytes (unabhängig vom Log);
// serial2LeftoverLastLogged hält den zuletzt GEMELDETEN Wert, damit
// webLogf() nur bei einer Änderung der Restbyte-Anzahl schreibt. Bleibt
// dabei über Nullstände hinweg erhalten (kein Reset bei avail==0): sonst
// würde z.B. während ALARM_RUNNING derselbe wiederkehrende Restbyte-Wert
// (Poll → Restbytes → gedraint auf 0 → nächster Poll dieselbe Anzahl) bei
// jedem Zyklus erneut als "neue" Änderung gemeldet, nur weil der Puffer
// dazwischen kurz leer war.
static uint32_t serial2LeftoverCount      = 0;
static int      serial2LeftoverLastLogged = -1;                                          // -1 = noch nichts gemeldet

static void checkSerial2Leftover(const char* label) {
  int avail = Serial2.available();
  if (avail > 0) {
    serial2LeftoverCount++;
    if (avail != serial2LeftoverLastLogged) {
      serial2LeftoverLastLogged = avail;
      char ts[20];
      snapTimeStr(ts, sizeof(ts));
      webLogf("[DFPlayer] Serial2 Restbytes vor %s: %d (seit Boot: %lu, %s)",
              label, avail, (unsigned long)serial2LeftoverCount, ts);
    }
  }
}

// 20v00: Direkter GPIO-Status des DFPlayer BUSY-Pins (ab Hardware 2v0), ohne
// Serial2/playerMutex – LOW = Wiedergabe läuft, HIGH = Pause/Idle. Kein Ersatz
// für readStateDrained() (liefert keinen Grund/Quelle wie Play-Finished), aber
// als schnelle, blockierungsfreie Zusatzabfrage nutzbar (z.B. Anzeige/Polling).
static bool dfPlayerBusy() {
  return digitalRead(DFPLAYER_BUSY) == LOW;
}

// 20v03: Mehrfach-Sampling für sicherheitskritische Stellen (ALARM_RUNNING
// beendet den Alarm bei einer einzigen Fehlmessung unwiderruflich – Motor/
// Licht aus, State-Reset). Trotz RC-Filter (100µs Zeitkonstante, siehe
// Hardware/Schaltplan/DFPlayer-BUSY.md) bleibt ein Rest-Risiko für Störimpulse,
// die die Filter-Zeitkonstante überdauern. Erst wenn 3 Abtastungen im Abstand
// von je 5 ms übereinstimmend "idle" (HIGH) melden, gilt der Player als
// wirklich nicht mehr beschäftigt – ein einzelner durchschlagender Störimpuls
// kann so keinen vorzeitigen Alarm-Abbruch mehr auslösen.
static bool dfPlayerIdleDebounced() {
  for (uint8_t i = 0; i < 3; i++) {
    if (dfPlayerBusy()) { return false; }
    vTaskDelay(pdMS_TO_TICKS(5));                                                        // außerhalb Mutex – Projektregel eingehalten
  }
  return true;
}

// 20v11 (C2-Fix, Audit 2026-08-13): gemeinsame fortschrittsbasierte Drain-
// Schleife für drainSerial2Pre() und readStateDrained() (s.u.). Bricht ab,
// sobald ein player.read() den Rohpuffer NICHT verkleinert hat (Bytes noch
// unvollständig, mitten im Empfang eines Frames) – anders als der bisherige
// Abbruch am Rückgabewert von player.available(), der auch nach einem
// VOLLSTÄNDIG konsumierten 0x41-ACK-Frame false liefert (parseStack() der
// Bibliothek behandelt den ACK intern, ohne _isAvailable zu setzen). Bei
// Pufferinhalt [ACK][Feedback] brach die alte Version dadurch nach dem ACK
// ab und ließ den Feedback-Frame stehen – genau der Fall, der in
// readStateDrained() (unten) ein Altframe als Antwort auf eine neue Abfrage
// vortäuschen konnte (Motor/Licht liefen, obwohl kein Ton spielte).
// Endlosschleife ausgeschlossen (Review-Notiz 2026-08-14): der äußere
// Schleifenkopf verlangt bereits Serial2.available() >= DFPLAYER_RECEIVED_
// LENGTH, ein unvollständiger Frame kann die Schleife also gar nicht erst
// betreten; SERIAL2_DRAIN_MAX_BYTES bleibt zusätzlich als harte Grenze.
static void drainSerial2Progress() {
  uint16_t drained = 0;
  while (Serial2.available() >= DFPLAYER_RECEIVED_LENGTH && drained < SERIAL2_DRAIN_MAX_BYTES) {
    int before = Serial2.available();
    player.available();
    player.read();
    if (Serial2.available() >= before) { break; }                                        // kein Fortschritt: unvollstaendiger Frame, stehen lassen
    drained += DFPLAYER_RECEIVED_LENGTH;
  }
}

// 13v00: player.readState() (DFRobotDFPlayerMini) sendet die 0x42-Abfrage und
// wertet dann NUR den erstbesten eintreffenden Frame aus – ist dieser nicht
// vom Typ DFPlayerFeedBack (z.B. eine unaufgefordert gesendete Play-Finished-
// Meldung 0x3C/0x3D, noch vom vorherigen Titel), gibt die Funktion sofort -1
// zurück, OHNE weiter auf die tatsächliche Antwort zu warten. Der 0x41-ACK-
// Frame selbst ist davon nicht betroffen – parseStack() der Bibliothek
// verschluckt ihn bereits intern, ohne _isAvailable zu setzen (kein Faktor
// hier, unabhängig vom ACK-Modus). Die echte Antwort auf die eigene Abfrage
// trifft in diesem Fall oft nur wenige ms später ein, wurde bisher aber
// verpasst, wenn sie zum Prüfzeitpunkt noch nicht im Puffer lag (Diagnose vom
// 10.08.2026: "kein Start-Status nach playFolder" trotz aktivem Alarm).
// readStateDrained() behebt das: nach einem -1 von readState() wird noch bis
// zu SERIAL2_FEEDBACK_GRACE_MS lang aktiv auf einen Feedback-Frame gewartet
// (Timer wird bei jedem empfangenen Störframe zurückgesetzt), statt sofort
// aufzugeben. Trifft bereits vor Ablauf eine gültige Antwort ein oder liegt
// schon ein gültiges st vor, wird nicht unnötig weitergewartet.
// 20v11 (C2-Fix): drainSerial2Progress() VOR der eigenen 0x42-Abfrage –
// verhindert, dass ein bereits vor diesem Aufruf im Puffer liegender Altframe
// als Antwort auf die JETZT gesendete Abfrage gewertet wird.
static int16_t readStateDrained() {
  drainSerial2Progress();
  int16_t st = (int16_t)player.readState();
  uint32_t graceStart = millis();
  uint16_t drained = 0;
  while (drained < SERIAL2_DRAIN_MAX_BYTES) {                                              // 12v11: Obergrenze – nie unbegrenzt schleifen (Watchdog-Schutz)
    if (player.available()) {
      uint8_t frameType = player.readType();
      if (frameType == DFPlayerFeedBack) {
        st = (int16_t)player.read();                                                       // neuester Feedback-Wert gewinnt, ältere werden verworfen
      } else if (frameType == DFPlayerError) {
        // 20v09 (C3-Fix, Audit 2026-08-13): bisher wurde jeder Nicht-Feedback-
        // Frame kommentarlos verworfen – ein Error-Frame mit dem echten
        // Fehlercode (z.B. "Datei nicht gefunden") ging damit spurlos
        // unter. Jetzt geloggt, macht "Datei fehlt" von "Modul abgestürzt"
        // im Web-Log unterscheidbar (das BUSY-Signal allein kann das nicht,
        // siehe Review-Notiz zu C3).
        webLogf("[DFPlayer] Error-Frame empfangen: Code %d", (int)player.read());
      } else {
        player.read();                                                                     // Störframe (z.B. Play-Finished) verwerfen, weiter warten
      }
      drained += DFPLAYER_RECEIVED_LENGTH;
      graceStart = millis();                                                               // nach jedem Frame: Gnadenfrist erneut gewähren
      continue;
    }
    if (st != -1) { break; }                                                               // gültige Antwort liegt schon vor – kein weiteres Warten nötig
    if (millis() - graceStart >= SERIAL2_FEEDBACK_GRACE_MS) { break; }                      // Gnadenfrist abgelaufen, wirklich nichts mehr zu erwarten
  }
  return st;
}

// 12v19: drainSerial2Pre() räumt den Puffer VOR dem Senden eines Befehls ab
// (analog zum bisherigen Inline-Drain in triggerAlarm(), siehe dort für die
// Begründung des Bibliotheks-Drains statt roher Serial2.read()-Bytes). Bisher
// war dieser Vorab-Drain nur für den zeitkritischen Alarm-playFolder() aktiv;
// volume()/stop()/Testsound-playFolder() liefen ungeschützt, wodurch dort
// entstehende Restframes (z.B. ACK je Befehl) unbemerkt liegen blieben, bis
// ein zufälliger späterer readStateDrained()-Aufruf sie mit abräumte – und
// im ungünstigen Fall genau während der Alarm-Verifikation noch vorhanden
// waren. Muss VOR dem jeweiligen player.*-Befehl, unter gehaltenem
// playerMutex, aufgerufen werden.
static void drainSerial2Pre(const char* label) {
  checkSerial2Leftover(label);
  drainSerial2Progress();                                                                  // 20v11 (C2-Fix): siehe drainSerial2Progress() oben
}

// Aktualisiert den Touch-Baseline-Snapshot (thread-safe)
static void updateSnapTouch(const uint16_t* baseline) {
  char tmp[SNAP_BUF_LEN];
  int pos = 0;
  for (int i = 0; i < 4 && pos < (int)sizeof(tmp) - 1; i++) {
    uint16_t thr = (baseline[i] > TOUCH_DROP)
                 ? baseline[i] - TOUCH_DROP
                 : baseline[i] - baseline[i] / 5;
    pos += snprintf(tmp + pos, sizeof(tmp) - pos,
                    "  Pad %d  Baseline: %u  Threshold: %u\n", i, baseline[i], thr);
  }
  if (pos > 0 && tmp[pos - 1] == '\n') tmp[pos - 1] = '\0';
  if (webLogMutex && xSemaphoreTake(webLogMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    strncpy(snapTouchBuf, tmp, SNAP_BUF_LEN - 1);
    snapTouchBuf[SNAP_BUF_LEN - 1] = '\0';
    snapTimeStr(snapTouchTime, sizeof(snapTouchTime));
    xSemaphoreGive(webLogMutex);
  }
}

// Aktualisiert den Stack-HWM-Snapshot (thread-safe)
static void updateSnapStack() {
  char tmp[SNAP_BUF_LEN];
  int pos = 0;
  pos += snprintf(tmp + pos, sizeof(tmp) - pos,
    "  touchTask   : %4u\n", uxTaskGetStackHighWaterMark(hTouchTask));
  pos += snprintf(tmp + pos, sizeof(tmp) - pos,
    "  wifiTask    : %4u\n", uxTaskGetStackHighWaterMark(hWifiTask));
  pos += snprintf(tmp + pos, sizeof(tmp) - pos,
    "  nvrTask     : %4u\n", uxTaskGetStackHighWaterMark(hNvrTask));
  pos += snprintf(tmp + pos, sizeof(tmp) - pos,
    "  inputTask   : %4u\n", uxTaskGetStackHighWaterMark(hInputTask));
  pos += snprintf(tmp + pos, sizeof(tmp) - pos,
    "  displayTask : %4u\n", uxTaskGetStackHighWaterMark(hDisplayTask));
  pos += snprintf(tmp + pos, sizeof(tmp) - pos,
    "  alarmTask   : %4u\n", uxTaskGetStackHighWaterMark(hAlarmTask));
  pos += snprintf(tmp + pos, sizeof(tmp) - pos,
    "  watchdogTask: %4u\n", uxTaskGetStackHighWaterMark(hWatchdogTask));
  pos += snprintf(tmp + pos, sizeof(tmp) - pos,
    "  stackMonTask: %4u\n", uxTaskGetStackHighWaterMark(hStackMonTask));  // 9v14: Handle statt nullptr (identisch, aber konsistent)
  pos += snprintf(tmp + pos, sizeof(tmp) - pos,
    "  webLogTask  : %4u\n", uxTaskGetStackHighWaterMark(hWebLogTask));    // 9v14: neu mitgemessen
  pos += snprintf(tmp + pos, sizeof(tmp) - pos,
    "  Freier Heap : %u Bytes", esp_get_free_heap_size());
  if (webLogMutex && xSemaphoreTake(webLogMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    strncpy(snapStackBuf, tmp, SNAP_BUF_LEN - 1);
    snapStackBuf[SNAP_BUF_LEN - 1] = '\0';
    snapTimeStr(snapStackTime, sizeof(snapStackTime));
    xSemaphoreGive(webLogMutex);
  }
}

// =============================================================
//  Hilfsfunktionen
// =============================================================

void bTn_info() {
  Serial.println("\n----------------------------------------");
  Serial.println(PGMInfo);
}

bool delayFunction(uint32_t lastTime, uint32_t actualDelay) {
  return (millis() - lastTime >= actualDelay);
}

void showTime() {
  time(&now);
  localtime_r(&now, &timeinfo);
  snprintf(datum, sizeof(datum), "%02u.%02u.%04u", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
  snprintf(zeit,  sizeof(zeit),  "%02u:%02u:%02u",  timeinfo.tm_hour, timeinfo.tm_min,    timeinfo.tm_sec);
  t_sec  = timeinfo.tm_sec;
  t_min  = timeinfo.tm_min;
  t_hour = timeinfo.tm_hour;
}

// NTP-Synchronisations-Callback (wird vom SNTP-Task aufgerufen, nicht vom
// Haupt-Task). Schreibt nur in thread-lokale tmp-Puffer und setzt ein Flag.
// displayTask überträgt die Daten sicher unter displayMutex.
// 9v12: lokale tm-Struktur verwenden (statt globaler timeinfo) – vermeidet
// Race Condition mit showTime() im displayTask, das dieselben globalen
// Variablen ohne Sync-Bezug zum SNTP-Task beschreibt.
void timeavailable(struct timeval *t) {
  time_t    now_local;
  struct tm tm_local;
  time(&now_local);
  localtime_r(&now_local, &tm_local);
  snprintf(datum_sync_tmp, sizeof(datum_sync_tmp), "%04u%02u%02u", tm_local.tm_year + 1900, tm_local.tm_mon + 1, tm_local.tm_mday);
  snprintf(zeit_sync_tmp,  sizeof(zeit_sync_tmp),  "%02u:%02u:%02u", tm_local.tm_hour, tm_local.tm_min, tm_local.tm_sec);
  ntpSyncPending = true;                                                                   // displayTask überträgt unter Mutex
}



// =============================================================
//  Display-Hilfsfunktionen
//  Unverändert. Alle Aufrufe nur unter displayMutex (außer setup()).
// =============================================================

void cleanTXT(uint8_t xPos, uint8_t yPos, uint8_t dx, uint8_t dy) {
  display.setColor(BLACK);
  display.fillRect(xPos, yPos, dx, dy);
  display.setColor(WHITE);
}

void zeigeZ10C(uint8_t xPos, uint8_t yPos, const char* TXT) {
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(xPos, yPos, TXT);
}

void zeigeZ10L(uint8_t xPos, uint8_t yPos, const char* TXT) {
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(xPos, yPos, TXT);
}

void zeigeZ10R(uint8_t xPos, uint8_t yPos, const char* TXT) {
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(xPos, yPos, TXT);
}

void zeigeZ16C(uint8_t xPos, uint8_t yPos, const char* TXT) {
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(xPos, yPos, TXT);
}

void zeigeZ16L(uint8_t xPos, uint8_t yPos, const char* TXT) {
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);
  display.drawString(xPos, yPos, TXT);
}

// Alarm-Checkboxen (nutzt pageselect – wird von uiTransition synchron gesetzt)
void checkboxAlarm() {
  display.setColor(BLACK);
  display.fillRect(67, 37, 10, 10);
  display.fillRect(67, 54, 10, 10);
  display.setColor(WHITE);
  switch (pageselect) {
    case 0:
      display.drawRect(67, 37, 10, 10); if (a1_on) { display.fillRect(70, 40, 4, 4); }
      display.drawRect(67, 54, 10, 10); if (a2_on) { display.fillRect(70, 57, 4, 4); }
      break;
    case 1:
      display.drawRect(67, 37, 10, 10); if (a1_on) { display.fillRect(70, 40, 4, 4); }
      break;
    case 2:
      display.drawRect(67, 54, 10, 10); if (a2_on) { display.fillRect(70, 57, 4, 4); }
      break;
  }
  display.display();                                                                   // einmaliger Flush nach allen Zeichenoperationen
}

void checkboxSound() {
  display.setColor(BLACK);
  display.fillRect(67, 37, 10, 10);
  display.fillRect(67, 54, 10, 10);
  display.setColor(WHITE);
  switch (pageselect) {
    case 3:
      if (sound1_on) {
        display.drawRect(67, 37, 10, 10);
        display.fillRect(70, 40, 4, 4);
        display.display();                                                               // Checkbox anzeigen bevor Audio startet
        sound1_assigned = sound1_selected;
        if (xSemaphoreTake(playerMutex, pdMS_TO_TICKS(50)) == pdTRUE) {                  // 50 ms < 100 ms displayMutex-Timeout
          drainSerial2Pre("playFolder (Sound1 an)");
          player.playFolder(1, sound1_assigned);
          xSemaphoreGive(playerMutex);
        }
      } else {
        display.drawRect(67, 37, 10, 10);
        display.display();                                                               // Checkbox anzeigen bevor Player gestoppt
        if (xSemaphoreTake(playerMutex, pdMS_TO_TICKS(50)) == pdTRUE) {                  // 50 ms < 100 ms displayMutex-Timeout
          drainSerial2Pre("stop (Sound1 aus)");
          player.stop();
          xSemaphoreGive(playerMutex);
        }
      }
      break;
    case 4:
      if (sound2_on) {
        display.drawRect(67, 54, 10, 10);
        display.fillRect(70, 57, 4, 4);
        display.display();                                                               // Checkbox anzeigen bevor Audio startet
        sound2_assigned = sound2_selected;
        if (xSemaphoreTake(playerMutex, pdMS_TO_TICKS(50)) == pdTRUE) {                  // 50 ms < 100 ms displayMutex-Timeout
          drainSerial2Pre("playFolder (Sound2 an)");
          player.playFolder(1, sound2_assigned);
          xSemaphoreGive(playerMutex);
        }
      } else {
        display.drawRect(67, 54, 10, 10);
        display.display();                                                               // Checkbox anzeigen bevor Player gestoppt
        if (xSemaphoreTake(playerMutex, pdMS_TO_TICKS(50)) == pdTRUE) {                  // 50 ms < 100 ms displayMutex-Timeout
          drainSerial2Pre("stop (Sound2 aus)");
          player.stop();
          xSemaphoreGive(playerMutex);
        }
      }
      break;
  }
}

void checkboxFunction() {
  display.setColor(BLACK);
  display.fillRect(32, 20, 10, 10);
  display.fillRect(32, 37, 10, 10);
  display.fillRect(32, 54, 10, 10);
  display.setColor(WHITE);
  display.drawRect(32, 20, 10, 10); if (cuckoo_on) { display.fillRect(35, 23, 4, 4); }
  display.drawRect(32, 37, 10, 10); if (light_on)  { display.fillRect(35, 40, 4, 4); }
  display.drawRect(32, 54, 10, 10); if (wheel_on)  { display.fillRect(35, 57, 4, 4); }
  display.display();                                                                   // einmaliger Flush nach allen Zeichenoperationen
}

void menu(uint8_t page) {   // uint8_t: Koordinatenbereich 0–7 entspricht UiState-Werten
  switch (page) {
    case 0:
      display.clear();
      zeigeZ16C(64, 0,  zeit);
      zeigeZ10L(1,  17, datum);
      zeigeZ10L(77, 17, "Volume");
      zeigeZ10L(115,17, str_vol);
      zeigeZ16C(30, 32, "Alarm 1");
      zeigeZ16C(30, 49, "Alarm 2");
      zeigeZ16C(105,32, str_a1);
      zeigeZ16C(105,49, str_a2);
      checkboxAlarm();
      break;
    case 1:
      display.clear();
      zeigeZ16C(64, 0,  "Alarm einstellen");
      zeigeZ16C(5,  32, "\x3E");
      zeigeZ16C(15, 32, "\x3E");
      zeigeZ16C(40, 32, "A1");
      zeigeZ16C(40, 49, "A2");
      zeigeZ16C(105,32, str_a1);
      zeigeZ16C(105,49, str_a2);
      checkboxAlarm();
      break;
    case 2:
      cleanTXT(0, 32, 25, 15);
      zeigeZ16C(5,  49, "\x3E");
      zeigeZ16C(15, 49, "\x3E");
      zeigeZ16C(105,49, str_a2);
      checkboxAlarm();
      break;
    case 3:
      display.clear();
      zeigeZ16C(64, 0,  "Sound wählen");
      zeigeZ10L(0,  17, "Alarm 1:");
      zeigeZ10L(66, 17, "Alarm 2:");
      zeigeZ10L(42, 17, str_s1_play);
      zeigeZ10L(108,17, str_s2_play);
      zeigeZ16C(5,  32, "\x3E");
      zeigeZ16C(15, 32, "\x3E");
      zeigeZ16C(40, 32, "A1");
      zeigeZ16C(40, 49, "A2");
      zeigeZ16C(105,32, str_s1);
      zeigeZ16C(105,49, str_s2);
      sound1_on = false;
      sound2_on = false;
      checkboxSound();
      break;
    case 4:
      cleanTXT(108, 17, 20, 10);
      zeigeZ10L(108, 17, str_s2_play);
      cleanTXT(0,   32, 25, 15);
      zeigeZ16C(5,  49, "\x3E");
      zeigeZ16C(15, 49, "\x3E");
      zeigeZ16C(105,49, str_s2);
      checkboxSound();
      break;
    case 5:
      if (xSemaphoreTake(playerMutex, pdMS_TO_TICKS(50)) == pdTRUE) {                   // 50 ms < 100 ms displayMutex-Timeout
        drainSerial2Pre("stop (Funktionswahl)");
        player.stop();
        xSemaphoreGive(playerMutex);
      }
      display.clear();
      zeigeZ10C(64, 0,  "Funktion wählen");
      zeigeZ16C(5,  15, "\x3E");
      zeigeZ16C(15, 15, "\x3E");
      zeigeZ16L(50, 15, "Kuckuck");
      zeigeZ16C(5,  32, "\x3E");
      zeigeZ16C(15, 32, "\x3E");
      zeigeZ16L(50, 32, "Licht");
      zeigeZ16C(5,  49, "\x3E");
      zeigeZ16C(15, 49, "\x3E");
      zeigeZ16L(50, 49, "Mühlrad");
      checkboxFunction();
      break;
    case 6:
      display.clear();
      zeigeZ16C(64,  0, "Kuckuck aktiv");
      zeigeZ10L(1,  36, "Taste +");
      zeigeZ16C(60, 32, "von");
      zeigeZ16C(91, 32, str_cot);
      zeigeZ16L(100, 32, ":00");
      zeigeZ10L(1,  52, "Taste -");
      zeigeZ16C(60, 49, "bis");
      zeigeZ16C(91, 49, str_coff);
      zeigeZ16L(100, 49, ":00");
      break;
    case 7:
      display.clear();
      zeigeZ10C(63, 0, PGMInfo);
      { char webLogUrl[24];
        snprintf(webLogUrl, sizeof(webLogUrl), "%s:%u",
                 WiFi.localIP().toString().c_str(), (unsigned)WEBLOG_PORT);
        zeigeZ10C(63, 16, webLogUrl); }    // Z2: Web-Log-Adresse
      zeigeZ10L(1,  28, "MP3");             // Z3: MP3-Dateianzahl + Reset-Zähler
      zeigeZ10L(28, 28, str_mp3);
      zeigeZ10C(78, 28, "RESET");
      zeigeZ10R(127,28, str_reset);                                                      // rechtsbündig
      zeigeZ10L(1,   40, "Taste +");                                                    // Z4: T3 löst WLAN-Konfig aus
      zeigeZ10R(127, 40, "WiFi RESET");
      zeigeZ10L(1,   54, "Taste -");                                                    // Z5: T4 löst Werksreset aus
      zeigeZ10R(127, 54, "Werks-RESET");
      break;
  }
  display.display();                                                                     // einmaliger Flush nach vollständiger Seitenzeichnung
}



// =============================================================
//  NVR (Flash-Persistenz)
// =============================================================

void writeNVR() {
  data.putBool("a1_on",       a1_on);
  data.putInt ("a1_hour",     a1_hour);
  data.putInt ("a1_min",      a1_min);
  data.putBool("a2_on",       a2_on);
  data.putInt ("a2_hour",     a2_hour);
  data.putInt ("a2_min",      a2_min);
  data.putInt ("sound1_assigned", sound1_assigned);
  data.putInt ("sound2_assigned", sound2_assigned);
  data.putBool("cuckoo_on",   cuckoo_on);
  data.putInt ("cuckoo_on_h",  cuckoo_onTime);
  data.putInt ("cuckoo_off_h", cuckoo_offTime);
  data.putBool("light_on",    light_on);
  data.putBool("wheel_on",    wheel_on);
  data.putInt ("motor_duty",  motor_duty);              // 12v03: Web-verstellbare Pulsweite
  data.putInt ("vol",         vol);
}

void readNVR() {
  a1_on       = data.getBool("a1_on",       a1_on);
  a1_hour     = data.getInt ("a1_hour",      a1_hour);
  a1_min      = data.getInt ("a1_min",       a1_min);
  a2_on       = data.getBool("a2_on",        a2_on);
  a2_hour     = data.getInt ("a2_hour",      a2_hour);
  a2_min      = data.getInt ("a2_min",       a2_min);
  sound1_assigned = data.getInt ("sound1_assigned",  sound1_assigned);
  sound2_assigned = data.getInt ("sound2_assigned",  sound2_assigned);
  cuckoo_on      = data.getBool("cuckoo_on",    cuckoo_on);
  cuckoo_onTime  = data.getInt ("cuckoo_on_h",  cuckoo_onTime);
  cuckoo_offTime = data.getInt ("cuckoo_off_h", cuckoo_offTime);
  light_on    = data.getBool("light_on",     light_on);
  wheel_on    = data.getBool("wheel_on",     wheel_on);
  motor_duty  = data.getInt ("motor_duty",   motor_duty);
  vol         = data.getInt ("vol",          vol);

  // ── Wertebereich-Clamp: korrupte NVS-Daten abfangen ─────────
  // Fehlerhafter NVS-Inhalt (z.B. nach Flash-Fehler oder Versionsänderung)
  // könnte ohne Clamp zu falschen Display-Strings oder DFPlayer-Fehlern führen.
  a1_hour        = min(a1_hour,        (uint8_t)23);   // Stunde 0–23
  a1_min         = min(a1_min,         (uint8_t)59);   // Minute 0–59
  a2_hour        = min(a2_hour,        (uint8_t)23);
  a2_min         = min(a2_min,         (uint8_t)59);
  cuckoo_onTime  = min(cuckoo_onTime,  (uint8_t)23);   // Von-Stunde 0–23
  cuckoo_offTime = min(cuckoo_offTime, (uint8_t)23);   // Bis-Stunde 0–23
  if (sound1_assigned < 1) sound1_assigned = 1;                // DFPlayer: Dateinummer min. 1
  if (sound2_assigned < 1) sound2_assigned = 1;                // Obergrenze erst nach mp3Count bekannt
  if (vol > MAX_VOL)   vol = MAX_VOL;                  // Lautstärke 0–MAX_VOL
  // motor_duty ist uint8_t → Wertebereich 0..255 bereits durch Typ erzwungen
  // (korrupter NVS-Wert wird beim Zuweisen auf 8 Bit beschnitten, bleibt gültig).
}

// 9v14: Reset-Zähler in eigener Funktion – readNVR() ist jetzt seiteneffektfrei.
// 11v03: öffnet NVR-Namespace selbst (begin/end), wird erst NACH loadWifiCredentials()
//        aufgerufen – so zählt ein Werksreset-Folgeboot in den WiFi-Konfigurator nicht mit.
void bumpResetCount() {
  // 20v07 (D1-Fix, Audit 2026-08-13): Rückgabewert von data.begin() prüfen –
  // schlägt das Öffnen fehl, bleibt resetCount unverändert statt auf 0
  // zurückzufallen (getUInt() auf einem nicht gestarteten Handle liefert
  // sonst nur den mitgegebenen Default).
  if (!data.begin("varSafe", ReadWrite)) {
    webLog("[FEHLER] NVR: data.begin() in bumpResetCount() fehlgeschlagen");
    return;
  }
  resetCount = data.getUInt("resetCount", 0);
  resetCount++;                                                                          // Neustart zählen
  data.putUInt("resetCount", resetCount);
  data.end();
}



// =============================================================
//  WiFi-Konfigurator  (ab 4v0)
//
//  loadWifiCredentials()
//    Liest SSID und PSK aus NVR-Namespace "wifiCfg".
//    Gibt true zurück wenn gültige Daten vorliegen (SSID ≥ 1 Zeichen).
//
//  runWifiConfigServer()
//    Wird beim ersten Start (kein "wifiCfg"-Eintrag) oder auf
//    Anforderung (T3 auf Info-Seite, 11v05) aufgerufen – VOR dem Start
//    der FreeRTOS-Tasks, da er die Arduino-loop-Ebene blockiert.
//    ESP32 öffnet Access Point WIFI_AP_SSID ("bTn-Wecker"),
//    startet WebServer auf Port 80.
//    Nach erfolgreicher Eingabe: SSID+PSK in NVR speichern → ESP.restart().
// =============================================================

// ── NVR-Zugangsdaten laden ───────────────────────────────────
// Gibt true zurück wenn SSID vorhanden (min. 1 Zeichen).
bool loadWifiCredentials() {
  Preferences wifiPrefs;
  wifiPrefs.begin("wifiCfg", ReadOnly);
  bool valid = wifiPrefs.getBool("valid", false);
  if (valid) {
    wifiPrefs.getString("ssid", sta_ssid, sizeof(sta_ssid));
    wifiPrefs.getString("psk",  sta_psk,  sizeof(sta_psk));
  }
  wifiPrefs.end();
  return valid && (strlen(sta_ssid) > 0);
}

// ── NVR-Zugangsdaten löschen (erzwingt Config-Mode beim nächsten Start) ──
void clearWifiCredentials() {
  Preferences wifiPrefs;
  wifiPrefs.begin("wifiCfg", ReadWrite);
  wifiPrefs.putBool("valid", false);
  wifiPrefs.end();
}

// ── Serverseitige Validierung ────────────────────────────────
// SSID: 1–32 Zeichen; PSK: leer (offen) oder 8–63 Zeichen.
// Gibt leeren String bei Erfolg, sonst Fehlermeldung zurück.
static String validateWifiInput(const String& ssid, const String& psk) {
  if (ssid.length() < 1 || ssid.length() > 32)
    return "SSID ungueltig: 1-32 Zeichen erforderlich.";
  if (psk.length() > 0 && psk.length() < 8)
    return "Passwort ungueltig: Leer lassen oder 8-63 Zeichen.";
  if (psk.length() > 63)
    return "Passwort zu lang (max. 63 Zeichen).";
  return "";
}

// ── WiFi-Konfigurations-Webserver ───────────────────────────
// Blockiert bis Daten gespeichert wurden, dann ESP.restart().
void runWifiConfigServer() {
  Serial.println("\n[WiFi-Config] Starte Access Point: " WIFI_AP_SSID);

  // OLED: Hinweis anzeigen
  display.clear();
  zeigeZ10C(64,  2, PGMInfo);
  zeigeZ10C(64, 16, "WiFi Einrichtung");
  zeigeZ10C(64, 28, "WLAN: " WIFI_AP_SSID);
  zeigeZ10C(64, 40, "Browser oeffnen:");
  zeigeZ10C(64, 52, "192.168.4.1");
  display.display();

  // Access Point starten (kein Passwort → offenes AP-Netz)
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_AP_SSID, nullptr, WIFI_AP_CHANNEL);
  Serial.print("[WiFi-Config] AP-IP: ");
  Serial.println(WiFi.softAPIP());

  WebServer server(80);

  // GET / → Konfigurationsseite (aus WEB.h, PROGMEM)
  server.on("/", HTTP_GET, [&server]() {
    server.send_P(200, "text/html", WIFI_CONFIG_PAGE);
  });

  // POST /save → Daten prüfen und speichern
  server.on("/save", HTTP_POST, [&server]() {
    String ssid = server.arg("ssid");
    String psk  = server.arg("psk");
    ssid.trim();
    // PSK NICHT trimmen: Passwörter können Leerzeichen enthalten

    // Serverseitige Validierung
    String err = validateWifiInput(ssid, psk);
    if (err.length() > 0) {
      Serial.println("[WiFi-Config] Validierungsfehler: " + err);
      server.send(200, "text/html", wifiErrorRedirect(err.c_str()));
      return;
    }

    // In NVR speichern
    Preferences wifiPrefs;
    wifiPrefs.begin("wifiCfg", ReadWrite);
    wifiPrefs.putString("ssid", ssid);
    wifiPrefs.putString("psk",  psk);
    wifiPrefs.putBool  ("valid", true);
    wifiPrefs.end();

    Serial.println("[WiFi-Config] Gespeichert: SSID=" + ssid);

    // Erfolgsseite senden (aus WEB.h, PROGMEM)
    server.send_P(200, "text/html", WIFI_SUCCESS_PAGE);

    // Kurze Pause damit Browser die Seite empfangen kann.
    // delay() ist hier korrekt: runWifiConfigServer() läuft VOR Task-Start,
    // kein FreeRTOS-Scheduler aktiv – vTaskDelay() wäre hier nicht verfügbar.
    delay(3500);

    // OLED: Neustart-Meldung
    display.clear();
    zeigeZ16C(64, 22, "Gespeichert!");
    zeigeZ16C(64, 42, "Neustart ...");
    display.display();
    delay(500);
    ESP.restart();
  });

  // Alle anderen Pfade → zurück zur Hauptseite
  server.onNotFound([&server]() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });

  server.begin();
  Serial.println("[WiFi-Config] HTTP-Server gestartet, warte auf Eingabe ...");

  // Blockiere bis POST /save verarbeitet wurde
  while (true) {
    server.handleClient();
    delay(5);                 // delay() korrekt: vor Task-Start, kein Scheduler aktiv; verhindert WDT-Reset
  }
}



// =============================================================
//  UI-State-Machine
//
//  uiTransition(next)  –  Zustandswechsel + Bildschirm zeichnen
//  uiDispatch(evt)     –  Event → richtiger State-Handler
//
//  Globale Events (T0, S1, S2, S3) werden vor dem per-State-
//  Handler behandelt. T2/T3/T4 sind State-spezifisch.
// =============================================================

// ── Zustandswechsel ──────────────────────────────────────────
// Muss unter displayMutex aufgerufen werden.
void uiTransition(UiState next) {
  uiState    = next;
  pageselect = (uint8_t)next;                                                            // checkboxAlarm/Sound nutzen pageselect
  menu(pageselect);                                                                      // Bildschirm zeichnen (Entry-Aktion)
}

// ── Per-State-Handler: T2/T3/T4 ─────────────────────────────
// Jeder Handler gibt den Folge-Zustand zurück.
// Bleibt der Zustand gleich, wird uiTransition NICHT aufgerufen
// (kein unnötiges Neuzeichnen; nur partielle Display-Updates).

// — UI_CLOCK: Lautstärke mit T3/T4 ──────────────────────────
static UiState onClock(uint8_t evt) {
  switch (evt) {
    case EVT_T3:                                                                         // Lautstärke +
      if (vol < MAX_VOL) {
        vol++;
        if (xSemaphoreTake(playerMutex, pdMS_TO_TICKS(50)) == pdTRUE) {                 // 50 ms < 100 ms displayMutex-Timeout
          drainSerial2Pre("volume+");
          player.volume(vol);
          xSemaphoreGive(playerMutex);
        }
        snprintf(str_vol, sizeof(str_vol), "%02u", vol);
        cleanTXT(115, 17, 15, 10);
        zeigeZ10L(115, 17, str_vol);
        display.display();                                                               // partielles Update übertragen
        markSafeChange();
      }
      break;
    case EVT_T4:                                                                         // Lautstärke –
      if (vol > 0) {
        vol--;
        if (xSemaphoreTake(playerMutex, pdMS_TO_TICKS(50)) == pdTRUE) {                 // 50 ms < 100 ms displayMutex-Timeout
          drainSerial2Pre("volume-");
          player.volume(vol);
          xSemaphoreGive(playerMutex);
        }
        snprintf(str_vol, sizeof(str_vol), "%02u", vol);
        cleanTXT(115, 17, 15, 10);
        zeigeZ10L(115, 17, str_vol);
        display.display();                                                               // partielles Update übertragen
        markSafeChange();
      }
      break;
  }
  return UI_CLOCK;
}

// — UI_ALARM1: Alarm 1 Ein/Aus, Stunde+, Minute+ ─────────────
static UiState onAlarm1(uint8_t evt) {
  switch (evt) {
    case EVT_T2:
      a1_on = !a1_on;
      checkboxAlarm();
      markSafeChange();
      break;
    case EVT_T3:                                                                         // Stunde +
      if (a1_hour < 23) { a1_hour++; } else { a1_hour = 0; }
      snprintf(str_a1, sizeof(str_a1), "%02u:%02u", a1_hour, a1_min);
      cleanTXT(82, 34, 46, 13);
      zeigeZ16C(105, 32, str_a1);
      display.display();                                                                 // partielles Update übertragen
      markSafeChange();
      break;
    case EVT_T4:                                                                         // Minute +
      if (a1_min < 59) { a1_min++; } else { a1_min = 0; }
      snprintf(str_a1, sizeof(str_a1), "%02u:%02u", a1_hour, a1_min);
      cleanTXT(82, 34, 46, 13);
      zeigeZ16C(105, 32, str_a1);
      display.display();                                                                 // partielles Update übertragen
      markSafeChange();
      break;
  }
  return UI_ALARM1;
}

// — UI_ALARM2: Alarm 2 Ein/Aus, Stunde+, Minute+ ─────────────
static UiState onAlarm2(uint8_t evt) {
  switch (evt) {
    case EVT_T2:
      a2_on = !a2_on;
      checkboxAlarm();
      markSafeChange();
      break;
    case EVT_T3:                                                                         // Stunde +
      if (a2_hour < 23) { a2_hour++; } else { a2_hour = 0; }
      snprintf(str_a2, sizeof(str_a2), "%02u:%02u", a2_hour, a2_min);
      cleanTXT(82, 51, 46, 13);                                                          // A2-Zeile (Y=49) – war fälschlich 34 (A1-Zeile)
      zeigeZ16C(105, 49, str_a2);
      display.display();                                                                 // partielles Update übertragen
      markSafeChange();
      break;
    case EVT_T4:                                                                         // Minute +
      if (a2_min < 59) { a2_min++; } else { a2_min = 0; }
      snprintf(str_a2, sizeof(str_a2), "%02u:%02u", a2_hour, a2_min);
      cleanTXT(82, 51, 46, 13);
      zeigeZ16C(105, 49, str_a2);
      display.display();                                                                 // partielles Update übertragen
      markSafeChange();
      break;
  }
  return UI_ALARM2;
}

// — UI_SOUND1: Sound 1 OK/+/– ────────────────────────────────
static UiState onSound1(uint8_t evt) {
  switch (evt) {
    case EVT_T2:                                                                         // Sound 1 OK (Vorschau)
      sound1_on = !sound1_on;
      checkboxSound();
      if (sound1_on) {
        snprintf(str_s1_play, sizeof(str_s1_play), "%03u", sound1_selected);
        cleanTXT(42, 17, 20, 10);
        zeigeZ10L(42, 17, str_s1_play);
        display.display();                                                               // partielles Update übertragen
      }
      break;
    case EVT_T3:                                                                         // Sound 1 +
      if (sound1_selected < mp3Count) { sound1_selected++; } else { sound1_selected = 1; }
      snprintf(str_s1, sizeof(str_s1), "%03u", sound1_selected);
      cleanTXT(82, 34, 46, 13);
      zeigeZ16C(105, 32, str_s1);
      sound1_on = false;
      checkboxSound();
      markSafeChange();
      break;
    case EVT_T4:                                                                         // Sound 1 –
      // 9v13: Fallback auf 1 wenn mp3Count == 0 – vermeidet ungültige
      // Dateinummer 0 an DFPlayer (kann beim Boot vor readFileCounts auftreten).
      if (sound1_selected > 1) { sound1_selected--; }
      else                     { sound1_selected = (mp3Count >= 1) ? mp3Count : 1; }
      snprintf(str_s1, sizeof(str_s1), "%03u", sound1_selected);
      cleanTXT(82, 34, 46, 13);
      zeigeZ16C(105, 32, str_s1);
      sound1_on = false;
      checkboxSound();
      markSafeChange();
      break;
  }
  return UI_SOUND1;
}

// — UI_SOUND2: Sound 2 OK/+/– ────────────────────────────────
static UiState onSound2(uint8_t evt) {
  switch (evt) {
    case EVT_T2:                                                                         // Sound 2 OK (Vorschau)
      sound2_on = !sound2_on;
      checkboxSound();
      if (sound2_on) {
        snprintf(str_s2_play, sizeof(str_s2_play), "%03u", sound2_selected);
        cleanTXT(108, 17, 20, 10);
        zeigeZ10L(108, 17, str_s2_play);
        display.display();                                                               // partielles Update übertragen
      }
      break;
    case EVT_T3:                                                                         // Sound 2 +
      if (sound2_selected < mp3Count) { sound2_selected++; } else { sound2_selected = 1; }
      snprintf(str_s2, sizeof(str_s2), "%03u", sound2_selected);
      cleanTXT(82, 51, 46, 13);                                                          // S2-Zeile (Y=49)
      zeigeZ16C(105, 49, str_s2);                                                        // S2-Zeile – war fälschlich 32 (S1-Zeile)
      sound2_on = false;
      checkboxSound();
      markSafeChange();
      break;
    case EVT_T4:                                                                         // Sound 2 –
      // 9v13: Fallback auf 1 wenn mp3Count == 0 (analog Sound 1)
      if (sound2_selected > 1) { sound2_selected--; }
      else                     { sound2_selected = (mp3Count >= 1) ? mp3Count : 1; }
      snprintf(str_s2, sizeof(str_s2), "%03u", sound2_selected);
      cleanTXT(82, 51, 46, 13);
      zeigeZ16C(105, 49, str_s2);
      sound2_on = false;
      checkboxSound();
      markSafeChange();
      break;
  }
  return UI_SOUND2;
}

// — UI_FUNCS: Kuckuck / Licht / Mühlrad toggle ───────────────
static UiState onFuncs(uint8_t evt) {
  switch (evt) {
    case EVT_T2:
      cuckoo_on = !cuckoo_on;
      checkboxFunction();
      markSafeChange();
      break;
    case EVT_T3:
      light_on = !light_on;
      checkboxFunction();
      markSafeChange();
      break;
    case EVT_T4:
      wheel_on = !wheel_on;
      checkboxFunction();
      markSafeChange();
      break;
  }
  return UI_FUNCS;
}

// — UI_CUCKOO_TIME: T3 → von hh +, T4 → bis hh + ───────────
static UiState onCuckooTime(uint8_t evt) {
  switch (evt) {
    case EVT_T3:                                                                         // von hh +
      if (cuckoo_onTime < 23) { cuckoo_onTime++; } else { cuckoo_onTime = 0; }
      snprintf(str_cot, sizeof(str_cot), "%02u", cuckoo_onTime);
      cleanTXT(78, 35, 23, 13);
      zeigeZ16C(91, 32, str_cot);
      display.display();                                                                 // partielles Update übertragen
      markSafeChange();
      break;
    case EVT_T4:                                                                         // bis hh +
      if (cuckoo_offTime < 23) { cuckoo_offTime++; } else { cuckoo_offTime = 0; }
      snprintf(str_coff, sizeof(str_coff), "%02u", cuckoo_offTime);
      cleanTXT(78, 52, 23, 13);
      zeigeZ16C(91, 49, str_coff);
      display.display();                                                                 // partielles Update übertragen
      markSafeChange();
      break;
  }
  return UI_CUCKOO_TIME;
}

// Flag: wird von onInfo() gesetzt; inputTask wertet es NACH xSemaphoreGive aus.
// So liegt kein delay() unter displayMutex.
static volatile bool wifiConfigRequested  = false;
static volatile bool factoryResetRequested = false;  // T4 auf UI_INFO → NVS löschen + Neustart

// — UI_INFO: T3 → WiFi-Konfig-Modus (11v05, vorher T0); T4 → Werksreset; andere ignoriert ──
static UiState onInfo(uint8_t evt) {
  if (evt == EVT_T3) {
    clearWifiCredentials();                                                               // NVR-Flag löschen
    wifiConfigRequested = true;                                                          // inputTask führt Neustart durch
  }
  if (evt == EVT_T4) {
    factoryResetRequested = true;                                                        // inputTask: NVS löschen + Neustart
  }
  return UI_INFO;                                                                        // Nur S3 verlässt Info (T3/T4 lösen Neustart)
}

// ── Haupt-Dispatcher ─────────────────────────────────────────
// Behandelt globale Events, dann per-State-Handler.
// Gibt den Folge-Zustand zurück; Aufrufer ruft uiTransition wenn nötig.
static UiState uiDispatch(UiState s, uint8_t evt) {

  // ── T0: Seitenwechsel (globaler Zyklus, State-unabhängig) ──
  // Ausnahme: T0 auf UI_INFO wird ignoriert (cycle[] hat nur 7 Einträge,
  // UI_INFO=7 wäre Out-of-Bounds; seit 11v05 ist T0 auf INFO zudem ohne Funktion,
  // der WLAN-Reset liegt nun auf T3 – siehe onInfo()).
  if (evt == EVT_T0 && s != UI_INFO) {
    static const UiState cycle[] = {
      UI_ALARM1,       // von UI_CLOCK
      UI_ALARM2,       // von UI_ALARM1
      UI_SOUND1,       // von UI_ALARM2
      UI_SOUND2,       // von UI_SOUND1
      UI_FUNCS,        // von UI_SOUND2
      UI_CUCKOO_TIME,  // von UI_FUNCS
      UI_CLOCK,        // von UI_CUCKOO_TIME
    };
    return cycle[(uint8_t)s];
  }

  // ── S3: Info-Seite ein/aus (global, mit Taster-Entprellung) ─
  if (evt == EVT_S3) {
    uint32_t t_now = millis();
    if (t_now - lastBtnMs[2] >= BTN_LOCKOUT_MS) {
      lastBtnMs[2] = t_now;
      return (s == UI_INFO) ? UI_CLOCK : UI_INFO;                                       // Toggle Info
    }
    return s;
  }

  // ── T2/T3/T4: State-spezifisch ──────────────────────────────
  switch (s) {
    case UI_CLOCK:  return onClock (evt);
    case UI_ALARM1: return onAlarm1(evt);
    case UI_ALARM2: return onAlarm2(evt);
    case UI_SOUND1: return onSound1(evt);
    case UI_SOUND2: return onSound2(evt);
    case UI_FUNCS:       return onFuncs      (evt);
    case UI_CUCKOO_TIME: return onCuckooTime(evt);
    case UI_INFO:        return onInfo      (evt);
    default:        return s;
  }
}



// =============================================================
//  System-State-Machines  (werden von alarmTask aufgerufen)
// =============================================================

static uint16_t lastA1Day = 0xFFFF;  // Tages-Sperre Alarm 1 (tm_yday) – file-scope: auch vom manuellen Abbruch lesbar
static uint16_t lastA2Day = 0xFFFF;

// 20v04 (A1-Fix, Audit 2026-08-13): pegel- statt flankenbasierte, tagesbezogene
// Fälligkeitsprüfung mit begrenztem Nachholfenster (ALARM_CATCHUP_MIN). Fängt
// Zeitsprünge (NTP-Sync nach Stromausfall), Reboots im Alarmfenster und die
// März-Zeitumstellung ab – anders als die vorherige sec==0-Flanke, die jedes
// verpasste Ereignis ersatzlos gelöscht hat. triggerAlarm() setzt lastA*Day
// erst NACH erfolgreichem Start; ein fehlgeschlagener Mutex-Take heilt sich
// dadurch im nächsten 500-ms-Tick von selbst (solange das Fenster nicht
// abgelaufen ist).
static bool alarmDue(bool on, uint8_t h, uint8_t m,
                      uint8_t hour, uint8_t min, uint16_t yday, uint16_t lastDay) {
  if (!on || yday == lastDay) { return false; }
  int16_t diff = (int16_t)(hour * 60 + min) - (int16_t)(h * 60 + m);
  return (diff >= 0 && diff <= ALARM_CATCHUP_MIN);
}

// ── DFPlayer-Absturz-Neustart: Alarm-Merker in RTC-Speicher ──
// RTC_NOINIT_ATTR übersteht ESP.restart() (Software-Reset, kein Power-On) –
// dadurch weiß setup() nach einem durch verifyPlayStarted()/triggerAlarm()
// ausgelösten Neustart, welcher Alarm nicht mehr abgespielt werden konnte,
// und löst ihn dort erneut aus. rtcRetryMagic dient als Gültigkeits-Flag,
// da RTC_NOINIT_ATTR nach echtem Power-On undefinierten Inhalt hat.
#define RTC_RETRY_MAGIC 0xA1A2B3C4UL
RTC_NOINIT_ATTR uint32_t rtcRetryMagic;
RTC_NOINIT_ATTR uint8_t  rtcRetryAlarm;    // 1 oder 2
RTC_NOINIT_ATTR uint8_t  rtcRetryFileNo;   // sound1_assigned bzw. sound2_assigned zum Ausfallzeitpunkt
RTC_NOINIT_ATTR uint8_t  rtcRetryMin;      // Minute zum Ausfallzeitpunkt – fuer alarmTriggerMin/Logging nach Retry
RTC_NOINIT_ATTR uint8_t  rtcRetryCount;    // 12v07: bisherige Fehlversuche – nur gültig wenn rtcRetryMagic zuvor gesetzt war

// 12v12: In-Flight-Snapshot des gerade laufenden triggerAlarm()-Versuchs.
// alarmState wechselt erst NACH erfolgreicher verifyPlayStarted() auf
// ALARM_RUNNING – friert alarmTask VORHER ein (z.B. in der Drain-Schleife
// oder in verifyPlayStarted()), sieht watchdogTask nur ALARM_IDLE und weiß
// ohne diesen Marker nicht, dass gerade ein Alarm-Versuch lief. Gewöhnliche
// RAM-Variablen genügen: watchdogTask liest sie VOR dem ESP.restart(),
// müssen also nicht wie rtcRetry* einen Neustart überstehen. uint8_t/bool
// sind auf Xtensa atomar – kein Mutex nötig (analog wdg_*-Timestamps).
static volatile bool    alarmTriggerInFlight  = false;  // true nur während des riskanten Abschnitts in triggerAlarm()
static volatile uint8_t alarmTriggerNum       = 0;
static volatile uint8_t alarmTriggerFileNo    = 0;
static volatile uint8_t alarmTriggerMin       = 0;
static volatile uint8_t alarmTriggerFailCount = 0;

// Display einschalten, falls abgeschaltet – analog zum Touch-Wake in inputTask.
static void wakeDisplay() {
  if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    if (displayBlanked) {
      display.displayOn();
      displayBlanked = false;
      lastTouchMs = millis();
    }
    xSemaphoreGive(displayMutex);
  }
}

// ── Mühlrad-Motor ein/aus (12v03) ────────────────────────────
// Zentrale Ansteuerung von E2 statt verstreuter ledcWrite(E2,…)-Aufrufe.
// motorStart(): respektiert wheel_on; bei motor_duty < MOTOR_PWM_KICK_THRESHOLD
// kurzer Vollgas-Anlaufimpuls (3-V-Motor läuft sonst aus dem Stand nicht an),
// danach Sollwert. Das vTaskDelay des Kickstarts ist unkritisch:
//  • Alle Aufrufer (alarmTask @ALARM_IDLE, inputTask @S2) halten an der
//    Aufrufstelle KEINEN Mutex → Projektregel "kein vTaskDelay unter Mutex"
//    eingehalten.
//  • vTaskDelay gibt die CPU frei (kein Busy-Wait) → weder Hardware-TWDT
//    noch App-Watchdog (Timeout >> MOTOR_PWM_KICK_MS) sind betroffen.
//  • 150 ms verzögern lediglich den Folge-Code (Alarm-Statuspolling bzw.
//    S2-Tastenreaktion) – funktional irrelevant.
static void motorStart() {
  if (!wheel_on || motor_duty == 0) { ledcWrite(E2, 0); motorRunning = false; return; }
  if (motor_duty < MOTOR_PWM_KICK_THRESHOLD) {
    ledcWrite(E2, MOTOR_PWM_KICK_DUTY);                                                  // Vollgas-Anlaufimpuls
    vTaskDelay(pdMS_TO_TICKS(MOTOR_PWM_KICK_MS));                                        // außerhalb Mutex – Projektregel eingehalten
  }
  ledcWrite(E2, motor_duty);                                                            // auf Laufzeit-Sollwert
  motorRunning = true;
}

static void motorStop() {
  ledcWrite(E2, 0);
  motorRunning = false;
}

// ── DFPlayer Start-Check ──────────────────────────────────────
// Bestätigt nach playFolder(), dass der DFPlayer den Play-Befehl tatsächlich
// angenommen hat. Der DFPlayer braucht nach dem Kommando etwas Zeit, um die
// Datei von der SD-Karte zu laden und den Status auf "playing" zu setzen –
// zu frühes Abfragen liefert sonst fälschlich st<=0, obwohl der Player
// korrekt gestartet ist (12v06: löste unnötige Neustarts aus). Deshalb bis
// zu VERIFY_PLAY_RETRIES Versuche im Abstand von VERIFY_PLAY_DELAY_MS –
// bei aktuell 500 ms/3 Versuchen steht das Ergebnis spätestens nach 1500 ms
// fest.
// 20v09 (C3-Fix, Audit 2026-08-13): st==0 (Modul antwortet, aber "gestoppt" –
// typisch für "Datei nicht gefunden") und st==-1 (keine Antwort) wurden
// bisher identisch als "abgestürzt" behandelt und lösten beide denselben
// ESP.restart() aus – ein Neustart behebt einen Datei-/Kartenfehler aber
// nicht, das Ergebnis waren zwei sinnlose Reboots und danach ein Wecker ohne
// Ton, Motor und Licht (siehe triggerAlarm(), ALARM_MAX_RESTARTS). Jetzt
// getrennt: nur wenn NIE eine Antwort (st!=-1) ankam, gilt der DFPlayer als
// abgestürzt und rechtfertigt den Neustart. Kam mindestens einmal st==0 an,
// lebt das Modul nachweislich – PLAY_NO_SOUND signalisiert dem Aufrufer,
// den Alarm trotzdem als laufend zu behandeln (Motor/Licht), statt neu zu
// starten.
enum PlayVerifyResult : uint8_t { PLAY_OK, PLAY_NO_SOUND, PLAY_CRASHED };

static PlayVerifyResult verifyPlayStarted(const char* label, uint8_t fileNo) {
  int16_t st = -1;
  bool gotResponse = false;                                                             // mindestens einmal st != -1 gelesen → Modul lebt (st==0)
  for (uint8_t attempt = 1; attempt <= VERIFY_PLAY_RETRIES; attempt++) {
    vTaskDelay(pdMS_TO_TICKS(VERIFY_PLAY_DELAY_MS));                                    // außerhalb Mutex: DFPlayer Zeit zum Laden/Starten geben
    if (xSemaphoreTake(playerMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
      checkSerial2Leftover("readState (verifyPlayStarted)");
      st = readStateDrained();
      xSemaphoreGive(playerMutex);
    }
    // 20v02: BUSY-Pin (GPIO34) als Zusatzkriterium – bestätigt "läuft" auch
    // dann, wenn readStateDrained() (UART) trotz Gnadenfrist weiter st<=0
    // liefert. Vermeidet einen unnötigen ESP.restart(), wenn der DFPlayer in
    // Wahrheit korrekt läuft und nur die serielle Statusantwort ausbleibt.
    if (st > 0 || dfPlayerBusy()) { return PLAY_OK; }
    if (st != -1) { gotResponse = true; }
    webLogf("[DFPlayer] %s: kein Start-Status nach playFolder (Versuch %d/%d, st=%d, Datei %d)", label, attempt, VERIFY_PLAY_RETRIES, st, fileNo);
  }
  if (gotResponse) {
    webLogf("[FEHLER] %s: DFPlayer antwortet (kein Absturz), Datei %d startet aber nicht (st=0)", label, fileNo);
    return PLAY_NO_SOUND;
  }
  webLogf("[DFPlayer] %s: DFPlayer vermutlich abgestürzt nach %d Versuchen (Datei %d), Neustart", label, VERIFY_PLAY_RETRIES, fileNo);
  return PLAY_CRASHED;
}

// Startet Sound + Motor + Licht für Alarm 1 (alarmNum=1) oder Alarm 2
// (alarmNum=2) und geht in ALARM_RUNNING über (mit oder ohne bestätigten
// Ton, siehe PLAY_NO_SOUND/alarmSilentFallback). Antwortet der DFPlayer gar
// nicht (verifyPlayStarted()==PLAY_CRASHED), hinterlegt die Funktion den
// Alarm im RTC-Merker (siehe rtcRetryMagic) und löst ESP.restart() aus –
// setup() löst den Alarm nach dem Neustart erneut über denselben Pfad aus.
// failCount zählt die bisherigen Fehlversuche dieses Alarms mit (0 bei
// regulärem Erstaufruf aus runAlarmMachine, sonst rtcRetryCount aus setup()).
// Ab ALARM_MAX_RESTARTS (12v07) bricht die Funktion statt eines weiteren
// ESP.restart() endgültig ab: kein Neustart mehr, Alarm bleibt inaktiv
// (Motor/Licht aus – vermeidet Endlos-Neustartschleife bei dauerhaft
// defektem/nicht angeschlossenem DFPlayer), stattdessen roter Fehlereintrag
// mit Datum/Uhrzeit im Web-Log ([FEHLER]-Tag, siehe webLogTask()).
static void triggerAlarm(uint8_t alarmNum, uint8_t fileNo, uint8_t min, uint8_t failCount) {
  const char* label = (alarmNum == 1) ? "Alarm 1" : "Alarm 2";
  if (xSemaphoreTake(playerMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
    // 12v12: In-Flight-Marker setzen, BEVOR der riskante Abschnitt beginnt –
    // watchdogTask kann bei einem Freeze ab hier einen Ersatzalarm auslösen.
    alarmTriggerNum       = alarmNum;
    alarmTriggerFileNo    = fileNo;
    alarmTriggerMin       = min;
    alarmTriggerFailCount = failCount;
    alarmTriggerInFlight  = true;

    // 20v06 (B2-Fix, Audit 2026-08-13): RTC-Merker JETZT schreiben statt erst
    // im expliziten Fehlerpfad unten – ein Freeze/Reset irgendwo zwischen
    // hier und der ersten bestätigten Wiedergabe (playFolder, verifyPlay-
    // Started(), motorStart()/State-Übergang, das ALARM_POLL_MS-Fenster vor
    // dem ersten Poll in runAlarmMachine) verlor den Alarm bisher ersatzlos,
    // weil rtcRetryMagic nur bei einem BESTÄTIGTEN Fehlschlag gesetzt wurde.
    // Gelöscht wird er erst in runAlarmMachine(), sobald die Wiedergabe
    // bestätigt lief oder der Alarm regulär endet (siehe dort). Sicher erst
    // seit dem Tages-Guard aus A1 (alarmDue()): ein später Reboot löst
    // höchstens einen bereits gelaufenen Alarm erneut aus, nie einen
    // verspäteten Zweitalarm (siehe Review-Notiz zu B2) – NICHT ohne A1
    // umsetzen.
    rtcRetryAlarm  = alarmNum;
    rtcRetryFileNo = fileNo;
    rtcRetryMin    = min;
    rtcRetryCount  = failCount;
    rtcRetryMagic  = RTC_RETRY_MAGIC;

    // 12v14: Drain über die Bibliothek (player.available()/read()) statt roher
    // Serial2.read()-Bytes – ein roher Discard reißt ggf. mitten in einem noch
    // eintreffenden Frame ab, ohne dass DFRobotDFPlayerMini davon weiß. Die
    // Bibliothek verliert dadurch die Synchronisation zu _receivedIndex/
    // _isSending, was zu anhaltendem Byte-Durcheinander auf der UART und in
    // der Folge zum 15s-TWDT-Hänger in available() führte (Root Cause des
    // Hardware-Resets statt Alarmauslösung – nicht in der Bibliothek, siehe
    // Software/Bibliotheken/README.md, Patch dort zurückgenommen).
    drainSerial2Pre("playFolder (triggerAlarm)");
    player.playFolder(1, fileNo);
    xSemaphoreGive(playerMutex);
    PlayVerifyResult playResult = verifyPlayStarted(label, fileNo);
    if (playResult == PLAY_CRASHED) {
      failCount++;
      if (failCount >= ALARM_MAX_RESTARTS) {
        char ts[20];
        snapTimeStr(ts, sizeof(ts));
        webLogf("[FEHLER] %s: DFPlayer antwortet nach %u Versuchen weiterhin nicht (Datei %d) – Alarm abgebrochen, %s",
                label, (unsigned)failCount, fileNo, ts);
        rtcRetryMagic = 0;                                                                // kein weiterer Retry nach Abbruch
        alarmTriggerInFlight = false;                                                     // 12v12: kein Freeze-Fallback mehr nötig – Abbruch ist final
        return;
      }
      rtcRetryCount  = failCount;                                                         // Merker ist bereits gesetzt (s.o.) – nur Fehlversuchszähler für den nächsten Versuch aktualisieren
      alarmTriggerInFlight = false;                                                       // 12v12: regulärer Retry-Pfad übernimmt – Freeze-Fallback nicht mehr nötig
      ESP.restart();
    }
    alarmTriggerInFlight = false;                                                         // 12v12: Play erfolgreich bestätigt (mit oder ohne Ton) – kein Freeze-Fallback mehr nötig
    // 20v09 (C3-Fix): PLAY_NO_SOUND läuft in denselben Erfolgspfad wie
    // PLAY_OK (Motor/Licht sollen den Nutzer trotzdem wecken) – einziger
    // Unterschied ist das Flag, das runAlarmMachine() vor einem sofortigen
    // Ausstieg über playerStatus==0 bewahrt (siehe ALARM_RUNNING unten).
    alarmSilentFallback = (playResult == PLAY_NO_SOUND);
    {
      // 20v04 (A1-Fix): Tages-Sperre erst nach erfolgreichem Start setzen –
      // eigener Zeitstempel statt des min-Parameters, der nur den urspruenglich
      // geplanten Zeitpunkt trägt (bei RTC-Retry ggf. der vorige Tag).
      time_t    now_trigger;
      struct tm tm_trigger;
      time(&now_trigger);
      localtime_r(&now_trigger, &tm_trigger);
      uint16_t today = (uint16_t)tm_trigger.tm_yday;
      if (alarmNum == 1) { lastA1Day = today; } else { lastA2Day = today; }
    }
    snapTimeStr(snapAlarmTime, sizeof(snapAlarmTime));                                    // letzter erfolgreicher Alarm für Web-Log
    motorStart();                                                                        // 12v03: Motor via PWM + Kickstart (respektiert wheel_on)
    if (light_on) { digitalWrite(E3, HIGH); }
    t_start6      = millis();
    alarmRunStart = t_start6;                                                            // 20v05 (A3-Restarbeit): fester Startzeitpunkt für ALARM_MAX_RUN_MS
    alarmState    = ALARM_RUNNING;                                                       // → ALARM_RUNNING
    wakeDisplay();
  }
}

// ── Alarm-State-Machine ──────────────────────────────────────
// sec/min/hour/yday: atomarer Zeitschnappschuss aus alarmTask – keine Race Condition mit displayTask
static void runAlarmMachine(uint8_t sec, uint8_t min, uint8_t hour, uint16_t yday) {
  switch (alarmState) {

    case ALARM_IDLE:
      // 20v04 (A1-Fix): pegelbasierte Tages-Sperre statt sec==0-Flanke, siehe
      // alarmDue(). lastA*Day wird erst NACH erfolgreichem playFolder gesetzt
      // (in triggerAlarm()) – vermeidet stille Alarme bei playerMutex-Timeout
      // (z.B. während langem WebLog-Zugriff). Nächster alarmTask-Tick (500 ms
      // später) versucht es dann automatisch erneut, solange das Nachhol-
      // fenster (ALARM_CATCHUP_MIN) nicht abgelaufen ist.
      // Alarm 1 prüfen
      if (alarmDue(a1_on, a1_hour, a1_min, hour, min, yday, lastA1Day)) {
        triggerAlarm(1, sound1_assigned, min, 0);
      }
      // Alarm 2 prüfen (else if → Alarm 1 hat Vorrang bei gleicher Zeit)
      else if (alarmDue(a2_on, a2_hour, a2_min, hour, min, yday, lastA2Day)) {
        triggerAlarm(2, sound2_assigned, min, 0);
      }
      break;

    case ALARM_RUNNING:
      if (delayFunction(t_start6, ALARM_POLL_MS)) {
        int16_t st = -1;
        if (xSemaphoreTake(playerMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
          checkSerial2Leftover("readState (ALARM_RUNNING Poll)");
          st = readStateDrained();
          xSemaphoreGive(playerMutex);                                                     // Mutex SOFORT freigeben – nie mit gehaltenem Mutex schlafen
        }
        vTaskDelay(pdMS_TO_TICKS(1));                                                      // 1ms Pause AUSSERHALB Mutex: DFPlayer-Antwort stabilisieren
        if (xSemaphoreTake(playerMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
          checkSerial2Leftover("readState (ALARM_RUNNING Poll, 2. Abfrage)");
          st = readStateDrained();                                                          // zweite Abfrage → korrekter Status
          xSemaphoreGive(playerMutex);
        }
        playerStatus = st;
        t_start6 = millis();
        if (playerStatus > 0) {
          // 20v06 (B2-Fix): erster Poll bestätigt echte Wiedergabe – RTC-
          // Merker kann weg, ein Reboot ab jetzt darf den Alarm nicht mehr
          // erneut auslösen (siehe früher Schreibpunkt in triggerAlarm()).
          rtcRetryMagic = 0;
          alarmSilentFallback = false;                                                   // 20v09 (C3-Fix): doch noch Ton bekommen – Fallback-Sonderfall beendet
        }
        // 20v00: BUSY-Pin (GPIO34, Hardware ab 2v0) löst den bisherigen
        // "-1=UART-Timeout → Alarm läuft sicherheitshalber weiter"-Fallback ab.
        // Serial2-Timeouts (st==-1) sind bei diesem DFPlayer-Chip nicht selten
        // (siehe readStateDrained()) und ließen den Alarm bislang im Zweifel
        // weiterlaufen. BUSY ist ein direkt getriebenes Hardware-Signal (kein
        // UART) und bestätigt in diesem Fall zuverlässig, ob wirklich noch
        // Wiedergabe läuft (LOW) oder nicht (HIGH). 20v03: dfPlayerIdleDebounced()
        // statt einzelnem dfPlayerBusy() – diese Entscheidung ist unwiderruflich
        // (Motor/Licht aus, State-Reset), daher zusätzliche Absicherung gegen
        // einen einzelnen durchschlagenden Störimpuls.
        // 20v09 (C3-Fix): im stummen Fallback (alarmSilentFallback) meldet
        // der Player dauerhaft playerStatus==0, ohne dass je Ton lief – ohne
        // dieses Gate würde mp3Finished schon beim allernächsten Poll (5 s)
        // zuschlagen und Motor/Licht sofort wieder abschalten, statt den
        // Nutzer wenigstens für eine sinnvolle Zeit zu wecken (Sinn von C3).
        // Einziger Ausstieg dann: ALARM_MAX_RUN_MS (A3) oder S1 (manueller Stopp).
        bool mp3Finished = !alarmSilentFallback
                         && ((playerStatus == 0)
                             || (playerStatus == -1 && dfPlayerIdleDebounced()));
        // 20v05 (A3-Restarbeit): harte Obergrenze als Rückfallebene – deckt
        // den Fall ab, dass der BUSY-Pin selbst dauerhaft LOW hängt (Modul-
        // Fehlfunktion, Leitungsfehler) und dfPlayerIdleDebounced() dadurch
        // nie "idle" meldet. Ohne diesen Deckel bliebe das Gerät in diesem
        // Fall für immer entwaffnet (Motor/Licht dauerhaft an, kein weiterer
        // Alarm möglich), siehe Audit-Befund A3.
        bool runTimeExceeded = (millis() - alarmRunStart >= ALARM_MAX_RUN_MS);
        if (mp3Finished || runTimeExceeded) {                                            // MP3 beendet ODER Obergrenze erreicht
          if (runTimeExceeded && !mp3Finished) {
            webLogf("[FEHLER] Alarm nach %lu ms ohne Endebestaetigung abgebrochen (playerStatus=%d)",
                    (unsigned long)(millis() - alarmRunStart), playerStatus);
          }
          motorStop();                                                                   // 12v03: Motor-PWM abschalten
          if (light_on) { digitalWrite(E3, LOW); }
          // 20v04: Tages-Sperre (lastA1Day/lastA2Day) bleibt bewusst bestehen –
          // ein Reset hier würde die pegelbasierte Fälligkeitsprüfung (siehe
          // alarmDue()) im nächsten 500-ms-Tick sofort wieder auslösen.
          // 20v06 (B2-Fix): RTC-Merker sicherheitshalber auch hier löschen –
          // deckt einen sehr kurzen Sound ab, der schon vor dem ersten Poll
          // mit playerStatus > 0 beendet war (dann griff die Löschung oben
          // nie) sowie den runTimeExceeded-Abbruch (Alarm lief nachweislich,
          // ein späterer Reboot soll ihn nicht erneut auslösen).
          rtcRetryMagic = 0;
          alarmSilentFallback = false;                                                   // 20v09 (C3-Fix): Sonderfall beendet, nächster Alarm startet neutral
          alarmState = ALARM_IDLE;                                                       // → ALARM_IDLE
        }
      }
      break;
  }
}

// ── Kuckuck-State-Machine ────────────────────────────────────
// 20v04 (A6-Fix, Audit 2026-08-13): Tag+Stunden-Sperre statt reiner Minuten-
// Sperre. Die alte lastCuckooMin wurde beim Ende von CUCKOO_RUNNING (7,5 s
// nach Auslösung) bereits wieder aufgehoben – bei der Oktober-Zeitumstellung
// durchläuft die Ortszeit 02:00–02:59 zweimal, die zweite volle Stunde 02:00
// löste dadurch einen zweiten, ungewollten Kuckuck aus. Tag+Stunde bleiben bis
// zur nächsten (zwangsläufig anderen) Stunde gesperrt und decken den Fall ab.
static uint16_t lastCuckooDay  = 0xFFFF;  // file-scope für manuellen Zugriff (S1-Handler)
static uint8_t  lastCuckooHour = 0xFF;

// sec/min/hour/yday: atomarer Zeitschnappschuss aus alarmTask
static void runCuckooMachine(uint8_t sec, uint8_t min, uint8_t hour, uint16_t yday) {
  switch (cuckooState) {

    case CUCKOO_IDLE:
      if (min == 0 && sec == 0 && cuckoo_on
          && !(yday == lastCuckooDay && hour == lastCuckooHour)) {
        // Unterdrücken wenn Alarm 1 oder Alarm 2 auf diese volle Stunde eingestellt ist
        bool alarmThisHour = (a1_on && a1_min == 0 && hour == a1_hour)
                          || (a2_on && a2_min == 0 && hour == a2_hour);
        // Zeitfenster-Prüfung mit Mitternacht-Überlauf:
        // normal (z.B. 06–22):    hour >= on && hour <= off
        // über Mitternacht (z.B. 22–06): hour >= on || hour <= off
        bool inTimeWindow = (cuckoo_onTime <= cuckoo_offTime)
                          ? (hour >= cuckoo_onTime && hour <= cuckoo_offTime)
                          : (hour >= cuckoo_onTime || hour <= cuckoo_offTime);
        if (!alarmThisHour && inTimeWindow) {
          lastCuckooDay  = yday;
          lastCuckooHour = hour;
          digitalWrite(E1, HIGH);
          t_start4    = millis();
          cuckooState = CUCKOO_RUNNING;                                                  // → CUCKOO_RUNNING
        }
      }
      break;

    case CUCKOO_RUNNING:
      if (delayFunction(t_start4, CUCKOO_DURATION_MS)) {
        digitalWrite(E1, LOW);
        // Tag+Stunden-Sperre bleibt bestehen – die nächste Stunde hat
        // automatisch einen anderen hour-Wert, kein expliziter Reset nötig.
        cuckooState = CUCKOO_IDLE;                                                       // → CUCKOO_IDLE
      }
      break;
  }
}



// =============================================================
//  Task 1 – touchTask  (Core 0, Pri 2)
//
//  Touch-State-Machine:
//
//  ┌─────────┐  Touch ON        ┌───────────┐  ≥ TOUCH_HOLD_MS  ┌──────────┐
//  │ TS_IDLE │─────────────────▶│TS_PRESSED │──────────────────▶│TS_REPEAT │
//  └─────────┘  + EVT sofort    └───────────┘  + EVT            └──────────┘
//       ▲                            │                                │
//       │         Touch OFF          │         Touch OFF              │ alle TOUCH_REPEAT_MS
//       └────────────────────────────┘                               │  → EVT senden
//       ▲                                                             │
//       └─────────────────────────────────────────────────────────────┘
//
//  Exklusivität: Sobald ein Pad aktiv ist (TS_PRESSED / TS_REPEAT),
//  werden alle anderen Pads vollständig ignoriert.
// =============================================================
static void touchTask(void *pvParam) {
  touch_pad_init();
  touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
  for (int i = 0; i < 4; i++) { touch_pad_config(TOUCH_PADS[i], 0); }

  vTaskDelay(pdMS_TO_TICKS(300));                                                        // Einschwingenzeit

  uint16_t baseline[4];
  for (int i = 0; i < 4; i++) {
    touch_pad_read(TOUCH_PADS[i], &baseline[i]);
  }
  updateSnapTouch(baseline);                                                               // initiale Baseline im Snapshot speichern

  static const uint8_t EVT_ID[4] = { EVT_T0, EVT_T2, EVT_T3, EVT_T4 };

  // ── State-Machine-Variablen ──────────────────────────────────
  TouchState tsState     = TS_IDLE;
  int8_t     activeIdx   = -1;                                                            // Index des aktiven Pads (0–3), -1 = keines
  uint32_t   pressStart  = 0;                                                             // Zeitpunkt des ersten Kontakts
  uint32_t   lastRepeat  = 0;                                                             // Zeitpunkt des letzten EVT im REPEAT-Zustand
  uint32_t   lastRecal   = millis();                                                      // Zeitpunkt der letzten Baseline-Messung

  while (true) {
    // ── Baseline-Rekalibrierung (nur im Ruhezustand) ────────────
    // Kapazitive Touch-Pads driften thermisch. Eine periodische
    // Neumessung der Baseline verhindert Fehlauslösungen nach
    // langem Betrieb. Nur in TS_IDLE: kein aktiver Touch stört.
    if (tsState == TS_IDLE && millis() - lastRecal >= TOUCH_RECAL_MS) {
      for (int i = 0; i < 4; i++) {
        touch_pad_read(TOUCH_PADS[i], &baseline[i]);
      }
      updateSnapTouch(baseline);                                                           // Snapshot aktualisieren (kein Ring-Puffer-Eintrag)
      lastRecal = millis();
    }

    // Alle vier Pads einlesen
    uint16_t val[4];
    bool     padPressed[4];
    for (int i = 0; i < 4; i++) {
      touch_pad_read(TOUCH_PADS[i], &val[i]);
      uint16_t thr  = (baseline[i] > TOUCH_DROP)
                     ? baseline[i] - TOUCH_DROP          // Normalfall: absoluter Schwellwert
                     : baseline[i] - baseline[i] / 5;    // Fallback: 80 % der Baseline (kein float)
      padPressed[i] = (val[i] < thr);
    }

    uint32_t now = millis();

    switch (tsState) {

      // ── TS_IDLE: alle Pads beobachten, erstes aktives gewinnt ──
      case TS_IDLE:
        for (int i = 0; i < 4; i++) {
          if (padPressed[i]) {
            activeIdx  = i;
            pressStart = now;
            lastRepeat = now;
            uint8_t evt = EVT_ID[i];
            xQueueSend(inputQueue, &evt, 0);                                             // sofortiger erster EVT
            tsState = TS_PRESSED;
            break;                                                                       // Exklusiv: nur erstes Pad, restliche ignorieren
          }
        }
        break;

      // ── TS_PRESSED: nur activeIdx prüfen, auf HOLD-Schwelle warten
      case TS_PRESSED:
        if (!padPressed[activeIdx]) {                                                    // losgelassen vor HOLD → kein weiterer EVT
          tsState   = TS_IDLE;
          activeIdx = -1;
        } else if (now - pressStart >= TOUCH_HOLD_MS) {                                 // HOLD-Schwelle überschritten
          uint8_t evt = EVT_ID[activeIdx];
          if (xQueueSend(inputQueue, &evt, 0) == pdTRUE) {                               // nur weitermachen wenn EVT angenommen
            lastRepeat = now;
            tsState    = TS_REPEAT;
          }
        }
        break;

      // ── TS_REPEAT: alle TOUCH_REPEAT_MS weiteren EVT senden ────
      case TS_REPEAT:
        if (!padPressed[activeIdx]) {                                                    // losgelassen → zurück zu IDLE
          tsState   = TS_IDLE;
          activeIdx = -1;
        } else if (now - lastRepeat >= TOUCH_REPEAT_MS) {                               // Wiederholintervall abgelaufen
          uint8_t evt = EVT_ID[activeIdx];
          if (xQueueSend(inputQueue, &evt, 0) == pdTRUE) {                               // lastRepeat nur bei Erfolg aktualisieren
            lastRepeat = now;
          }
        }
        break;
    }

    vTaskDelay(pdMS_TO_TICKS(TOUCH_POLL_MS));
  }
}



// =============================================================
//  Taster-ISRs  (S1, S2, S3)
//
//  Stufe 1 – Hardware-Entprellung im ISR (BTN_DEBOUNCE_MS = 30 ms):
//  Prellimpulse innerhalb des Fensters werden verworfen, die Queue
//  bleibt sauber. millis() ist in ISR-Kontext auf dem ESP32 sicher.
//
//  Stufe 2 – Aktionssperre in inputTask (BTN_LOCKOUT_MS = 1000 ms):
//  Verhindert unbeabsichtigte Doppelaktionen nach bewusstem Druck.
// =============================================================
void IRAM_ATTR isrS1() {
  uint32_t now = millis();
  if (now - isrBtnMs[0] < BTN_DEBOUNCE_MS) return;          // Prellimpuls → verwerfen
  isrBtnMs[0] = now;
  uint8_t e = EVT_S1; BaseType_t hp = pdFALSE;
  xQueueSendFromISR(inputQueue, &e, &hp); portYIELD_FROM_ISR(hp);
}
void IRAM_ATTR isrS2() {
  uint32_t now = millis();
  if (now - isrBtnMs[1] < BTN_DEBOUNCE_MS) return;          // Prellimpuls → verwerfen
  isrBtnMs[1] = now;
  uint8_t e = EVT_S2; BaseType_t hp = pdFALSE;
  xQueueSendFromISR(inputQueue, &e, &hp); portYIELD_FROM_ISR(hp);
}
void IRAM_ATTR isrS3() {
  uint32_t now = millis();
  if (now - isrBtnMs[2] < BTN_DEBOUNCE_MS) return;          // Prellimpuls → verwerfen
  isrBtnMs[2] = now;
  uint8_t e = EVT_S3; BaseType_t hp = pdFALSE;
  xQueueSendFromISR(inputQueue, &e, &hp); portYIELD_FROM_ISR(hp);
}



// =============================================================
//  Task 2 – inputTask  (Core 1, Pri 2)
//
//  Dispatch-Loop:
//    EVT_S1, EVT_S2 → vor displayMutex behandeln (kein Display nötig,
//                     kein vTaskDelay unter Mutex)
//    alle anderen   → displayMutex holen → uiDispatch → uiTransition
//    safeChange     → nvrSemaphore (erst NACH NVR_COMMIT_DELAY_MS Ruhezeit
//                     seit safeChangeMs – schützt Flash vor Touch-REPEAT-Hammer)
// =============================================================
static void inputTask(void *pvParam) {
  esp_task_wdt_add(NULL);          // Hardware-TWDT: diesen Task anmelden
  uint8_t evt;

  while (true) {
    esp_task_wdt_reset();          // Hardware-TWDT zurücksetzen – alle ~50 ms
    if (xQueueReceive(inputQueue, &evt, pdMS_TO_TICKS(50)) != pdTRUE) {
      wdg_inputTask = millis();                                                        // Alive-Signal: Task läuft (auch bei leerem Queue)
      if (safeChange && (millis() - safeChangeMs >= NVR_COMMIT_DELAY_MS)) {
        safeChange = false;
        xSemaphoreGive(nvrSemaphore);
      }
      continue;
    }
    wdg_inputTask = millis();                                                          // Alive-Signal: Event empfangen

    // ── 10v00: Display abgeschaltet → Touch weckt, Event wird verworfen ──
    // Spec: Berührung eines Touchpads schaltet Display erneut für
    // DISPLAY_TIMEOUT_MS (5 min, seit 10v02) ein; andere Touch-Funktionen
    // sind nur bei eingeschaltetem Display aktiv. Hardware-Taster S1/S2
    // arbeiten unabhängig weiter.
    // 11v04: S3 weckt das Display UND öffnet die Info-Seite (Event bleibt
    // erhalten). Auto-Return (20 s) garantiert, dass das Display nur von
    // UI_CLOCK aus blanken kann – der S3-Toggle in uiDispatch() landet also
    // deterministisch auf UI_INFO. Touch T0–T4 bleiben reines Wake+Discard,
    // um T3 (WLAN-Reset, seit 11v05) / T4 (Werksreset) auf der Info-Seite
    // nicht versehentlich auszulösen, wenn der Nutzer blind auf das dunkle
    // Display tippt.
    if (displayBlanked && evt <= EVT_T4) {
      if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        display.displayOn();
        displayBlanked = false;
        xSemaphoreGive(displayMutex);
      }
      lastTouchMs = millis();                                                            // DISPLAY_TIMEOUT_MS-Timer neu starten
      continue;                                                                          // Wake-Event nicht an State-Machine weitergeben
    }
    if (displayBlanked && evt == EVT_S3) {
      if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        display.displayOn();
        displayBlanked = false;
        xSemaphoreGive(displayMutex);
      }
      lastTouchMs = millis();                                                            // DISPLAY_TIMEOUT_MS-Timer neu starten
      // KEIN continue – S3 läuft weiter zur State-Machine → Info-Seite
    }

    // ── S1: Alarm/Sound stoppen oder Kuckuck einmalig ───────────
    // Kein Display nötig → außerhalb displayMutex.
    // vTaskDelay (player.readState-Schleife) ist damit nie unter Mutex.
    if (evt == EVT_S1) {
      uint32_t t_now = millis();
      if (t_now - lastBtnMs[0] >= BTN_LOCKOUT_MS) {
        lastBtnMs[0] = t_now;
        // Erster readState-Versuch außerhalb Mutex-Dauersperre
        int16_t st = -1;
        if (xSemaphoreTake(playerMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
          checkSerial2Leftover("readState (S1)");
          st = readStateDrained();
          xSemaphoreGive(playerMutex);                                                   // Mutex sofort freigeben
        }
        // Standby-Status auflösen: vTaskDelay AUSSERHALB Mutex (Projektregel)
        uint32_t rs_start = millis();
        while (st == -1) {
          if (millis() - rs_start >= 200) {                                             // Timeout 200 ms → DFPlayer antwortet nicht
            // 20v02: BUSY-Pin (GPIO34) statt blindem "als idle behandeln" –
            // Hardware-Signal klärt zuverlässig, ob noch Wiedergabe läuft
            // (LOW), statt bei UART-Timeout fälschlich den Kuckuck auszulösen.
            st = dfPlayerBusy() ? 1 : 0;
            break;
          }
          vTaskDelay(pdMS_TO_TICKS(1));                                                 // außerhalb Mutex – Projektregel eingehalten
          if (xSemaphoreTake(playerMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            checkSerial2Leftover("readState (S1 Retry)");
            st = readStateDrained();
            xSemaphoreGive(playerMutex);
          }
        }
        // 20v09 (C3-Fix, Audit 2026-08-13): alarmState zusätzlich zu st
        // heranziehen – vorher entschied ausschließlich der (möglicherweise
        // stille) Playerstatus, ob S1 stoppt oder Kuckuck auslöst. Seit C3
        // kann ein Alarm mit playerStatus==0 laufen (alarmSilentFallback,
        // Modul lebt, Datei startet nicht) – ohne diese Ergänzung hätte S1
        // in genau diesem Fall NICHT gestoppt, sondern versehentlich den
        // Kuckuck ausgelöst. player.stop() darf trotzdem scheitern: Motor,
        // Licht und Zustand werden unabhängig davon zurückgesetzt.
        if (alarmState == ALARM_RUNNING || st > 0) {
          if (xSemaphoreTake(playerMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
            drainSerial2Pre("stop (S1)");
            player.stop();
            xSemaphoreGive(playerMutex);
          }
          motorStop();                                                                   // 12v03: Motor-PWM abschalten (war digitalWrite LOW)
          digitalWrite(E3, LOW);
          alarmState = ALARM_IDLE;
          alarmSilentFallback = false;                                                   // 20v09 (C3-Fix): Sonderfall beendet, nächster Alarm startet neutral
          // 20v04 (A1-Fix): Tages-Sperre (lastA1Day/lastA2Day) bewusst NICHT
          // aufgehoben – sonst würde die pegelbasierte Fälligkeitsprüfung
          // (alarmDue()) den gerade manuell gestoppten Alarm im nächsten
          // 500-ms-Tick sofort wieder auslösen. Bleibt bis Tagesende gesperrt.
        } else {
          // 9v13 Hinweis: t_start4 / cuckooState / lastCuckooDay/-Hour werden
          // hier (inputTask, Core 1) und in runCuckooMachine (alarmTask,
          // Core 0) ohne Mutex beschrieben. Die Felder sind einzeln atomar
          // (Xtensa, kein Torn Write); die Logik toleriert einen minimalen
          // Zeitversatz zwischen den Feldern, weil alarmTask cuckooState als
          // Leitzustand verwendet und t_start4 erst im Folge-Tick (500 ms)
          // prüft. Daher bewusst ohne Mutex gelassen.
          digitalWrite(E1, HIGH);
          t_start4    = millis();
          cuckooState = CUCKOO_RUNNING;
          // 20v04 (A6-Fix): Tag+Stunde sperren statt Minute – konsistent mit
          // runCuckooMachine(). Eigener Zeitstempel statt der gemeinsamen
          // t_hour/t_min-Globals (Core-Grenze, kein tm_yday-Äquivalent dort).
          time_t    now_s1;
          struct tm tm_s1;
          time(&now_s1);
          localtime_r(&now_s1, &tm_s1);
          lastCuckooDay  = (uint16_t)tm_s1.tm_yday;
          lastCuckooHour = (uint8_t)tm_s1.tm_hour;
        }
      }
      if (safeChange && (millis() - safeChangeMs >= NVR_COMMIT_DELAY_MS)) {
        safeChange = false;
        xSemaphoreGive(nvrSemaphore);
      }
      continue;
    }

    // ── S2: Zugschalter – Licht + Mühlrad EIN/AUS ───────────────
    // Kein Display nötig → außerhalb displayMutex.
    if (evt == EVT_S2) {
      uint32_t t_now = millis();
      if (t_now - lastBtnMs[1] >= BTN_LOCKOUT_MS) {
        lastBtnMs[1] = t_now;
        S2_SW = !S2_SW;
        if (S2_SW) {
          t_start_S2 = t_now;                                                              // 12v02: Start der 30-min-Einschaltzeitbegrenzung
          motorStart();                                                                  // 12v03: Motor via PWM + Kickstart (respektiert wheel_on)
          if (light_on) { digitalWrite(E3, HIGH); }
        } else {
          motorStop();                                                                    // 12v03: Motor-PWM abschalten
          digitalWrite(E3, LOW);
        }
      }
      if (safeChange && (millis() - safeChangeMs >= NVR_COMMIT_DELAY_MS)) {
        safeChange = false;
        xSemaphoreGive(nvrSemaphore);
      }
      continue;
    }

    // ── alle anderen Events: displayMutex holen ───────────────────
    // Touch-Events (T0–T4) und S3 (Info-Seite) aktualisieren den Auto-Rückkehr-Timer.
    if (evt <= EVT_T4 || evt == EVT_S3) {
      lastTouchMs = millis();
    }

    if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
      continue;                                                                          // Display belegt – Event verwerfen
    }

    UiState next = uiDispatch(uiState, evt);                                            // State-Machine auswerten
    if (next != uiState) {
      uiTransition(next);                                                               // Zustand wechseln + Bildschirm zeichnen
    }

    xSemaphoreGive(displayMutex);

    // ── WiFi-Konfig angefordert (von onInfo/EVT_T3, 11v05) ───
    // Mutex erneut holen – displayTask könnte sonst dazwischenfunken.
    // vTaskDelay liegt bewusst AUSSERHALB des Mutex-Blocks.
    if (wifiConfigRequested) {
      wifiConfigRequested = false;
      if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        display.clear();
        zeigeZ16C(64, 16, "WiFi-Setup");
        zeigeZ16C(64, 32, "Neustart ...");
        display.display();
        xSemaphoreGive(displayMutex);
      }
      vTaskDelay(pdMS_TO_TICKS(1500));                                                   // Meldung lesbar halten – außerhalb Mutex
      ESP.restart();
    }

    // ── Werksreset angefordert (von onInfo/EVT_T4) ────────────
    if (factoryResetRequested) {
      factoryResetRequested = false;
      if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        display.clear();
        zeigeZ16C(64, 10, "Werksreset");
        zeigeZ16C(64, 26, "NVS wird");
        zeigeZ16C(64, 42, "gelöscht ...");
        display.display();
        xSemaphoreGive(displayMutex);
      }
      vTaskDelay(pdMS_TO_TICKS(2000));                                                   // Meldung lesbar halten – außerhalb Mutex
      nvs_flash_erase();                                                                 // NVS-Partition vollständig löschen
      nvs_flash_init();                                                                  // NVS neu initialisieren
      ESP.restart();
    }

    if (safeChange && (millis() - safeChangeMs >= NVR_COMMIT_DELAY_MS)) {
      safeChange = false;
      xSemaphoreGive(nvrSemaphore);
    }
  }
}



// =============================================================
//  Task 3 – displayTask  (Core 1, Pri 1)
//
//  showTime() wird jetzt unter displayMutex aufgerufen:
//  datum[], zeit[], t_* werden konsistent mit menu() geschrieben.
//  NTP-Pending-Flag wird unter Mutex in echte Sync-Buffer übertragen.
//  Auto-Rückkehr zu UI_CLOCK nach AUTO_RETURN_MS (20 s) ohne Touch-Eingabe.
//  UI_INFO (Seite 7) eingeschlossen – auch von der Info-Seite kehrt Auto-Return zurück.
// =============================================================
static void displayTask(void *pvParam) {
  esp_task_wdt_add(NULL);          // Hardware-TWDT: diesen Task anmelden
  static uint8_t t_sec_alt = 0xFF;                                                         // 9v14: task-lokal (früher file-scope)
  while (true) {
    esp_task_wdt_reset();          // Hardware-TWDT zurücksetzen – alle ~300 ms
    wdg_displayTask = millis();                                                            // Alive-Signal: vor Mutex – gültig auch wenn Mutex-Timeout auftritt

    if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) == pdTRUE) {

      showTime();                                                                          // datum[], zeit[], t_* unter Mutex → konsistent mit menu()

      // NTP-Sync-Daten sicher übertragen (Callback schrieb in tmp-Puffer)
      if (ntpSyncPending) {
        ntpSyncPending = false;
        memcpy(datum_sync, datum_sync_tmp, sizeof(datum_sync));
        memcpy(zeit_sync,  zeit_sync_tmp,  sizeof(zeit_sync));

        // 12v04: Reset-Zeitstempel ("letzter Reset" im Web-Log) nachtragen,
        // falls beim Boot kein WLAN/NTP zustande kam (z.B. nach Stromausfall,
        // Router noch nicht oben) und snapNtpTime in setup() nie gesetzt wurde.
        // Rekonstruiert den tatsächlichen Reset-Zeitpunkt aus aktueller Zeit
        // minus Uptime (NTP ist hier gerade erst synchron geworden).
        if (snapNtpTime[0] == '\0') {
          time_t reset_t = time(nullptr) - (time_t)(millis() / 1000UL);
          struct tm tm_reset;
          localtime_r(&reset_t, &tm_reset);
          strftime(snapNtpTime, sizeof(snapNtpTime), "%d.%m.%Y %H:%M:%S", &tm_reset);
        }
      }

      // WiFi-Verbindungsdaten sicher übertragen (wifiTask schrieb in tmp-Puffer auf Core 0)
      if (wifiSyncPending) {
        wifiSyncPending = false;
        memcpy(datum_WiFi, datum_WiFi_tmp, sizeof(datum_WiFi)); // atomarer Puffer-Tausch unter displayMutex
        memcpy(zeit_WiFi,  zeit_WiFi_tmp,  sizeof(zeit_WiFi));  // atomarer Puffer-Tausch
      }

      if (uiState == UI_CLOCK) {
        // 9v13: Mitternachts-Redraw als One-Shot – früher wurde menu(0)
        // ~6-7× innerhalb der 2-Sekunden-Fenster aufgerufen (alle 300 ms)
        // und verursachte sichtbares Flackern. Jetzt genau ein Full-Redraw
        // beim Eintritt in 00:00:00, Flag resettet bei Stundenwechsel.
        static bool midnightDrawn = false;
        if (t_hour == 0 && t_min == 0 && t_sec < 2) {
          if (!midnightDrawn) {
            midnightDrawn = true;
            t_sec_alt     = t_sec;                                                       // Sekundenzähler synchron halten
            uiTransition(UI_CLOCK); // Mitternacht: menu() zeichnet Seite komplett + ruft display.display() intern
          }
        } else {
          midnightDrawn = false;
          if (t_sec != t_sec_alt) {
            t_sec_alt = t_sec;
            cleanTXT(20, 0, 120, 16);
            zeigeZ16C(64, 0, zeit);
            display.display();                                                           // Uhrzeitzeile übertragen
          }
        }
      }

      // Auto-Rückkehr: nur wenn nicht UI_CLOCK,
      // und letzter Touch-Event mindestens AUTO_RETURN_MS (20 s) zurückliegt.
      if (uiState != UI_CLOCK &&
          (millis() - lastTouchMs >= AUTO_RETURN_MS)) {
        uiTransition(UI_CLOCK);  // Auto-Rückkehr: menu() übernimmt display.display()
      }

      // 10v00: OLED nach DISPLAY_TIMEOUT_MS (5 min) ohne Touch-Event abschalten.
      // Wecken erfolgt in inputTask bei Berührung eines Touchpads.
      if (!displayBlanked &&
          (millis() - lastTouchMs >= DISPLAY_TIMEOUT_MS)) {
        display.displayOff();
        displayBlanked = true;
      }

      xSemaphoreGive(displayMutex);
    }

    // 12v02: Max. Einschaltzeit Licht/Mühlrad (Zugschalter S2) auf
    // S2_TIMEOUT_MS (30 min) begrenzen – analog Auto-Rückkehr der Menü-
    // Seiten. Außerhalb displayMutex: betrifft nur GPIO/S2_SW (kein
    // Display). displayTask + inputTask laufen beide auf Core 1, S2_SW
    // und t_start_S2 sind volatile. Schaltet E2 (Motor-PWM) + E3 (Licht)
    // ab und setzt S2_SW zurück; die Checkbox-Konfig (light_on/wheel_on)
    // bleibt unverändert. Spiegelt den OFF-Zweig des S2-Handlers.
    if (S2_SW && (millis() - t_start_S2 >= S2_TIMEOUT_MS)) {
      S2_SW = false;
      motorStop();                                                                       // 12v03: Motor-PWM abschalten
      digitalWrite(E3, LOW);
    }

    vTaskDelay(pdMS_TO_TICKS(DISPLAY_UPDATE_MS));
  }
}



// =============================================================
//  Task 4 – alarmTask  (Core 0, Pri 2)
//
//  Führt Alarm- und Kuckuck-State-Machine aus.
// =============================================================
static void alarmTask(void *pvParam) {
  esp_task_wdt_add(NULL);          // Hardware-TWDT: diesen Task anmelden
  while (true) {
    esp_task_wdt_reset();          // Hardware-TWDT zurücksetzen – alle 500 ms
    // 20v04 (A5-Fix): keine Alarm-/Kuckuck-Auswertung auf ungültiger Uhr
    // (kein NTP-Sync, Systemzeit noch auf Epoch 0 seit Boot).
    if (!timeValid()) {
      wdg_alarmTask = millis();                                                        // Alive-Signal: alarmTask aktiv (Gate ist kein Freeze)
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }
    // Atomarer Zeitschnappschuss – verhindert Race Condition mit displayTask:
    // displayTask (Core 1) und alarmTask (Core 0) laufen parallel – showTime()
    // könnte t_sec/t_min/t_hour gerade inkonsistent beschreiben.
    // localtime_r() ist re-entrant und schreibt nur in den lokalen Puffer.
    time_t    now_alarm;
    struct tm tm_alarm;
    time(&now_alarm);
    localtime_r(&now_alarm, &tm_alarm);
    const uint8_t  a_sec  = (uint8_t)tm_alarm.tm_sec;
    const uint8_t  a_min  = (uint8_t)tm_alarm.tm_min;
    const uint8_t  a_hour = (uint8_t)tm_alarm.tm_hour;
    const uint16_t a_yday = (uint16_t)tm_alarm.tm_yday;                                // 20v04: Tag im Jahr – Basis der Tages-Sperren (A1/A6)
    runAlarmMachine(a_sec, a_min, a_hour, a_yday);
    runCuckooMachine(a_sec, a_min, a_hour, a_yday);
    wdg_alarmTask = millis();                                                          // Alive-Signal: alarmTask aktiv
    vTaskDelay(pdMS_TO_TICKS(500)); // 500 ms Takt – ausreichend für Sekundengenauigkeit
  }
}



// =============================================================
//  Task 5 – wifiTask  (Core 0, Pri 1)
//
//  WiFi.begin() statt disconnect()+reconnect(): reconnect() ruft
//  intern disconnect() auf und unterbricht dabei den SNTP-Client.
//  WiFi.begin() mit denselben Credentials verbindet sauber neu,
//  ohne den SNTP-Task zu stören.
// =============================================================
static void wifiTask(void *pvParam) {
  while (true) {
    if (WiFi.status() != WL_CONNECTED && delayFunction(t_start7, WIFI_RECONNECT_MS)) {
      // 11v00: tmp-Puffer nur dann neu beschreiben, wenn displayTask das
      // vorherige Paar bereits übernommen hat (wifiSyncPending == false).
      // Ohne diesen Guard könnte wifiTask mitten in den memcpy der
      // displayTask hineinschreiben → Torn-Read in datum_WiFi/zeit_WiFi.
      // Lokaler Snapshot via localtime_r() – thread-safe, kein Mutex nötig.
      if (!wifiSyncPending) {
        time_t    now_local;
        struct tm tm_local;
        time(&now_local);
        localtime_r(&now_local, &tm_local);
        snprintf(datum_WiFi_tmp, sizeof(datum_WiFi_tmp), "%04u%02u%02u", tm_local.tm_year + 1900, tm_local.tm_mon + 1, tm_local.tm_mday);
        snprintf(zeit_WiFi_tmp,  sizeof(zeit_WiFi_tmp),  "%02u:%02u:%02u", tm_local.tm_hour, tm_local.tm_min, tm_local.tm_sec);
        wifiSyncPending = true;  // displayTask überträgt unter displayMutex → kein torn read
      }
      WiFi.begin(sta_ssid, sta_psk);                                                     // Laufzeit-Credentials aus NVR
      t_start7 = millis();
    }
    vTaskDelay(pdMS_TO_TICKS(WIFI_RECONNECT_MS));
  }
}



// =============================================================
//  Task 6 – nvrTask  (Core 0, Pri 1)
// =============================================================
static void nvrTask(void *pvParam) {
  while (true) {
    xSemaphoreTake(nvrSemaphore, portMAX_DELAY);
    // 20v07 (D1-Fix, Audit 2026-08-13): Rückgabewert von data.begin() prüfen –
    // schlägt das Öffnen fehl, liefen bisher alle putBool/putInt-Aufrufe in
    // writeNVR() als No-Op ins Leere (Preferences-Standardverhalten bei nicht
    // gestartetem Handle), die soeben geänderte Einstellung ging beim
    // nächsten Stromausfall ersatzlos verloren, ohne dass irgendetwas darauf
    // hindeutete. markSafeChange() löst nach NVR_COMMIT_DELAY_MS automatisch
    // einen neuen Versuch aus, statt den Commit stillschweigend zu verwerfen.
    if (data.begin("varSafe", ReadWrite)) {
      writeNVR();
      data.end();
    } else {
      webLog("[FEHLER] NVR: data.begin() fehlgeschlagen – Aenderung nicht gespeichert, naechster Versuch folgt");
      markSafeChange();
    }
  }
}



// =============================================================
//  Task 7 – stackMonTask  (Core 0, Pri 1)
//
//  Aktualisiert alle STACK_MON_INTERVAL_MS (60 s) den
//  Stack-HWM-Snapshot (Bytes) und freien Heap für die Web-Log-Seite.
//  Ein Wert nahe 0 zeigt drohenden Stack-Überlauf an.
// =============================================================
static void stackMonTask(void *pvParam) {
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(STACK_MON_INTERVAL_MS));
    updateSnapStack();                                                                     // Snapshot aktualisieren (kein Ring-Puffer-Eintrag)
  }
}



// =============================================================
//  Task 8 – watchdogTask  (Core 0, Pri 1)
//
//  Anwendungs-Watchdog: überwacht ob inputTask, displayTask und alarmTask
//  noch regelmäßig laufen. Jeder Task setzt seinen wdg_*-Timestamp in
//  jedem Zyklus. Der Watchdog prüft alle WDG_CHECK_MS ob der Timestamp
//  jünger als WDG_TIMEOUT_MS ist (Werte siehe SysConf, 20v08 auf 10 s/1 s
//  gesenkt, damit dieser Task wieder vor dem 15-s-Hardware-TWDT feuert).
//
//  Schützt gegen echte Deadlocks (Task hängt komplett).
//  Begrenzt NICHT die Alarm-Dauer: alarmTask läuft in ALARM_RUNNING
//  weiterhin alle 500 ms und setzt seinen Timestamp.
// =============================================================
static void watchdogTask(void *pvParam) {
  // Kurze Startpause: alle Tasks sollen sich einmal initialisiert haben
  vTaskDelay(pdMS_TO_TICKS(WDG_TIMEOUT_MS));

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(WDG_CHECK_MS)); // Prüfintervall aus SysConf (20v08: 1 s, war 5 s)
    uint32_t now = millis();

    bool inputOk   = (now - wdg_inputTask)   < WDG_TIMEOUT_MS;
    bool displayOk = (now - wdg_displayTask) < WDG_TIMEOUT_MS;
    bool alarmOk   = (now - wdg_alarmTask)   < WDG_TIMEOUT_MS;

    if (!inputOk || !displayOk || !alarmOk) {
      webLog("[WATCHDOG] Task-Freeze erkannt!");
      if (!inputOk)   webLogf("  inputTask   : %lu ms ohne Lebenszeichen", now - wdg_inputTask);
      if (!displayOk) webLogf("  displayTask : %lu ms ohne Lebenszeichen", now - wdg_displayTask);
      if (!alarmOk)   webLogf("  alarmTask   : %lu ms ohne Lebenszeichen", now - wdg_alarmTask);
      // 12v12: Fror alarmTask mitten in einem triggerAlarm()-Versuch ein
      // (alarmTriggerInFlight, siehe dort), hätte der reguläre DFPlayer-
      // Absturz-Neustart in triggerAlarm() selbst nie stattgefunden – ohne
      // diesen Fallback bliebe der Alarm nach diesem Neustart ersatzlos aus.
      // Nur für alarmTask, nicht für input-/displayTask-Freezes (die stehen
      // in keinem Zusammenhang mit einem laufenden Alarm-Versuch).
      // 20v06 (B2-Fix): rtcRetryMagic wird seit 20v06 bereits am Anfang von
      // triggerAlarm() gesetzt – dieser Block schreibt dieselben Werte daher
      // in der Praxis redundant (Sicherheitsnetz, falls ein TWDT-Panic statt
      // eines normalen Freeze-Erkennens zuschlägt, bevor die 30-s-Schwelle
      // hier überhaupt geprüft wird). Bewusst nicht entfernt.
      if (!alarmOk && alarmTriggerInFlight) {
        rtcRetryAlarm  = alarmTriggerNum;
        rtcRetryFileNo = alarmTriggerFileNo;
        rtcRetryMin    = alarmTriggerMin;
        rtcRetryCount  = alarmTriggerFailCount;
        rtcRetryMagic  = RTC_RETRY_MAGIC;
        webLogf("[WATCHDOG] alarmTask fror während Alarm-Versuch ein (%s, Datei %d) – Ersatzalarm nach Neustart vorgemerkt",
                (alarmTriggerNum == 1) ? "Alarm 1" : "Alarm 2", alarmTriggerFileNo);
      }
      // Display-Meldung nur wenn displayTask noch läuft (sonst I2C-Zugriff riskant)
      if (displayOk) {
        if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
          display.clear();
          zeigeZ10C(64,  8, "WATCHDOG");
          zeigeZ10C(64, 24, "Task Freeze!");
          zeigeZ10C(64, 40, "Neustart...");
          display.display();
          xSemaphoreGive(displayMutex);
        }
      }
      vTaskDelay(pdMS_TO_TICKS(2000));  // Meldung kurz lesbar halten
      ESP.restart();
    }
  }
}



// =============================================================
//  Task 9 – webLogTask  (Core 0, Pri 1)
//
//  Startet HTTP-Server auf Port WEBLOG_PORT sobald WiFi bereit.
//  GET /     → HTML-Seite mit Auto-Refresh (alle 20 s)
//  GET /log  → Ring-Puffer als plain text (neueste Zeilen zuerst)
//  Der Task blockiert bis WiFi verbunden ist (prüft alle 2 s).
//  Aktiv sobald sich ein Client verbindet.
// =============================================================
static void webLogTask(void *pvParam) {
  // Warten bis WiFi steht
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(2000));
  }

  WebServer logServer(WEBLOG_PORT);

  // GET / → HTML-Seite
  logServer.on("/", HTTP_GET, [&logServer]() {
    String ip = WiFi.localIP().toString();
    // 9v13: reserve() verhindert inkrementelle Reallokationen beim
    // String-Zusammenbau – jede += kann den Puffer verdoppeln und
    // alten Heap-Block freigeben → Fragmentierung. 8 kB reicht für
    // CSS + 40 Log-Zeilen + Snapshots ohne Nachallokation.
    String html;
    html.reserve(8192);
    html +=
      "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
      "<meta http-equiv='refresh' content='20'>"
      "<title>bTn Wecker Log</title>"
      "<style>"
      "body{font-family:monospace;background:#1a1a2e;color:#e0e0e0;margin:0;padding:16px}"
      "h2{color:#BDD7EE;margin-bottom:8px;font-size:1.6rem}"
      "h3{color:#888;font-weight:normal;font-size:1rem;margin:0 0 12px}"
      ".snap-wrap{margin-bottom:16px}"
      ".snap-title{font-size:1rem;color:#78909c;margin-bottom:4px}"
      ".snap-title .ts{color:#4A9EFF;font-weight:bold}"
      ".snap-box{background:#0d1a0d;border:1px solid #2a4a2a;border-radius:6px;padding:10px;"
      "white-space:pre;overflow-x:auto;font-size:19px;color:#b0d0b0}"
      "#log,#dflog{background:#0d0d1a;border:1px solid #333;border-radius:6px;padding:12px;"
      "white-space:pre;overflow-x:auto;max-height:60vh;overflow-y:auto;font-size:19px}"
      "#dflog{margin-bottom:16px}"
      ".ok{color:#6BCB77}.err{color:#FF6B6B}.warn{color:#FFD93D}"
      ".sec-title{font-size:1rem;color:#78909c;margin:16px 0 4px}"
      "</style></head><body>"
      "<h2>&#x1F553; bTn Wecker " FW_VERSION " &ndash; Web-Log</h2>"
      "<h3>IP: " + ip + ":" + String(WEBLOG_PORT) + " &nbsp;|&nbsp; Auto-Refresh: 20 s"
      " &nbsp;|&nbsp; Aktualisiert: <span id='upd'></span></h3>";

    // ── 12v03: Mühlrad-Motor Pulsweiten-Slider ───────────────
    // GET-Form auf /motor (0..100 %). Auto-Refresh (20 s) lädt den
    // aktuellen Wert nach; oninput aktualisiert die %-Anzeige live.
    {
      int mpct = ((int)motor_duty * 100 + 127) / 255;                 // 0..255 → 0..100 % (gerundet)
      String mp = String(mpct);
      html += "<div class='sec-title'>M&uuml;hlrad-Motor &ndash; Pulsweite (Drehzahl)</div>"
              "<form action='/motor' method='get' class='snap-box' style='color:#b0d0b0'>"
              "Duty: <output id='dv'>" + mp + "</output> %"
              " &nbsp;<input type='range' name='duty' min='0' max='100' value='" + mp + "'"
              " style='vertical-align:middle;width:55%' oninput='dv.value=this.value'>"
              " &nbsp;<button type='submit'>Setzen</button>"
              "<div style='color:#78909c;font-size:0.85rem;margin-top:6px'>"
              "&lt; 35 % &rarr; Kickstart-Anlaufimpuls &middot; Wert wird in NVS gespeichert</div>"
              "</form>";
    }

    // ── DFPlayer: eigener Abschnitt mit allen DFPlayer-Meldungen ──
    {
      String alarmTs = strlen(snapAlarmTime) > 0 ? String(snapAlarmTime) : String("–");
      html += "<div class='sec-title'>DFPlayer &ndash; letzter erfolgreicher Alarm: "
              "<span style='color:#4A9EFF'>" + alarmTs + "</span></div>";
    }
    html += "<div id='dflog'>";
    // ── Ring-Puffer: DFPlayer-Meldungen und allgemeine Meldungen getrennt ──
    String generalLog;
    if (webLogMutex && xSemaphoreTake(webLogMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
      uint16_t start = (webLogCount < WEBLOG_LINES)
                     ? 0
                     : webLogHead;                         // ältester Eintrag
      for (uint16_t i = 0; i < webLogCount; i++) {
        uint16_t idx = (start + i) % WEBLOG_LINES;
        String line = String(webLogBuf[idx]);
        bool isDfPlayer = line.indexOf("DFPlayer") >= 0;
        // [xxx]-Tag mit Leerzeichen auf feste Breite (WEBLOG_TAG_WIDTH)
        // bringen, damit der Text dahinter immer in derselben Spalte beginnt
        if (line.length() > 0 && line[0] == '[') {
          int close = line.indexOf(']');
          if (close >= 0 && (close + 1) < WEBLOG_TAG_WIDTH) {
            String rest = line.substring(close + 1);
            line = line.substring(0, close + 1);
            while ((int)line.length() < WEBLOG_TAG_WIDTH) line += ' ';
            line += rest;
          }
        }
        String entry;
        if (line.indexOf("[WATCHDOG]") >= 0 || line.indexOf("[PANIC]") >= 0 || line.indexOf("[FEHLER]") >= 0 || line.indexOf("failed") >= 0)
          entry += "<span class='err'>";
        else if (line.indexOf("OK") >= 0 || line.indexOf("ready") >= 0 || line.indexOf("connected") >= 0)
          entry += "<span class='ok'>";
        else if (line.indexOf("Timeout") >= 0 || line.indexOf("Warnung") >= 0)
          entry += "<span class='warn'>";
        else
          entry += "<span>";
        line.replace("<", "&lt;"); line.replace(">", "&gt;");
        entry += line + "</span>\n";
        if (isDfPlayer) html += entry;
        else            generalLog += entry;
      }
      xSemaphoreGive(webLogMutex);
    }
    html += "</div>";

    // ── Allgemeines Log ────────────────────────────────────────
    {
      String ntpTs = strlen(snapNtpTime) > 0 ? String(snapNtpTime) : String("–");
      html += "<div class='sec-title'>Allgemeines Log &ndash; letzter Reset: "
              "<span style='color:#4A9EFF'>" + ntpTs + "</span></div>";
    }
    html += "<div id='log'>" + generalLog + "</div>";

    // ── Verbindung: letzter Restart (WiFi + NTP analog Info-Seite) ─
    {
      String wDate = strlen(datum_WiFi) > 0 ? String(datum_WiFi) : String("–");
      String wTime = strlen(zeit_WiFi)  > 0 ? String(zeit_WiFi)  : String("–");
      String nDate = strlen(datum_sync) > 0 ? String(datum_sync) : String("–");
      String nTime = strlen(zeit_sync)  > 0 ? String(zeit_sync)  : String("–");
      html += "<div class='snap-wrap'>"
              "<div class='snap-title'>Verbindung &ndash; letzter WiFi Reconnect / NTP Sync</div>"
              "<div class='snap-box'>"
              "  WiFi  " + wDate + "  " + wTime + "\n"
              "  NTP   " + nDate + "  " + nTime +
              "</div></div>";
    }

    // ── Snapshot: Touch Baseline + Stack HWM ─────────────────
    if (webLogMutex && xSemaphoreTake(webLogMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
      String touchTime = String(snapTouchTime);
      String touchContent = String(snapTouchBuf);
      String stackTime = String(snapStackTime);
      String stackContent = String(snapStackBuf);
      xSemaphoreGive(webLogMutex);

      touchContent.replace("<", "&lt;"); touchContent.replace(">", "&gt;");
      stackContent.replace("<", "&lt;"); stackContent.replace(">", "&gt;");

      html += "<div class='snap-wrap'>"
              "<div class='snap-title'>Touch Baseline – letzte Kalibrierung: "
              "<span class='ts'>" + touchTime + "</span></div>"
              "<div class='snap-box'>" + touchContent + "</div></div>";

      html += "<div class='snap-wrap'>"
              "<div class='snap-title'>Stack High-Water Marks – letzte Messung: "
              "<span class='ts'>" + stackTime + "</span></div>"
              "<div class='snap-box'>" + stackContent + "</div></div>";
    }
    html += "<script>document.getElementById('upd').textContent=new Date().toLocaleTimeString();</script></body></html>";
    logServer.send(200, "text/html; charset=UTF-8", html);
  });

  // GET /log → plain text für curl / wget
  logServer.on("/log", HTTP_GET, [&logServer]() {
    String out = "";
    if (webLogMutex && xSemaphoreTake(webLogMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
      uint16_t start = (webLogCount < WEBLOG_LINES) ? 0 : webLogHead;
      for (uint16_t i = 0; i < webLogCount; i++) {
        out += String(webLogBuf[(start + i) % WEBLOG_LINES]) + "\n";
      }
      xSemaphoreGive(webLogMutex);
    }
    logServer.send(200, "text/plain; charset=UTF-8", out);
  });

  // 12v03: GET /motor?duty=NN (0..100 %) → Mühlrad-Pulsweite zur Laufzeit
  // setzen. Live-Übernahme falls Motor gerade läuft; Persistenz über die
  // bestehende safeChange→nvrSemaphore→nvrTask-Kette (kein Flash-Zugriff
  // aus dem HTTP-Handler). Antwort 303 → zurück auf die Log-Seite.
  logServer.on("/motor", HTTP_GET, [&logServer]() {
    if (logServer.hasArg("duty")) {
      long pct = logServer.arg("duty").toInt();
      if (pct < 0)   pct = 0;
      if (pct > 100) pct = 100;
      uint8_t d = (uint8_t)((pct * 255 + 50) / 100);                 // % → 0..255 (gerundet)
      motor_duty = d;
      if (motorRunning) ledcWrite(E2, d);                            // Live-Übernahme, falls Motor läuft
      markSafeChange();                                              // nvrTask sichert nach Ruhezeit
      webLogf("[MOTOR] Duty -> %ld %% (%u/255)", pct, (unsigned)d);
    }
    logServer.sendHeader("Location", "/");
    logServer.send(303, "text/plain", "");
  });

  logServer.begin();
  webLogf("[RESET] resetCount: %lu", (unsigned long)resetCount);

  while (true) {
    logServer.handleClient();                              // eingehende Requests verarbeiten
    vTaskDelay(pdMS_TO_TICKS(10));                        // 10 ms Pause – verhindert WDT-Reset
  }
}

// =============================================================
//  rtosPanic() – FreeRTOS-Objekt- / Task-Erstellungsfehler
//
//  Wird aufgerufen wenn xQueueCreate, xSemaphoreCreate* oder
//  xTaskCreatePinnedToCore nullptr / pdFAIL zurückgeben.
//  Zeigt Fehlertext auf OLED + Serial, startet nach 3 s neu.
//  Heap-Mangel ist die häufigste Ursache – Neustart schafft
//  einen sauberen Zustand.
// =============================================================
static void rtosPanic(const char* what) {
  Serial.print("\n[PANIC] FreeRTOS-Fehler: ");
  Serial.println(what);
  display.clear();
  zeigeZ10C(64,  8, "RTOS FEHLER");
  zeigeZ10C(64, 24, what);
  zeigeZ10C(64, 40, "Neustart in 3s");
  display.display();
  delay(3000);
  ESP.restart();
}

// =============================================================
//  setup()
// =============================================================
void setup() {
  pinMode(17, OUTPUT);
  digitalWrite(17, LOW);
  delay(3000);

  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  delay(2000);
  bTn_info();

  // ── webLogMutex früh initialisieren: setup()-Meldungen puffern ─
  // 20v07 (D1-Fix): VOR das NVR-Laden gezogen, damit ein data.begin()-
  // Fehlschlag dort sofort als [FEHLER] ins Web-Log geschrieben werden kann
  // (webLog() puffert erst still, solange der Mutex noch nicht existiert).
  webLogMutex = xSemaphoreCreateMutex();
  if (!webLogMutex) rtosPanic("webLogMutex");

  // ── NVR laden ────────────────────────────────────────────
  // 20v07 (D1-Fix, Audit 2026-08-13): Rückgabewert von data.begin() geprüft –
  // schlägt das Öffnen fehl (beschädigter NVS, ESP_ERR_NO_MEM), arbeitete die
  // Firmware bisher stillschweigend mit den Compile-Defaults statt einer
  // Fehlermeldung. readNVR() läuft jetzt unabhängig vom state-Flag: die
  // getX()-Fallbacks liefern beim allerersten Boot ohnehin die Compile-
  // Defaults zurück (Namespace noch leer), ein bedingter Aufruf ist unnötig.
  if (data.begin("varSafe", ReadWrite)) {
    bool varState = data.getBool("state", false);
    readNVR();
    if (!varState) {
      data.putBool("state", true); // Erststart-Flag dauerhaft setzen
      writeNVR();                  // aktuelle (Compile-)Defaults als Baseline persistieren
    }
    data.end();
  } else {
    webLog("[FEHLER] NVR: data.begin() beim Start fehlgeschlagen – Firmware laeuft mit Compile-Defaults");
  }
  // bumpResetCount() folgt bewusst erst nach loadWifiCredentials() – siehe dort.

  // ── Display init (wird auch von runWifiConfigServer genutzt) ─
  display.init();
  display.flipScreenVertically();
  display.clear();
  zeigeZ10C(64, 16, PGMInfo);
  display.display();                                                                   // Versionsstring anzeigen

  // ── WiFi-Credentials aus NVR laden ───────────────────────
  // Erster Start oder NVR-Flag gelöscht (z.B. via T3 auf Info, 11v05):
  // → WiFi-Konfigurator starten (blockiert bis Neustart).
  if (!loadWifiCredentials()) {
    Serial.println("[WiFi-Config] Keine Zugangsdaten – starte Konfigurator");
    runWifiConfigServer();   // kehrt nicht zurück (ESP.restart am Ende)
  }
  // 11v03: erst JETZT zählen – der Konfigurator-Boot nach Werksreset (NVS leer,
  // loadWifiCredentials() false → ESP.restart in runWifiConfigServer) wird damit
  // übersprungen. Der nachfolgende reguläre Boot landet sauber auf resetCount=1.
  bumpResetCount();
  webLogf("[WiFi] Credentials geladen: SSID=%s", sta_ssid);

  // ── NTP ──────────────────────────────────────────────────
  sntp_set_time_sync_notification_cb(timeavailable);
  configTime(0, 0, MY_NTP_SERVER);
  setenv("TZ", MY_TZ, 1);
  tzset();

  // ── WiFi verbinden ───────────────────────────────────────
  display.clear();
  zeigeZ10C(64, 8, PGMInfo);
  zeigeZ16C(64, 32, "warte auf");
  zeigeZ16C(64, 49, "WLAN ...");
  display.display();                                                                   // WiFi-Wartebildschirm anzeigen
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(sta_ssid, sta_psk);                                                        // Laufzeit-Credentials aus NVR
  Serial.println("\nwarte auf WiFi");
  bool wifiConnected = false;                                                            // ersetzt goto wifi_skip
  {
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED) {
      if (millis() - t0 >= SETUP_WIFI_TIMEOUT_MS) {
        Serial.println("\nWiFi Timeout – starte ohne WLAN");
        cleanTXT(0, 32, 128, 32);
        zeigeZ16C(64, 32, "kein WLAN");
        zeigeZ10C(64, 49, "weiter ohne NTP");
        display.display();                                                             // Timeout-Meldung anzeigen
        delay(2000);
        break;                                                                           // wifiConnected bleibt false → NTP-Block wird übersprungen
      }
      delay(500);
      Serial.print(".");
    }
    wifiConnected = (WiFi.status() == WL_CONNECTED);
  }
  if (wifiConnected) {
    // Ausnahme zur Projektregel "nach WiFi-Connect nur webLogf()":
    // Die Web-Log-Adresse MUSS im Serial-Monitor erscheinen, sonst ist
    // das Web-Log praktisch unerreichbar – die URL steht ja erst im
    // Web-Log selbst, das ohne bekannte Adresse nicht aufgerufen werden
    // kann. Dieses eine Serial.printf ist daher betrieblich notwendig.
    Serial.printf("\nWiFi connected – IP: %s  Log: http://%s:%u\n",
                  WiFi.localIP().toString().c_str(),
                  WiFi.localIP().toString().c_str(),
                  (unsigned)WEBLOG_PORT);
    webLog("[WiFi] connected");
    webLogf("[WiFi] IP: %s", WiFi.localIP().toString().c_str());

    // ── NTP warten ─────────────────────────────────────────
    cleanTXT(0, 49, 128, 15);
    zeigeZ16C(64, 49, "NTP ...");
    display.display();                                                                 // NTP-Wartemeldung anzeigen
    webLog("[NTP] warte auf Synchronisation ...");
    {
      uint32_t t0 = millis();
      while (timeinfo.tm_year < 71) {
        if (millis() - t0 >= SETUP_NTP_TIMEOUT_MS) {
          webLog("[NTP] Timeout – Uhr nicht gestellt");
          cleanTXT(0, 49, 128, 15);
          zeigeZ10C(64, 49, "NTP Timeout");
          display.display();                                                           // NTP-Timeout-Meldung anzeigen
          delay(2000);
          break;
        }
        showTime();
        delay(500);
        // kein Web-Log für Fortschrittspunkte
      }
    }
    webLog("[NTP] Synchronisation OK");
    snapTimeStr(snapNtpTime, sizeof(snapNtpTime));
  }

  // ── GPIO ─────────────────────────────────────────────────
  pinMode(S1, INPUT_PULLUP);
  pinMode(S2, INPUT_PULLUP);
  pinMode(S3, INPUT_PULLUP);
  pinMode(E1, OUTPUT);
  pinMode(DFPLAYER_BUSY, INPUT);                                                        // 20v00: input-only Pin, kein Pull-up möglich/nötig (BUSY treibt aktiv)
  // 12v00: E2 (Motor) wird per LEDC-PWM angesteuert – ledcAttach konfiguriert
  // den Pin als Ausgang und ordnet ihn einem LEDC-Kanal zu; pinMode entfällt.
  // MOSFET-Gate hat einen 10 kΩ Pull-Down, also bleibt der Motor während der
  // kurzen Boot-Phase vor ledcAttach sicher aus.
  ledcAttach(E2, MOTOR_PWM_FREQ, MOTOR_PWM_RES);
  ledcWrite(E2, 0);                                                                      // definierter Startzustand: Motor aus
  pinMode(E3, OUTPUT);

  // ── FreeRTOS Objekte ─────────────────────────────────────
  // webLogMutex wurde bereits früh in setup() erstellt (vor den webLog-Aufrufen)
  inputQueue   = xQueueCreate(32, sizeof(uint8_t));
  displayMutex = xSemaphoreCreateMutex();
  playerMutex  = xSemaphoreCreateMutex();
  nvrSemaphore = xSemaphoreCreateBinary();
  if (!inputQueue)   rtosPanic("inputQueue");
  if (!displayMutex) rtosPanic("displayMutex");
  if (!playerMutex)  rtosPanic("playerMutex");
  if (!nvrSemaphore) rtosPanic("nvrSemaphore");

  // ── Taster-Interrupts ────────────────────────────────────
  attachInterrupt(S1, isrS1, FALLING);
  attachInterrupt(S2, isrS2, FALLING);
  attachInterrupt(S3, isrS3, FALLING);

  // ── DFPlayer ─────────────────────────────────────────────
  cleanTXT(0, 49, 128, 15);
  zeigeZ16C(64, 49, "Sound ...");
  display.display();                                                                   // DFPlayer-Initialisierung anzeigen
  // 13v00: ACK-Modus zunächst deaktiviert, dann noch am selben Tag wieder
  // aktiviert (Regression): sendStack() der Bibliothek wartet bei aktivem
  // ACK vor jedem neuen Befehl blockierend auf das 0x41-ACK des vorherigen
  // (while(_isSending){waitAvailable();}) – ohne ACK sendet sie stattdessen
  // nur ein pauschales 10-ms-Delay, unabhängig davon, ob der DFPlayer den
  // vorherigen Befehl schon verarbeitet hat. Direkt nach reset() folgen hier
  // volume()/EQ()/playFolder() (Startsound) unmittelbar hintereinander –
  // ohne die ACK-Wartelogik kam der DFPlayer damit nicht mehr mit, der
  // Startsound blieb aus. Der 0x41-ACK-Frame war ohnehin nie die Ursache der
  // "kein Start-Status"-Fehlschläge (siehe readStateDrained()) – ACK bleibt
  // daher aktiv.
  if (player.begin(Serial2, true, true)) {
    webLog("[DFPlayer] Serial2 OK");
    // readFileCountsInFolder(1) als Bereitschaftsprüfung: DFPlayer antwortet
    // erst, wenn SD-Karte vollständig indiziert ist. Nach Power-On/Flash
    // dauert das länger als nach Reset-Taste (DFPlayer bleibt dort unter
    // Spannung). Erst danach playFolder aufrufen – kein UART-Verkehr mehr
    // während der Wiedergabe.
    // 20v10 (C4-Fix, Audit 2026-08-13): bisher player.readFileCounts() (0x48,
    // Gesamtzahl über ALLE Ordner) verwendet und mp3Count = c - 1 gerechnet,
    // in der Annahme, genau eine Datei liege außerhalb von Ordner 01 (der
    // Startsound in Ordner 02) – zusätzliche Dateien in Ordner 02 oder vom
    // Betriebssystem beim Kopieren angelegte Extra-Dateien machten diese
    // Zahl falsch (wählbare, nicht existierende Alarmdatei → Reboot-Kaskade,
    // siehe C3). player.readFileCountsInFolder(1) (0x4E) liefert die Anzahl
    // direkt für Ordner 01, kein Rechnen/Raten mehr nötig. Bei Timeout bleibt
    // mp3Count bewusst auf 0 statt eines geratenen Fallbacks (war: 99) – C5
    // sperrt die Sound-Auswahl dann korrekt, statt eine falsche Dateizahl
    // vorzugaukeln.
    {
      uint32_t t0 = millis();
      while (mp3Count < 1) {
        if (millis() - t0 >= SETUP_MP3_TIMEOUT_MS) {
          webLog("[FEHLER] DFPlayer-Dateizahl unbekannt (Timeout) – Sound-Auswahl bleibt gesperrt");
          break;                                                                         // mp3Count bleibt 0 – kein geratener Fallback mehr (C4)
        }
        checkSerial2Leftover("readFileCountsInFolder (setup)");
        int16_t c = player.readFileCountsInFolder(1);
        if (c > 0) mp3Count = c;
      }
    }
    checkSerial2Leftover("volume (setup)");
    player.volume(vol);
    checkSerial2Leftover("EQ (setup)");
    player.EQ(DFPLAYER_EQ_BASS);
    checkSerial2Leftover("playFolder (setup Startsound)");
    player.playFolder(2, 1);
    delay(4000);
    playerStatus = 1;
  } else {
    webLog("[DFPlayer] Verbindung fehlgeschlagen!");
  }
  // 20v07 (C5-Fix, Audit 2026-08-13): Clamp nur bei bekannter Dateizahl. Bei
  // mp3Count==0 (player.begin() fehlgeschlagen) war die Bedingung unten sonst
  // IMMER wahr – readNVR() clamped sound*_assigned nur nach UNTEN auf 1, nie
  // nach oben – und überschrieb beide Zuordnungen mit 1. Verstellt der Nutzer
  // danach irgendetwas (markSafeChange()), landet dieser falsche Wert
  // unwiederbringlich in NVS, während die eigentliche Melodie verloren geht.
  if (mp3Count > 0) {
    if (sound1_assigned > mp3Count) sound1_assigned = 1;                                    // NVR-Wert > SD-Inhalt abfangen
    if (sound2_assigned > mp3Count) sound2_assigned = 1;
  } else {
    webLog("[FEHLER] DFPlayer-Dateizahl unbekannt – Sound-Zuordnung unveraendert");
  }
  snprintf(str_mp3, sizeof(str_mp3), "%03u", mp3Count);
  snprintf(str_reset, sizeof(str_reset), "%04lu", (unsigned long)resetCount);            // rechtsbündig 4-stellig
  webLogf("[DFPlayer] mp3Count: %d", mp3Count);

  // ── DFPlayer-Absturz-Neustart: verpassten Alarm erneut auslösen ──
  // rtcRetryMagic übersteht ESP.restart() (RTC_NOINIT_ATTR) und wird nur
  // durch triggerAlarm()/verifyPlayStarted() bei fehlgeschlagenem Play-
  // Befehl gesetzt – ein echter Power-On liefert hier undefinierten
  // Speicherinhalt, daher die Prüfung gegen RTC_RETRY_MAGIC statt nur != 0.
  // rtcRetryCount (12v07) läuft mit durch triggerAlarm() – ab
  // ALARM_MAX_RESTARTS Fehlversuchen bricht triggerAlarm() endgültig ab.
  if (rtcRetryMagic == RTC_RETRY_MAGIC) {
    rtcRetryMagic = 0;                                                                    // Merker sofort löschen – kein Retry-Loop bei erneutem Fehlschlag
    webLogf("[DFPlayer] Nach Neustart: %s wird erneut ausgelöst (Datei %d, Versuch %u/%u)",
            (rtcRetryAlarm == 1) ? "Alarm 1" : "Alarm 2", rtcRetryFileNo, (unsigned)rtcRetryCount + 1, (unsigned)ALARM_MAX_RESTARTS);
    triggerAlarm(rtcRetryAlarm, rtcRetryFileNo, rtcRetryMin, rtcRetryCount);
  }

  // ── Startseite ───────────────────────────────────────────
  // 9v12: showTime() vor snprintf, damit timeinfo sicher gültig ist
  // (ohne WiFi/NTP wurde sie in der NTP-Warteschleife nie aufgerufen
  // und hätte "19000101" produziert). Fallback bei fehlendem NTP-Sync
  // schreibt Platzhalterstrings statt uninitialisierter Werte.
  showTime();
  if (wifiConnected && timeinfo.tm_year >= 71) {
    snprintf(datum_WiFi,  sizeof(datum_WiFi), "%04u%02u%02u", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
    snprintf(zeit_WiFi,   sizeof(zeit_WiFi),  "%02u:%02u:%02u", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  } else {
    snprintf(datum_WiFi,  sizeof(datum_WiFi), "--------");
    snprintf(zeit_WiFi,   sizeof(zeit_WiFi),  "--:--:--");
  }
  snprintf(str_a1,      sizeof(str_a1),      "%02u:%02u", a1_hour, a1_min);
  snprintf(str_a2,      sizeof(str_a2),      "%02u:%02u", a2_hour, a2_min);
  snprintf(str_s1,      sizeof(str_s1),      "%03u",      sound1_selected);
  snprintf(str_s2,      sizeof(str_s2),      "%03u",      sound2_selected);
  snprintf(str_s1_play, sizeof(str_s1_play), "%03u",      sound1_assigned);
  snprintf(str_s2_play, sizeof(str_s2_play), "%03u",      sound2_assigned);
  snprintf(str_vol,     sizeof(str_vol),     "%02u",      vol);
  snprintf(str_cot,     sizeof(str_cot),     "%02u",      cuckoo_onTime);
  snprintf(str_coff,    sizeof(str_coff),    "%02u",      cuckoo_offTime);
  uiTransition(UI_CLOCK);                                                               // initialer Zustand + Bildschirm

  // ── Hardware Task Watchdog Timer (TWDT) ──────────────
  // Initialisierung VOR Task-Start: esp_task_wdt_add() in den Task-Funktionen
  // setzt ein bereits existierendes TWDT voraus, sonst schlägt die Anmeldung
  // mit ESP_ERR_INVALID_STATE fehl (Rückgabewert dort ungeprüft) und jeder
  // spätere esp_task_wdt_reset() bricht mit "task not found" ab, weil der
  // Task nie wirklich angemeldet wurde – beobachtet am 10.08.2026 (12v14).
  // 12v16: Auf dieser Hardware ist das TWDT tatsächlich schon vor setup()
  // initialisiert (esp_task_wdt_init() meldete "already initialized") – die
  // ursprüngliche Annahme "Core 3.x init bereits beim Boot" war also richtig,
  // nur die Reihenfolge relativ zum Task-Start war falsch. Blindes
  // esp_task_wdt_init() erzeugt dabei zusätzlich ein ESP_LOGE (Seiteneffekt
  // der Bibliotheksfunktion, unabhängig vom Rückgabewert) – daher vorher per
  // esp_task_wdt_status(NULL) abfragen statt auf den Fehlschlag zu warten:
  // ESP_ERR_INVALID_STATE bedeutet "TWDT nie initialisiert", jeder andere
  // Rückgabewert (ESP_OK oder ESP_ERR_NOT_FOUND) bedeutet "existiert bereits".
  // trigger_panic=true: TWDT-Ablauf erzeugt Backtrace + Reset statt stiller Neustart.
  // Timeout WDT_HARDWARE_MS kürzer als Software-Watchdog WDG_TIMEOUT_MS:
  // Hardware greift bei echtem CPU-Lock, Software bei logischem Freeze.
  const esp_task_wdt_config_t twdt_cfg = {
    .timeout_ms    = WDT_HARDWARE_MS,  // aus SysConf_20v11.h
    .idle_core_mask = 0,               // Idle-Tasks nicht überwachen
    .trigger_panic  = true,            // Backtrace + Reset bei Ablauf
  };
  if (esp_task_wdt_status(NULL) == ESP_ERR_INVALID_STATE) {
    esp_task_wdt_init(&twdt_cfg);                                                        // TWDT existiert noch nicht – anlegen
  } else {
    esp_task_wdt_reconfigure(&twdt_cfg);                                                 // TWDT existiert schon (Boot-Default) – nur umkonfigurieren
  }
  webLogf("[TWDT] Hardware Watchdog aktiv (%u ms)", (unsigned)WDT_HARDWARE_MS);

  // ── FreeRTOS Tasks starten ───────────────────────────────
  // Jetzt garantiert NACH TWDT-Init: esp_task_wdt_add() in den Task-Funktionen
  // trifft immer auf ein bereits existierendes TWDT.
  if (xTaskCreatePinnedToCore(touchTask,    "touchTask",    STACK_TOUCH, nullptr, 2, &hTouchTask,   0) != pdPASS) rtosPanic("touchTask");   // Core 0, Prio 2
  if (xTaskCreatePinnedToCore(alarmTask,    "alarmTask",    STACK_ALARM, nullptr, 2, &hAlarmTask,   0) != pdPASS) rtosPanic("alarmTask");   // Core 0, Prio 2 – getrennt von inputTask
  if (xTaskCreatePinnedToCore(wifiTask,     "wifiTask",     STACK_WIFI, nullptr, 1, &hWifiTask,    0) != pdPASS) rtosPanic("wifiTask");    // Core 0, Prio 1
  if (xTaskCreatePinnedToCore(nvrTask,      "nvrTask",      STACK_NVR, nullptr, 1, &hNvrTask,     0) != pdPASS) rtosPanic("nvrTask");     // Core 0, Prio 1
  if (xTaskCreatePinnedToCore(stackMonTask, "stackMonTask", STACK_STACKMON, nullptr, 1, &hStackMonTask,   0) != pdPASS) rtosPanic("stackMonTask"); // Core 0, Prio 1
  if (xTaskCreatePinnedToCore(watchdogTask, "watchdogTask", STACK_WATCHDOG, nullptr, 1, &hWatchdogTask,   0) != pdPASS) rtosPanic("watchdogTask"); // Core 0, Prio 1
  if (xTaskCreatePinnedToCore(inputTask,    "inputTask",    STACK_INPUT, nullptr, 2, &hInputTask,   1) != pdPASS) rtosPanic("inputTask");   // Core 1, Prio 2
  if (xTaskCreatePinnedToCore(displayTask,  "displayTask",  STACK_DISPLAY, nullptr, 1, &hDisplayTask, 1) != pdPASS) rtosPanic("displayTask"); // Core 1, Prio 1
  if (xTaskCreatePinnedToCore(webLogTask,   "webLogTask",   STACK_WEBLOG,  nullptr, 1, &hWebLogTask,  0) != pdPASS) rtosPanic("webLogTask");  // Core 0, Prio 1
}



// =============================================================
//  loop() – leer
// =============================================================
void loop() {
  vTaskDelete(nullptr);
}
