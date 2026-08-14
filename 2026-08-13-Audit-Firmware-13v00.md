# Audit Firmware 13v00 – Zuverlässigkeit

## Gesamtbild

Die Firmware ist handwerklich deutlich besser als der Durchschnitt vergleichbarer Arduino-Projekte: die Projektregel „kein `vTaskDelay()` unter Mutex" wird in allen geprüften Pfaden tatsächlich eingehalten (`verifyPlayStarted()` Z. 1320, S1-Retry Z. 1715, ALARM_RUNNING-Poll Z. 1426/1428), der Zeitschnappschuss per `localtime_r()` je Task (Z. 1955-1961) eliminiert eine reale Race gegen `displayTask`, `rtosPanic()` verhindert einen halb gestarteten Systemzustand, `ALARM_MAX_RESTARTS` (12v10) und `SERIAL2_DRAIN_MAX_BYTES` (12v11) sind genau die Deckel, die Endlos-Neustartschleifen bzw. Endlos-Drains verhindern, und die TWDT-Initialisierung vor dem Task-Start (12v15/12v16) ist korrekt und richtig begründet. **Diese fünf Mechanismen bitte nicht wegoptimieren** – ebenso wenig den ACK-Modus (`player.begin(…, true, true)`, Z. 2485), dessen Deaktivierung in 13v00 bereits eine Regression war.

Das Restrisiko liegt nicht in der Nebenläufigkeit, sondern in drei strukturellen Punkten: **(A)** die Alarm-Fälligkeit ist eine reine Flanke auf `sec == 0` im Zustand `ALARM_IDLE` – jedes verpasste Ereignis (Zeitsprung, Reboot, blockierter Tick, laufender Vor-Alarm) löscht den Alarm des Tages ersatzlos und ohne einen einzigen Log-Eintrag; **(B)** die gesamte Absturz-Wiederherstellung hängt an `rtcRetryMagic`, das ausschließlich im DFPlayer-Fehlerpfad gesetzt wird – der 12v12-Ersatzalarm ist wegen `WDT_HARDWARE_MS (15 s) < WDG_TIMEOUT_MS (30 s)` faktisch toter Code; **(C)** der DFPlayer-Status ist die einzige Entscheidungsgrundlage für Stopp, Erfolg und Zustandswechsel, wobei `st == 0` (Modul antwortet, ist gestoppt) und `st == -1` (Modul tot) an mehreren Stellen bewusst verschieden, an anderen fälschlich gleich behandelt werden.

Kein einziger dieser Pfade schreibt beim Scheitern eine `[FEHLER]`-Zeile ins Web-Log. Der schlimmste Ausgang – Wecker bleibt stumm – ist damit nicht nur möglich, sondern auch nachträglich nicht diagnostizierbar.

---

## Befunde

### Ursachengruppe A – Alarmfälligkeit ist eine Flanke ohne Nachholung

Die Befunde A1–A6 haben genau eine Wurzel: `runAlarmMachine()` prüft `a1_*`/`a2_*` nur im `case ALARM_IDLE` und nur bei exakt `sec == 0`, die Wiederholsperre ist eine reine Minutenzahl ohne Datum. Ein einziger Umbau (pegel- statt flankenbasierte, tagesbezogene Fälligkeitsprüfung außerhalb des `switch`, plus Zeit-Gültigkeitsgate und Maximallaufzeit) behebt A1, A2, A4, A5 und A6 gemeinsam. Einzeln zu flicken lohnt hier nicht.

---

### [Kritisch] A1 – Alarm nur bei exakt `sec == 0`, kein Nachholmechanismus

- **Ort:** `Software/Firmware_aktuell/Wecker_13v00.ino:1409` und `:1414` (`runAlarmMachine`)
- **Was passiert:** Bedingung ist `a1_on && sec == 0 && min == a1_min && hour == a1_hour && min != lastA1Min`. Wird diese eine Sekunde nicht beobachtet, existiert kein Codepfad, der eine bereits verstrichene Alarmminute nachprüft. `lastA1Min`/`lastA2Min` (Z. 1439-1440) sperren nur Wiederholungen, sie holen nichts nach.
- **Wann es passiert:** (a) **Harter NTP-Zeitsprung** – die Hardware hat keine gepufferte RTC. Nach Stromausfall bootet das Gerät mit Epoch 0 (lokal 01:00). `configTime(0, 0, MY_NTP_SERVER)` (Z. 2371) lässt den Sync-Modus auf dem ESP-IDF-Default (`SNTP_SYNC_MODE_IMMED`, harter `settimeofday`-Sprung). Kommt der Router/DSL erst um 06:02 hoch, springt die Uhr in einem Schritt von 1970 auf 06:02 – die Sekunde 06:00:00 existiert nie, Alarm 1 fällt aus. Dieser Fall ist nach jedem nächtlichen Stromausfall der Normalfall, nicht der Ausnahmefall. (b) **Reboot im Alarmfenster** – Brownout/TWDT-Panic/`watchdogTask`-Restart um 05:59:45; `setup()` braucht minimal ~14 s (3000 ms Z. 2325 + 2000 ms Z. 2329 + `player.begin()`-Reset + 4000 ms Startsound Z. 2510), mit WiFi-/NTP-/MP3-Timeouts bis ~80 s. (c) **Märzumstellung** – 01:59:59 → 03:00:00, ein Alarm auf 02:30 existiert an diesem Tag nicht.
- **Warum es zählt:** Genau der Worst Case des Geräts: stiller, unprotokollierter Alarmausfall nach einem Alltagsereignis. Der Kommentar Z. 1403-1407 unterstellt, ein Folge-Tick liege noch in derselben Sekunde 0 – bei 500-ms-Takt (Z. 1965) und 200 ms Mutex-Timeout stimmt das nur in etwa 60 % der Phasenlagen.
- **Fix:** Fälligkeit tagesbezogen und als Pegel prüfen, mit begrenztem Nachholfenster:

```cpp
static uint16_t lastA1Day = 0xFFFF;                                                      // tm_yday des zuletzt behandelten Auslösetags
static uint16_t lastA2Day = 0xFFFF;

static bool alarmDue(bool on, uint8_t h, uint8_t m,
                     uint8_t hour, uint8_t min, uint16_t yday, uint16_t lastDay) {
  if (!on || yday == lastDay) { return false; }
  int16_t diff = (int16_t)(hour * 60 + min) - (int16_t)(h * 60 + m);
  return (diff >= 0 && diff <= ALARM_CATCHUP_MIN);                                       // Nachholfenster fängt Zeitsprünge und Reboots ab
}
```
  `ALARM_CATCHUP_MIN` (z.B. 15) neu in SysConf. `lastA1Day`/`lastA2Day` in `triggerAlarm()` statt `lastA1Min` setzen; die Minutensperren entfallen dadurch ersatzlos, ebenso ihr Reset in Z. 1439-1440. Analog `runCuckooMachine()` (Z. 1456) – dort genügt eine Sperre auf `hour` statt `min`.
- **Sicherheit des Befunds:** Bestätigt, beide Prüfer unabhängig, alle Zeilenangaben verifiziert. Offene Annahme: SNTP-Default-Sync-Modus (ESP-IDF-Kernverhalten, kein Bibliotheksinternum).
- **Notiz (Review 2026-08-14):** `ALARM_CATCHUP_MIN = 15` ist zu knapp für den in diesem Befund selbst genannten Auslöser (c) Märzumstellung: beim Sprung 01:59:59 → 03:00:00 fehlt eine Alarmminute um bis zu 60 Minuten (Alarm 02:00 → Diff 60, Alarm 02:44 → Diff 16), beide über dem Beispielwert. Der Wert muss entweder auf ≥60 angehoben oder die DST-Vorwärtslücke separat behandelt werden – sonst holt der eigene Fix genau den Fall nicht nach, den er laut Begründung mit abdecken soll. Zusätzlich: `lastA1Min`/`lastA2Min` werden nicht nur hier verwendet, sondern auch im S1-Handler (`inputTask`, Z. 1730-1732) und `lastCuckooMin` in Z. 1744 zurückgesetzt/gesetzt – der Umbau auf `lastA*Day` muss diese Stelle einschließen (Semantikfrage: soll ein manuell gestoppter Alarm für den Rest des Tages gesperrt bleiben?), sonst bleibt dort ein inkonsistenter Mischzustand aus Minuten- und Tages-Logik stehen. Diese Stelle überschneidet sich mit C1 und sollte nicht getrennt umgesetzt werden.

---

### [Kritisch] A2 – `triggerAlarm()`: fehlgeschlagener `playerMutex`-Take lässt den Alarm still ausfallen

- **Ort:** `Software/Firmware_aktuell/Wecker_13v00.ino:1347` (`triggerAlarm`)
- **Was passiert:** `if (xSemaphoreTake(playerMutex, pdMS_TO_TICKS(200)) == pdTRUE) { … }` hat keinen `else`-Zweig; die Funktion endet Z. 1395 stillschweigend. Kein Ton, kein Web-Log, kein `rtcRetryMagic`, kein `alarmTriggerInFlight`, kein Zustandswechsel. Da `alarmTriggerInFlight` false bleibt, greift auch der `watchdogTask`-Ersatzalarm (Z. 2070) nicht.
- **Wann es passiert:** Ein konkurrierender `playerMutex`-Halter hält die Sperre in der Alarmsekunde über 200 ms: S1-Handler (Z. 1703/1716/1723), Sound-Vorschau (Z. 562/570/583/591), Lautstärke (Z. 959/974), Menüseite 5 (Z. 672). Weil ein Fehlversuch selbst 200 ms kostet und der Folge-Tick 700 ms später liegt, gibt es in rund 40 % der Phasenlagen nur einen einzigen Versuch – ein einzelner Timeout genügt.
- **Warum es zählt:** Alarm fällt für den Tag aus, ohne eine einzige Log-Zeile. Der zweite Prüfer stuft das auf „mittel" herab, weil ein Halter >200 ms einen trägen DFPlayer voraussetzt und die Kollision Bedienung in der Alarmsekunde erfordert. Beides stimmt – die Folgenlosigkeit im Log macht den Fall trotzdem inakzeptabel.
- **Fix:** Fehlschlag protokollieren und den Alarmwunsch vom Player-Zugriff entkoppeln. Mit dem A1-Fix genügt: bei Take-Fehlschlag `webLogf("[FEHLER] …")` und **kein** Setzen von `lastA*Day` – das Nachholfenster sorgt dann automatisch für den Wiederholversuch im nächsten Tick. `alarmTriggerInFlight` zusätzlich **vor** dem Take setzen, damit der Freeze-Fallback greift.
- **Sicherheit des Befunds:** Bestätigt (Codestruktur eindeutig). Umstritten ist nur die Häufigkeit; die für lange Haltezeiten maßgebliche Blockierdauer von `player.readState()` ist ohne Bibliotheksquellen nicht hart belegbar.
- **Notiz (Review 2026-08-14):** Der Fix verlangt, `alarmTriggerInFlight` vor dem Take zu setzen, damit der Watchdog-Freeze-Fallback greift (Z. 2070-2078) – dieser Fallback ist laut B1 aber bis zum B1-Fix toter Code (TWDT feuert immer zuerst). D.h. dieser Teil von A2 ist real erst wirksam, nachdem B1 (oder mindestens dessen primäre Maßnahme B2) umgesetzt ist. Das Web-Log bei Take-Fehlschlag und der Verzicht auf `lastA*Day`-Setzen wirken dagegen unabhängig davon sofort.

---

### [Kritisch] A3 – `ALARM_RUNNING` endet bei dauerhaftem `st == -1` nie: Gerät bleibt für immer entwaffnet

- **Ort:** `Software/Firmware_aktuell/Wecker_13v00.ino:1436` (`runAlarmMachine`)
- **Was passiert:** Einziger Ausstieg ist `playerStatus == 0`. `t_start6` wird bei jedem Poll neu gesetzt (Z. 1435) und ist damit reiner Poll-Timer, keine Laufzeitgrenze. Liefert `readStateDrained()` dauerhaft -1, bleibt der Zustand für immer stehen. Da `a1_*`/`a2_*` ausschließlich im `case ALARM_IDLE` geprüft werden, wird ab diesem Moment **kein einziger weiterer Alarm mehr ausgelöst**.
- **Wann es passiert:** Alarm startet, `verifyPlayStarted()` bestätigt `st > 0` → `ALARM_RUNNING`. Danach fällt die DFPlayer-Antwort aus (TX-Leitung lose, Modul-Brownout beim Motoranlauf, SD-Karte gezogen). Motor (E2) und Licht (E3) laufen unbegrenzt weiter. S1 rettet nicht: die Retry-Schleife Z. 1710-1721 erzwingt nach 200 ms `st = 0`, landet damit im `else`-Zweig (Kuckuck, Z. 1733-1745) und fasst weder `alarmState` noch E2/E3 an. Der 30-min-Deckel Z. 1930 hängt an `S2_SW` und greift nicht. Der `watchdogTask` greift nicht, weil `alarmTask` normal weiterläuft (Kommentar Z. 2044 sagt das ausdrücklich).
- **Warum es zählt:** Der Wecker ist danach dauerhaft tot, ohne Fehlermeldung, und nur ein Stromausfall/Reset stellt ihn her – ausgerechnet der Neustart, der wegen `player.begin(…, doReset=true)` das hängende Modul geheilt hätte, findet nicht statt. Schaltet der Nutzer die sichtbare Last per S2 ab, verschwindet auch noch das Symptom. Die 12v05-Historie (SysConf Z. 180-182) benennt genau dieses Risiko; behoben wurde damals nur der Startcheck.
- **Fix:** Harte Obergrenze in `ALARM_RUNNING`, mit eigenem Zeitstempel (nicht `t_start6`, der wird zurückgesetzt):

```cpp
if (playerStatus == 0 || millis() - alarmRunStart >= ALARM_MAX_RUN_MS) {
  if (playerStatus != 0) {
    webLogf("[FEHLER] Alarm ohne DFPlayer-Rueckmeldung nach %lu ms beendet",
            (unsigned long)(millis() - alarmRunStart));
  }
  motorStop();
  …
}
```
  `alarmRunStart = millis()` in `triggerAlarm()` neben Z. 1391 setzen, `ALARM_MAX_RUN_MS` (10–15 min) in SysConf. Zusätzlich S1 unabhängig vom Playerstatus wirken lassen, solange `alarmState == ALARM_RUNNING` (siehe C1).
- **Sicherheit des Befunds:** Bestätigt, beide Prüfer, alle Zeilen verifiziert.

---

### [Hoch] A4 – Alarm 2 wird während laufendem Alarm 1 nie geprüft

- **Ort:** `Software/Firmware_aktuell/Wecker_13v00.ino:1420` (`runAlarmMachine`, `case ALARM_RUNNING`)
- **Was passiert:** Die Fälligkeitsprüfung liegt komplett im `case ALARM_IDLE`. Steht die Maschine zur Fälligkeitszeit von A2 in `ALARM_RUNNING`, wird A2 in diesem Tick nicht geprüft – und die `sec == 0`-Flanke kommt nicht wieder.
- **Wann es passiert:** A1 = 06:00, A2 = 06:05 als Sicherheitsnetz, A1-Titel länger als 5 Minuten (der Nutzer hat ihn verschlafen). Endet A1 um 06:07, werden die Sperren zurückgesetzt – 06:05:00 ist längst vorbei, A2 fällt ersatzlos aus. In Kombination mit A3 fallen alle Folgealarme aus.
- **Warum es zählt:** Genau der Fall, für den A2 konfiguriert wurde, versagt. (Ein Prüfer stuft auf „mittel", weil das Gerät zur Fälligkeitssekunde nicht stumm ist und die Konstellation überlappende Alarmzeiten voraussetzt – korrekt, ändert aber nichts daran, dass das Sicherheitsnetz reißt.)
- **Fix:** Mit dem A1-Fix erledigt: `alarmDue()` in jedem Tick auswerten, unabhängig vom Zustand. Bei Treffer während `ALARM_RUNNING` den fälligen Alarm merken und beim Übergang nach `ALARM_IDLE` nachziehen (das Nachholfenster begrenzt das automatisch).
- **Sicherheit des Befunds:** Bestätigt. Die im Ursprungsbefund genannte Begründung über `ALARM_POLL_MS` ist falsch (das Ende wird alle 5 s geprüft, nicht erst nach Minuten) – am Mechanismus ändert das nichts.

---

### [Hoch] A5 – Alarm und Kuckuck laufen ungeprüft auf der ungestellten 1970-Uhr

- **Ort:** `Software/Firmware_aktuell/Wecker_13v00.ino:1962` (`alarmTask`)
- **Was passiert:** Der Zeitschnappschuss geht ohne jede Gültigkeitsprüfung an beide State Machines. Ohne NTP steht die Systemzeit auf Epoch 0, mit `MY_TZ` also 01:00:00 ab Bootzeitpunkt. Das Projekt kennt beide Prüfungen (`tm_year < 71` Z. 2423, `t > 1700000000UL` Z. 300), benutzt sie im Alarmpfad aber nicht.
- **Wann es passiert:** Stromausfall, WLAN kommt nicht zurück (falsches PSK, defekter Router). WiFi-Timeout nach 30 s, der NTP-Block (nur unter `if (wifiConnected)`, Z. 2403) wird übersprungen. Nach 5 h Uptime erreicht die Fake-Uhr 06:00:00 und löst Alarm 1 samt Motor und Licht mitten am Tag aus; ab dann schlägt bei aktiviertem Kuckuck stündlich der Magnet 7,5 s.
- **Warum es zählt:** Fehlalarm zu beliebiger Tageszeit. Wichtiger noch: **ohne dieses Gate ist der A1-Fix gefährlich** – eine pegelbasierte Prüfung würde auf der 1970-Uhr sofort nach Boot feuern. Das Gate ist Voraussetzung, nicht Zusatz.
- **Fix:**

```cpp
static inline bool timeValid() { return time(nullptr) > 1700000000UL; }
```
  In `alarmTask` vor Z. 1962: `if (!timeValid()) { wdg_alarmTask = millis(); vTaskDelay(…); continue; }`. Optional die Uhr im Display als `--:--:--` zeigen, solange ungültig.
- **Sicherheit des Befunds:** Bestätigt. Ein Prüfer stuft auf „mittel", weil das Display „01.01.1970" zeigt und `cuckoo_on` per Default aus ist – beides zutreffend.

---

### [Mittel] A6 – Oktober-Zeitumstellung: Alarm zwischen 02:00 und 02:59 löst zweimal aus

- **Ort:** `Software/Firmware_aktuell/Wecker_13v00.ino:1439` (`runAlarmMachine`)
- **Was passiert:** `lastA1Min`/`lastA2Min` sind reine Minutenzahlen ohne Datumsbezug und werden beim Titelende auf 0xFF zurückgesetzt.
- **Wann es passiert:** Letzter Sonntag im Oktober, `MY_TZ = "CET-1CEST,M3.5.0/02,M10.5.0/03"`: die Ortszeit durchläuft 02:00–02:59 zweimal. Ein Alarm auf 02:30 startet beim zweiten Durchlauf erneut. Analog der Kuckuck bei 02:00 (`lastCuckooMin`, Z. 1467/1478).
- **Warum es zählt:** Zusätzlicher Nachtalarm, einmal jährlich, nur bei Alarmzeiten in dieser Stunde. Störend, nicht kritisch.
- **Fix:** Mit dem A1-Fix erledigt (`lastA*Day` statt `lastA*Min`).
- **Sicherheit des Befunds:** Bestätigt, beide Prüfer.

---

### Ursachengruppe B – Crash-Recovery hängt an einem Merker, der zu selten und zu früh gelöscht wird

`rtcRetryMagic` ist der einzige Zustand, der einen Reset übersteht. Er wird nur im DFPlayer-Fehlerpfad gesetzt (Z. 1378-1382) und im Watchdog-Freeze-Zweig (Z. 2075), der praktisch nie erreicht wird. Jeder andere Reset – Power-On, Brownout, TWDT-Panic, `watchdogTask`-Restart wegen input-/displayTask-Freeze – verliert einen laufenden oder gerade startenden Alarm ersatzlos.

---

### [Hoch] B1 – Hardware-TWDT (15 s) feuert immer vor dem App-Watchdog (30 s): Ersatzalarm-Fallback ist toter Code

- **Ort:** `Software/Firmware_aktuell/Wecker_13v00.ino:2070` (`watchdogTask`), Konstanten `SysConf_13v00.h:423-425`
- **Was passiert:** In allen drei überwachten Tasks liegen `esp_task_wdt_reset()` und das `wdg_*`-Alive-Signal in derselben Schleifeniteration (Z. 1652/1654+1661, 1849/1850, 1950/1964). Jeder Hänger stoppt zwangsläufig beide Zähler. Der TWDT löst nach 15 s per Panic aus; `watchdogTask` bräuchte 30 s plus bis zu 5 s Prüfraster. Der gesamte Freeze-Zweig ab Z. 2059 inklusive der 12v12-Ersatzalarm-Vormerkung (Z. 2070-2078) ist damit unerreichbar.
- **Wann es passiert:** Jeder Task-Freeze ≥ 15 s. Dass der Software-Watchdog historisch überhaupt je feuerte (12v11), lag nur daran, dass die TWDT-Anmeldung bis 12v15 mit „task not found" ins Leere lief – seit dem 12v16-Fix gewinnt immer der TWDT. Der 12v12-Vorfall (SysConf Z. 89-109, „Reboot ohne Ersatzalarm trotz 12v11/12v12-Fixes", mit TWDT-Backtrace) ist genau dieser Ablauf, auf der Hardware beobachtet.
- **Warum es zählt:** Die einzige Absicherung gegen „Freeze während Alarmversuch" ist funktionslos. Reboot ohne Ton, Alarm fällt komplett aus.
- **Fix:** Zwei Änderungen, unabhängig voneinander wirksam:
  1. **Primär (B2-Fix, siehe unten):** RTC-Merker beim Betreten des riskanten Abschnitts schreiben – dann ist es egal, welcher Watchdog feuert.
  2. **Sekundär:** `WDG_TIMEOUT_MS` auf 6000 und `WDG_CHECK_MS` auf 1000 senken. Detektion dann bei 6–7 s, Restart bei ~9 s, sicher vor den 15 s des TWDT. Die Alive-Intervalle tragen das (`inputTask` ≤50 ms, `displayTask` ~300 ms, `alarmTask` ~500 ms, im ungünstigsten `verifyPlayStarted()`-Durchlauf ~2,5 s).
- **Sicherheit des Befunds:** Bestätigt, beide Prüfer, durch die eigene Änderungshistorie empirisch belegt.
- **Notiz (Review 2026-08-14):** Vor der sekundären Maßnahme (`WDG_TIMEOUT_MS`=6000/`WDG_CHECK_MS`=1000) den realen Worst-Case nachrechnen statt den Schätzwert übernehmen: `verifyPlayStarted()` kann allein bis zu `VERIFY_PLAY_RETRIES × VERIFY_PLAY_DELAY_MS` plus Mutex-Wartezeiten summieren, dazu kommt `readStateDrained()`s Gnadenfrist (`SERIAL2_FEEDBACK_GRACE_MS`). Die im Befund genannten ~2,5 s sind plausibel, aber die Marge zum neuen 6-7-s-Detektionsfenster ist knapp – erst nach Schritt 3 (B2) testen, wie im Befund selbst vorgesehen (Reihenfolge, Schritt 5).

---

### [Hoch] B2 – `alarmTriggerInFlight` wird vor dem Einschalten von Motor und Licht gelöscht

- **Ort:** `Software/Firmware_aktuell/Wecker_13v00.ino:1386` (`triggerAlarm`)
- **Was passiert:** Nach bestätigtem `playFolder()` wird `alarmTriggerInFlight = false` gesetzt (Z. 1386), **danach** erst `motorStart()` (Z. 1389, ggf. 150 ms Vollgas-Kickstart), E3 HIGH (Z. 1390) und `alarmState = ALARM_RUNNING` (Z. 1392). Für einen Reset ab Z. 1386 existiert kein Recovery-Pfad: `rtcRetryMagic` ist 0, der Freeze-Fallback prüft `alarmTriggerInFlight`.
- **Wann es passiert:** Jeder Reset zwischen Z. 1386 und dem regulären Alarmende. Konkret erreichbar über: `watchdogTask` erkennt einen input-/displayTask-Freeze und ruft `ESP.restart()` (Z. 2091) – ohne jeden Merker; TWDT-Panic während der `ALARM_RUNNING`-Pollschleife (dieselben `player.available()`-Aufrufe, die laut 12v13 schon einmal 15 s blockierten); Brownout. Nach dem Neustart löst `setup()` keinen Ersatzalarm aus, und `player.begin(…, doReset=true)` plus Startsound (Z. 2509) beenden die laufende Datei ohnehin.
- **Warum es zählt:** Die Firmware zerstört im eigenen Recovery-Pfad einen laufenden Alarm, ohne ihn vorzumerken – genau das, was 12v12 verhindern sollte.
- **Fix:** Merker früh schreiben, spät löschen:

```cpp
// in triggerAlarm(), vor drainSerial2Pre():
rtcRetryAlarm = alarmNum; rtcRetryFileNo = fileNo; rtcRetryMin = min;
rtcRetryCount = failCount; rtcRetryMagic = RTC_RETRY_MAGIC;
```
  Löschen erst im `ALARM_RUNNING`-Zweig nach dem ersten Poll mit `playerStatus > 0` (Z. 1434) sowie im finalen Abbruch (Z. 1374). Wichtig: die Nachhol-Logik aus A1 begrenzt dann automatisch, dass ein spät im Alarm liegender Reboot nicht Stunden später erneut auslöst – **dieser Fix sollte nicht ohne A1 gemacht werden.**
- **Sicherheit des Befunds:** Bestätigt (ein Prüfdurchlauf, alle Zeilen verifiziert). Die im Ursprungsbefund genannte Ursache „Brownout durch Motor-Kickstart" halte ich für unbelegt: ~250 mA Zusatzlast an einem Anker-323-Netzteil, Kickstart zudem nur bei `motor_duty < 89`. Der Befund trägt auch ohne diese Annahme, weil `watchdogTask` und TWDT selbst die Reset-Quelle sind.
- **Notiz (Review 2026-08-14):** Wird der RTC-Merker bei jedem Alarmversuch vorab geschrieben (auch bei später erfolgreichem Start), muss der Löschzeitpunkt sauber vor dem nächsten Freeze-Fenster liegen. Zwischen `alarmState = ALARM_RUNNING` (Z. 1392) und dem ersten Poll in `runAlarmMachine` liegt `ALARM_POLL_MS` (mehrere Sekunden) – ein Reset in diesem Fenster löst den Alarm nach Neustart erneut aus, obwohl er bereits lief. Unkritisch, aber nur, weil A1s Tages-Guard den doppelten Trigger abfängt: **B2 ist ohne A1 nicht nur „nicht ideal", sondern an dieser konkreten Stelle nachweisbar falsch** (wiederholter Alarm trotz bereits laufendem Motor/Licht).

---

### Ursachengruppe C – Der DFPlayer-Status ist die einzige Wahrheit, und er wird unvollständig gelesen

`readStateDrained()` filtert auf `DFPlayerFeedBack` und verwirft **jeden** anderen Frame kommentarlos (Z. 363); `drainSerial2Pre()` prüft den Typ gar nicht. Ein Error-Frame des Moduls kann die Firmware daher nie erreichen. Gleichzeitig wird `st` an drei Stellen unterschiedlich interpretiert: in `ALARM_RUNNING` bedeutet -1 „Alarm läuft weiter" (konservativ, richtig), im S1-Handler wird -1 aktiv zu 0 umgedeutet (unsicher), und in `verifyPlayStarted()`/`triggerAlarm()` gilt 0 wie -1 als Absturz (falsch).

---

### [Hoch] C1 – S1 entscheidet nur nach Playerstatus: laufender Alarm wird nicht gestoppt, stattdessen Kuckuck

- **Ort:** `Software/Firmware_aktuell/Wecker_13v00.ino:1712` (`inputTask`, `EVT_S1`)
- **Was passiert:** Bekommt der Handler innerhalb von 200 ms kein `st > 0`, setzt er `st = 0` („als idle behandeln → kein stop(), Kuckuck auslösen") und nimmt den `else`-Zweig: E1 HIGH für 7,5 s. Kein `player.stop()`, kein `motorStop()`, kein E3 LOW, `alarmState` bleibt `ALARM_RUNNING`. Der zuverlässig lesbare `alarmState` (Z. 105, `volatile`, cross-core) wird nie herangezogen.
- **Wann es passiert:** Der DFPlayer antwortet während eines laufenden Alarms nicht (`st == -1`) – laut Firmware-Kommentar Z. 338-353 ein am 10.08.2026 real beobachtetes Verhalten. Nach ~1,2 s Wartezeit bekommt der Nutzer statt Ruhe 7,5 s Kuckuck, der Alarm bläst weiter, und jeder Druck nach `BTN_LOCKOUT_MS` wiederholt das. Zweiter, alltäglicherer Fall: die MP3 ist gerade zu Ende, `alarmTask` räumt aber erst beim nächsten 5-s-Poll auf – wer in diesem Fenster S1 drückt, um Motor und Licht abzuschalten, löst den Kuckuck aus.
- **Warum es zählt:** Die Stopp-Taste eines Weckers wirkt nicht. In Kombination mit A3 ist S1 der einzige Notausstieg, der ebenfalls versagt.
- **Fix:** Entscheidungsgrundlage umkehren:

```cpp
if (alarmState == ALARM_RUNNING || st > 0) {
  // stop() darf scheitern – Motor, Licht und Zustand werden trotzdem zurückgesetzt
  if (xSemaphoreTake(playerMutex, pdMS_TO_TICKS(200)) == pdTRUE) { … }
  motorStop(); digitalWrite(E3, LOW); alarmState = ALARM_IDLE; …
} else { /* Kuckuck */ }
```
  Der Kuckuck-Zweig nur bei `ALARM_IDLE` **und** sicher gelesenem `st == 0`; `st == -1` nicht mehr in 0 umdeuten.
- **Sicherheit des Befunds:** Bestätigt, beide Prüfer. Die im Ursprungsbefund behauptete Mutex-Aushungerung ist so nicht konstruierbar (`alarmTask` gibt den Mutex zwischen seinen beiden Abfragen frei) – der Schaden entsteht direkt über `st == -1`.
- **Notiz (Review 2026-08-14):** Der Fix-Vorschnitt ("Entscheidungsgrundlage umkehren") deckt nicht alle Fälle: bleibt `alarmState == ALARM_IDLE` und der Player antwortet nicht (`st == -1` nach 200-ms-Timeout), fällt weder die Stop- noch die sicher-`st==0`-Kuckuck-Bedingung – aktuell fängt genau das der `st=0`-Fallback (Z. 1712) auf, wenn auch mit falscher Interpretation. Der Umbau braucht also einen dritten, expliziten Zweig „Status unbekannt bei `ALARM_IDLE`" (z.B. nichts auslösen, nur loggen) statt einer reinen Bedingungsumkehr, sonst verschwindet die Kuckuck-Auslösung bei totem DFPlayer ersatzlos.

---

### [Hoch] C2 – `drainSerial2Pre()` bricht beim ersten ACK-Frame ab, ein Altframe kann „playing" vortäuschen

- **Ort:** `Software/Firmware_aktuell/Wecker_13v00.ino:389` (`drainSerial2Pre`)
- **Was passiert:** Die Schleifenbedingung prüft `Serial2.available()` (Rohbytes), der Abbruch hängt aber am Rückgabewert von `player.available()` (Parserzustand). Der Kommentar begründet den `break` mit „unvollständiger Frame" – derselbe `false`-Wert entsteht aber auch, wenn ein **vollständiger** 0x41-ACK-Frame konsumiert wurde: `parseStack()` behandelt 0x41 intern, ohne `_isAvailable` zu setzen. Das steht in der Firmware selbst (Z. 342-344) und ist in der Historie gemessen (SysConf Z. 143-154: „konstant 20 Byte / 2 Frames Rückstand"). Bei Pufferinhalt `[ACK][Feedback]` wird nur der ACK konsumiert, `drainedPre` bleibt sogar 0 – das Byte-Limit ist nie der begrenzende Faktor.
- **Wann es passiert:** Nach einer Sound-Vorschau plus spätem 0x42-Feedback steht `[ACK]["playing"]` im Puffer. Beim nächsten Alarm konsumiert der Vorab-Drain nur den ACK. Bleibt `playFolder()` wirkungslos (SD gezogen, Datei fehlt, Modul im Fehlerzustand), parst `readStateDrained()` den stehengebliebenen „playing"-Frame → `st > 0` → `verifyPlayStarted()` liefert `true`.
- **Warum es zählt:** Motor an, Licht an, `alarmState = ALARM_RUNNING`, `lastA*Min` gesetzt – **aber kein Ton**, kein Fehlerlog, kein Retry, kein Ersatzalarm. Die komplette dreistufige Absicherung (verify → RTC-Retry → `ALARM_MAX_RESTARTS`) ist genau in dem Fall ausgehebelt, für den sie gebaut wurde.
- **Fix:** Fortschritt am Rohpuffer festmachen statt am Rückgabewert:

```cpp
static void drainSerial2Pre(const char* label) {
  checkSerial2Leftover(label);
  uint16_t drainedPre = 0;
  while (Serial2.available() >= DFPLAYER_RECEIVED_LENGTH && drainedPre < SERIAL2_DRAIN_MAX_BYTES) {
    int before = Serial2.available();
    player.available();
    player.read();
    if (Serial2.available() >= before) { break; }                                        // kein Fortschritt: unvollstaendiger Frame, stehen lassen
    drainedPre += DFPLAYER_RECEIVED_LENGTH;
  }
}
```
  Zusätzlich in `readStateDrained()` **vor** `player.readState()` drainen, damit nie ein Altframe als Antwort auf die gerade gesendete Abfrage gilt.
- **Sicherheit des Befunds:** Bestätigt, beide Prüfer, aber **library_assumption**: `parseStack()` setzt bei 0x41 kein `_isAvailable`. Die Annahme ist durch zwei Stellen im Repo gestützt (Firmware-Kommentar Z. 342-344, SysConf-Messung Z. 143-154), lokal aber nicht am Bibliotheksquelltext verifizierbar. Der Vollschaden setzt zusätzlich einen alten Feedback-Frame mit Wert > 0 hinter dem ACK voraus. **Diese Änderung berührt genau den Bereich, der in 12v13/12v14 eine Regression erzeugt hat – nach Umbau mindestens einen kompletten Alarm-, Vorschau- und S1-Zyklus mit offenem Web-Log durchfahren.**
- **Notiz (Review 2026-08-14):** Der Fortschritts-basierte Ansatz ist risikoärmer als er klingt: Der äußere Schleifenkopf verlangt bereits `Serial2.available() >= DFPLAYER_RECEIVED_LENGTH`, ein unvollständiger Frame kann die Schleife also gar nicht erst betreten – eine Endlosschleife über die Fortschrittsmessung ist damit ausgeschlossen, zusätzlich bleibt `SERIAL2_DRAIN_MAX_BYTES` als harte Grenze bestehen. Die "zuletzt, isoliert" Einordnung bleibt trotzdem richtig – nicht wegen der Fix-Qualität, sondern wegen der unverifizierbaren Bibliotheksannahme und der Regressionshistorie an genau dieser Stelle.

---

### [Hoch] C3 – `st == 0` (Modul antwortet, gestoppt) wird wie „abgestürzt" behandelt: 2 Reboots, danach stumm ohne echte Ursache im Log

- **Ort:** `Software/Firmware_aktuell/Wecker_13v00.ino:1326` (`verifyPlayStarted`), `:360` (`readStateDrained`)
- **Was passiert:** `verifyPlayStarted()` wertet nur `st > 0` als Erfolg; `st == 0` (Modul lebt, hat aber nichts gestartet – typisch für „Datei nicht gefunden") und `st == -1` (keine Antwort) führen zur identischen Diagnose „DFPlayer vermutlich abgestürzt" (Z. 1329) und zu `ESP.restart()` (Z. 1384). Parallel dazu verwirft `readStateDrained()` jeden Nicht-Feedback-Frame (Z. 363) – ein `DFPlayerError`-Frame mit dem echten Fehlercode wird kommentarlos weggeworfen. Repo-weit kommen `DFPlayerError`/`CardRemoved`/`PlayFinished` nirgends vor.
- **Wann es passiert:** `sound1_assigned` zeigt auf eine Datei, die in Ordner 01 nicht existiert (siehe C4/C5) oder nicht lesbar ist. Ablauf: 3× „kein Start-Status" → `failCount` 0→1 → Reboot → `setup()`-Retry (Z. 2532, geht am Clamp Z. 2515 vorbei, weil `rtcRetryFileNo` verwendet wird) → derselbe Fehlschlag → 1→2 → Reboot → 2→3 ≥ `ALARM_MAX_RESTARTS` → `return` in Z. 1376. **Motor (Z. 1389) und Licht (Z. 1390) stehen hinter diesem `return` und werden nie erreicht.**
- **Warum es zählt:** Zwei vollständige Neustarts à 15–80 s im Alarmfenster, danach absolut nichts – kein Ton, kein Motor, kein Licht. Ein Neustart behebt einen Datei-/Kartenfehler prinzipiell nicht; er ist die falsche Reaktion auf ein Modul, das nachweislich antwortet.
- **Fix:** `st == 0` und `st == -1` trennen. Nur `-1` (keine Antwort) rechtfertigt den Neustart; bei wiederholtem `st == 0` einen `[FEHLER]`-Eintrag schreiben, **den Alarm trotzdem als laufend behandeln** (Motor + Licht an, damit der Nutzer wenigstens geweckt wird) und optional auf Datei 1 zurückfallen. Zusätzlich in `readStateDrained()` den Frame-Typ auswerten, bevor er verworfen wird, und `DFPlayerError` mit Code ins Web-Log schreiben.
- **Sicherheit des Befunds:** Bestätigt, aber vom Prüfer von „kritisch" auf „mittel" heruntergestuft. Der Teil „Error-Frame wird verworfen" trägt eine Bibliotheksannahme (0x40 → `DFPlayerError`). Der hier tragende Kern – `st == 0` wird wie `st == -1` behandelt, `motorStart()` steht hinter dem `return` – ist annahmefrei aus dem Code belegt.
- **Notiz (Review 2026-08-14):** Wird bei wiederholtem `st == 0` "der Alarm trotzdem als laufend behandelt" (Motor/Licht an, kein Ton), muss A3s Maximallaufzeit-Ausstieg auch für diesen stummen Fall greifen – sonst läuft ein Wecker ohne Ton bis zum A3-Timeout unbegrenzt weiter, statt zeitnah in einen erkennbaren Fehlerzustand zu wechseln. C3 sollte deshalb zusammen mit A3 verifiziert werden, nicht isoliert, auch wenn beide getrennt committet werden.

---

### [Mittel] C4 – `mp3Count` wird geraten (Fallback 99, `c - 1`)

- **Ort:** `Software/Firmware_aktuell/Wecker_13v00.ino:2496` und `:2501` (`setup`)
- **Was passiert:** Bei `readFileCounts`-Timeout wird `mp3Count = 99` gesetzt, obwohl keine Dateizahl bekannt ist. Im Erfolgsfall unterstellt `mp3Count = c - 1`, dass **genau eine** Datei außerhalb von Ordner 01 liegt (der Startsound in Ordner 02). `readFileCounts()` (0x48) liefert die Gesamtzahl über alle Ordner; die ordnerbezogene Variante `readFileCountsInFolder()` (0x4E) wird nirgends verwendet.
- **Wann es passiert:** SD-Karte mit 12 Alarmdateien in Ordner 01 und 3 Dateien in Ordner 02 → `mp3Count = 14`, Nummer 13 und 14 sind wählbar und existieren nicht. Ebenso jede vom Betriebssystem angelegte Zusatzdatei beim Kopieren auf FAT32, oder der 99-Fallback nach langsamer SD-Indizierung.
- **Warum es zählt:** Direkter Zubringer für C3 – eine wählbare, nicht existierende Datei führt zur Reboot-Kaskade und zum endgültig stummen Alarm.
- **Fix:** `readFileCountsInFolder(1)` verwenden. Bei Timeout **nicht raten**: `mp3Count` bei 0 lassen, bestehende Zuordnungen unangetastet lassen (siehe C5) und die UI-Auswahl erst freigeben, wenn eine echte Dateizahl vorliegt.
- **Sicherheit des Befunds:** Bestätigt, beide Prüfer. **library_assumption**: Semantik 0x48 vs. 0x4E (gut dokumentiert, aber lokal nicht verifizierbar).
- **Notiz (Review 2026-08-14):** Keine substanziellen Einwände.

---

### [Hoch] C5 – `mp3Count == 0` nach fehlgeschlagenem `player.begin()` überschreibt die Sound-Zuordnung dauerhaft

- **Ort:** `Software/Firmware_aktuell/Wecker_13v00.ino:2515-2516` (`setup`)
- **Was passiert:** Im `else`-Zweig (Z. 2512) bleibt `mp3Count` bei 0. Die Clamp-Zeilen laufen trotzdem unbedingt: `if (sound1_assigned > mp3Count) sound1_assigned = 1;`. Da `readNVR()` (Z. 772/773) `sound*_assigned >= 1` garantiert, ist die Bedingung immer wahr – beide Zuordnungen werden auf 1 gesetzt.
- **Wann es passiert:** Der DFPlayer antwortet beim Wiederanlauf nicht rechtzeitig auf `begin()` (SD-Indizierung, Modul-Brownout). Verstellt der Nutzer danach **irgendetwas** (Lautstärke, Alarmzeit), löst `markSafeChange()` → `nvrTask` → `writeNVR()` (Z. 735/736) aus und persistiert `sound1_assigned = 1`. Nach dem nächsten Neustart ist der Originalwert auch aus NVS verschwunden. Einen `begin()`-Retry gibt es nicht (in 10v01 entfernt).
- **Warum es zählt:** Unwiederbringlicher, stiller Verlust einer Nutzereinstellung. Verschärfend: bei `mp3Count == 0` klemmt die Sound-Auswahl in der UI (Z. 1060/1098) auf 1 fest – der Nutzer kann seine Melodie in diesem Boot nicht einmal wieder einstellen, und der Versuch löst selbst den Commit aus.
- **Fix:** Clamp nur bei bekannter Dateizahl, plus sichtbarer Fehlerzustand:

```cpp
if (mp3Count > 0) {
  if (sound1_assigned > mp3Count) sound1_assigned = 1;
  if (sound2_assigned > mp3Count) sound2_assigned = 1;
} else {
  webLog("[FEHLER] DFPlayer-Dateizahl unbekannt – Sound-Zuordnung unveraendert");
}
```
- **Sicherheit des Befunds:** Bestätigt, beide Prüfer, deterministisch nachvollziehbar. Der Wecker wird dadurch nicht stumm (Datei 1 existiert immer), er weckt mit der falschen Melodie.
- **Notiz (Review 2026-08-14):** Keine substanziellen Einwände.

---

### [Mittel] C6 – `player.stop()` aus `inputTask` während `verifyPlayStarted()` erzwingt einen Reboot

- **Ort:** `Software/Firmware_aktuell/Wecker_13v00.ino:1725` (`inputTask`) gegen `:1367` (`triggerAlarm`)
- **Was passiert:** `triggerAlarm()` gibt `playerMutex` nach `playFolder()` frei (Z. 1366) und ruft `verifyPlayStarted()` ohne Mutex auf. `inputTask` (Core 1) kann den Mutex in diesem Fenster jederzeit bekommen und stoppt den Player, ohne `alarmState` oder `alarmTriggerInFlight` zu prüfen. Die Verifikation sieht danach 3× `st == 0` → `rtcRetryMagic` gesetzt → `ESP.restart()`. Nach dem Neustart löst `setup()` (Z. 2532) exakt denselben Alarm erneut aus.
- **Wann es passiert:** Der Nutzer drückt S1 zwischen dem Moment, in dem der DFPlayer „playing" meldet (typ. 100–400 ms nach `playFolder`), und dem ersten Poll bei t = 500 ms. Dasselbe passiert unbeabsichtigt über `menu(5)` (Z. 674) oder das Abwählen einer Sound-Vorschau (Z. 572/593).
- **Warum es zählt:** Der bewusst gestoppte Alarm klingelt nach einem unnötigen Reboot erneut. Nicht eskalierend: der Retry in `setup()` läuft vor der Task-Erstellung (Z. 2592), ein zweiter S1-Druck kann dort nichts auslösen – es bleibt bei einem Reboot-Zyklus.
- **Fix:** Abbruch-Flag (`alarmCancelRequested`), das `inputTask` vor `player.stop()` setzt, wenn `alarmTriggerInFlight` oder `ALARM_RUNNING` gilt. `verifyPlayStarted()` wertet es als reguläres Ende (kein `rtcRetryMagic`, kein Restart), `triggerAlarm()` prüft es vor Z. 1386.
- **Sicherheit des Befunds:** Bestätigt; Zeitfenster laut zweitem Prüfer eher 100–400 ms als 1500 ms.
- **Notiz (Review 2026-08-14):** Keine substanziellen Einwände.

---

### [Mittel] C7 – Fehlgeschlagener 50-ms-Take wird ignoriert: Anzeige/NVS und DFPlayer laufen auseinander

- **Ort:** `Software/Firmware_aktuell/Wecker_13v00.ino:959` und `:974` (`onClock`), gleiches Muster in `checkboxSound` (Z. 562/570/583/591) und `menu(5)` (Z. 672)
- **Was passiert:** `vol` wird bereits **vor** dem Take verändert (Z. 957/972); nur `drainSerial2Pre()` + `player.volume()` stehen im `if`. Anzeige-Update und `markSafeChange()` laufen unbedingt danach, ohne `else`, ohne Log, ohne Retry.
- **Wann es passiert:** Der Alarm läuft, der Nutzer drückt T4; `alarmTask` hält den Mutex gerade im 5-s-Poll bzw. in einer der drei `verifyPlayStarted()`-Abfragen. Die Anzeige zeigt 03, der Player spielt weiter auf 09 – und 03 landet in NVS. Bei der Vorschau wird die Checkbox geleert, `player.stop()` fällt aus, der Sound läuft weiter.
- **Warum es zählt:** Kein Alarmausfall, aber die Lautstärkeregelung wirkt genau dann nicht, wenn man sie braucht.
- **Fix:** Zustandsänderung, Anzeige und `markSafeChange()` erst nach erfolgreichem Take ausführen; mindestens den Fehlschlag per `webLogf()` protokollieren.
- **Sicherheit des Befunds:** Bestätigt, beide Prüfer. Das Kollisionsfenster ist kleiner als im Ursprungsbefund behauptet (Mutex-Hold eher 100–150 ms als 2 s).
- **Notiz (Review 2026-08-14):** Keine substanziellen Einwände.

---

### [Mittel, umstritten] C8 – `playerMutex` wird über blockierende Bibliotheksaufrufe gehalten

- **Ort:** `Software/Firmware_aktuell/Wecker_13v00.ino:366` (`readStateDrained`), Aufrufer Z. 1323/1425/1431/1705/1718; UI-Pfade Z. 562/959/672 unter zusätzlich gehaltenem `displayMutex` (Z. 1783)
- **Was passiert:** In `readStateDrained()` wird `graceStart` bei **jedem** empfangenen Frame neu gesetzt (Z. 366); begrenzt ist nur die Frameanzahl (20). Theoretischer Worst Case ~2 s Mutex-Haltezeit. Zusätzlich nehmen die UI-Pfade `playerMutex` unter gehaltenem `displayMutex` und rufen darunter blockierende `player.*`-Aufrufe.
- **Warum ich es abschwäche:** Ein **Deadlock ist ausgeschlossen** – es gibt keine Lock-Inversion, `displayMutex` wird nirgends unter `playerMutex` genommen, und `triggerAlarm()` nimmt `displayMutex` überhaupt nicht. Die 2-s-Rechnung setzt einen Frame-Strom mit knapp unter 100 ms Abstand über 2 s voraus; kommt gar nichts, endet die Schleife nach einer Gnadenfrist (100 ms), kommt es dichter, ist das Byte-Limit in Millisekunden erreicht. Real bleibt: die Haltezeit ist nach oben nicht sauber begrenzt, und das speist A2 und C7.
- **Fix (billig, risikoarm):** Absolutes Zeitbudget statt nachladbarer Gnadenfrist – Startzeitpunkt einmal vor der Schleife merken und zusätzlich gegen ein Gesamtlimit (z.B. 250 ms) prüfen. Die Umgruppierung der UI-Pfade (Kommando-Queue statt Direktaufruf unter zwei Locks) ist sauberer, aber angesichts der Regressionshistorie nicht vordringlich.
- **Sicherheit des Befunds:** Umstritten – ein Prüfer real/mittel, einer widerlegt („kein Deadlock, Zeitrechnung überzogen"). Ich folge weitgehend der Widerlegung; das Zeitbudget ist trotzdem sinnvoll.
- **Notiz (Review 2026-08-14):** Keine substanziellen Einwände.

---

### Ursachengruppe D – Rückgabewerte werden nirgends geprüft

### [Mittel] D1 – NVS-Rückgabewerte werden nirgends ausgewertet: Alarmzeit geht still verloren

- **Ort:** `Software/Firmware_aktuell/Wecker_13v00.ino:2011` (`nvrTask`), `:728-744` (`writeNVR`), `:2333` (`setup`), `:783` (`bumpResetCount`)
- **Was passiert:** Weder `data.begin()` noch die 15 `putBool`/`putInt` werden ausgewertet. Schlägt das Öffnen des Namespace fehl (beschädigter NVS, `ESP_ERR_NO_MEM`), laufen alle Writes ins Leere. `safeChange` wurde zu diesem Zeitpunkt bereits gelöscht (Z. 1656/1748/1771/1828), ein Nachholen findet nie statt.
- **Wann es passiert:** Nutzer stellt A1 von 06:00 auf 07:30. `nvrTask` erwacht, `begin()` scheitert. Display zeigt weiterhin 07:30 – bis zum nächsten Stromausfall; danach liest `readNVR()` 06:00 zurück. Gleiche Blindheit in `setup()` Z. 2333: schlägt `begin()` dort fehl, liefert `getBool("state", false)` den Default, `readNVR()` wird nie aufgerufen und das Gerät arbeitet ab jedem Boot still mit den Compile-Defaults.
- **Warum es zählt:** Der Wecker klingelt zur falschen Zeit, ohne dass irgendetwas darauf hindeutet.
- **Fix:** Rückgabewert von `data.begin()` an allen drei Stellen prüfen; in `nvrTask` bei `false` `safeChange` wieder setzen (Retry im nächsten Zyklus) und `[FEHLER]` ins Web-Log. `readNVR()` unabhängig vom `state`-Flag ausführen – die Defaults kommen ohnehin aus den `getX()`-Fallbacks.
- **Sicherheit des Befunds:** Bestätigt, beide Prüfer. **library_assumption**: `Preferences::begin()` liefert `bool`, `putX` wird bei nicht gestartetem Handle zum No-Op (Standardverhalten arduino-esp32).
- **Notiz (Review 2026-08-14):** Keine substanziellen Einwände.

---

### [Niedrig] D2 – Web-Log-Seite: `reserve(8192)` deckt den Worst Case nicht, `/log` reserviert gar nicht

- **Ort:** `Software/Firmware_aktuell/Wecker_13v00.ino:2123` und `:2259` (`webLogTask`)
- **Was passiert:** Fixes Markup ~2,5 kB + 40 Ringpufferzeilen à bis 127 Zeichen + 26 Byte Span-Overhead + Snapshots ergeben rund 8,7–9,8 kB – über der reservierten Kapazität. Ab dann realloziert jedes `html += entry` den kompletten Block. Der `/log`-Handler reserviert überhaupt nicht. Kein Rückgabewert (`reserve`, `+=`, `send`) wird geprüft.
- **Wann es passiert:** Während einer DFPlayer-Störung füllt sich der Puffer mit langen Zeilen (die Meldung aus Z. 332 ergibt real 119 Zeichen).
- **Warum es zählt:** Kein Crash (ein fehlgeschlagenes Realloc verwirft die Zuweisung folgenlos), aber die Diagnoseseite wird abgeschnitten ausgeliefert – genau dann, wenn man sie braucht.
- **Fix:** Seite streamen: `setContentLength(CONTENT_LENGTH_UNKNOWN)`, `send(200, …, "")` und die Blöcke einzeln per `sendContent()`. Minimal: `reserve()` auf 12288 anheben und `out` im `/log`-Handler ebenfalls reservieren.
- **Sicherheit des Befunds:** Bestätigt; Auswirkung nur auf das Wartungswerkzeug, daher niedrig.
- **Notiz (Review 2026-08-14):** Keine substanziellen Einwände.

---

### Ursachengruppe E – Bedien- und Hardwarepfade

### [Mittel] E1 – Klemmendes Touch-Pad: keine Rekalibrierung, Lautstärke fällt auf 0, Alarm bleibt stumm

- **Ort:** `Software/Firmware_aktuell/Wecker_13v00.ino:1531` und `:1570-1594` (`touchTask`)
- **Was passiert:** `TS_PRESSED`/`TS_REPEAT` haben keine Maximaldauer; einziger Ausstieg ist `!padPressed[activeIdx]`. Die Baseline-Rekalibrierung ist hart an `TS_IDLE` gekoppelt und findet dann nie wieder statt. `touchTask` hat als einziger Task **kein** `wdg_*`-Alive-Signal, wird also von nichts überwacht.
- **Wann es passiert:** Feuchtigkeit/Kondensation am 30 cm langen Twisted Pair oder ein Kabeldefekt hält ein Pad dauerhaft unter der Schwelle. Ab da alle 250 ms ein Event. Steht die UI auf `UI_CLOCK` und klemmt T4, zählt `onClock` die Lautstärke in ~3 s auf 0 herunter und sendet `player.volume(0)`. **`triggerAlarm()` setzt die Lautstärke nicht neu** – `player.volume()` existiert nur an drei Stellen (Z. 961, 976, 2505). Der nächste Alarm läuft real stumm. Nebeneffekt: `lastTouchMs` wird ständig erneuert, Auto-Return und Display-Timeout laufen nie ab.
- **Warum es zählt:** Ein Hardwarefehler wird durch die Firmware zum stummen Wecker verstärkt. Immerhin sichtbar (Display bleibt an, Lautstärke steht auf 00).
- **Fix:** Maximale Zusammenhangsdauer für `TS_PRESSED`/`TS_REPEAT` (z.B. 30 s → Zwangsrücksetzung auf `TS_IDLE` inkl. Baseline-Neuerfassung und `[WARNUNG]` ins Web-Log). Unabhängig davon in `triggerAlarm()` vor `playFolder()` ein `player.volume(vol)` senden, mit einer Untergrenze für den Alarmfall.
- **Sicherheit des Befunds:** Bestätigt. Die im Ursprungsbefund zusätzlich behauptete dauerhafte NVR-Commit-Blockade tritt so nicht ein: `markSafeChange()` wird am Anschlag (`vol == 0`) nicht mehr aufgerufen, der Commit läuft 2 s später.
- **Notiz (Review 2026-08-14):** Keine substanziellen Einwände.

---

### [Mittel] E2 – S3 liegt direkt auf dem Boot-Strapping-Pin GPIO0

- **Ort:** `Software/Firmware_aktuell/SysConf_13v00.h:452`, Nutzung `Wecker_13v00.ino:2444` / `:2468`
- **Was passiert:** `const uint8_t S3 = 0;` mit `INPUT_PULLUP` und `FALLING`-ISR, ohne Serienwiderstand, RC oder Diode. Verschärfend: T2 hängt auf GPIO2 (zweiter Strapping-Pin) mit 4,7 nF nach GND, ist beim Reset also sicher LOW – die Bedingung für UART-Download-Boot (GPIO0 = 0 **und** GPIO2 = 0) ist bei gedrücktem S3 vollständig erfüllt.
- **Wann es passiert:** Nur bei Resets, die den Strapping-Latch neu laden: Power-On (Netzteil einstecken nach Stromausfall), EN-Taster, Brownout. **Nicht** bei `ESP.restart()` – das ist auf dem ESP32 ein reiner CPU-Reset (`SW_CPU_RESET`), sonst würde der gesamte `rtcRetryMagic`-Mechanismus nicht funktionieren. Damit entfällt die häufigste im Ursprungsbefund behauptete Auslöserkette.
- **Warum es zählt:** Bleibt das Gerät im ROM-Downloadmodus, gibt es kein `setup()`, keine Tasks, kein Display und **keinen weiteren Startversuch** – schlimmer als ein Reboot-Loop. Realistisch wird das bei einem mechanisch verklemmten oder verschmutzten Taster im Holzgehäuse: dann ist jeder Netzausfall ein Dauerausfall.
- **Fix:** S3 auf einen GPIO ohne Strapping-Funktion legen (z.B. GPIO19/23 mit internem Pull-up) und Pin-Zuordnung in SysConf/CLAUDE.md/Schaltplan nachziehen. Falls die Verdrahtung bleiben muss: Diode oder RC, so dass ein Tastendruck GPIO0 in den ersten Millisekunden nach Reset nicht unter den LOW-Pegel zieht.
- **Sicherheit des Befunds:** Bestätigt, Schweregrad von „hoch" auf „mittel" korrigiert (Software-Reset-Pfad widerlegt). Das DevKitC hat ohnehin einen BOOT-Taster auf GPIO0 – S3 erhöht nur die Exposition.
- **Notiz (Review 2026-08-14):** Keine substanziellen Einwände.

---

### [Hoch] E5 – Motor hängt laut Schaltplan an 3,3 V, Firmware und Hardware-Notiz rechnen mit 5 V

- **Ort:** `Hardware/Schaltplan/Motor-Treiber.md:3`, `Hardware/Stückliste/Stückliste.md` (Abschnitt 3), gegen `SysConf_13v00.h:460-470` und `Hardware/hardware_notesmotor_led_driver.md:30-41`
- **Was passiert:** Der Schaltplan speist Motor+ über einen `AMS1117 3v3` aus der 5-V-Schiene, die Stückliste führt ihn ausdrücklich als „LDO 5 V → 3,3 V" im Motor-Treiber. Firmware-Kommentar und Hardware-Notiz gehen dagegen von 5 V am Motor aus und leiten daraus `MOTOR_PWM_DUTY 153` ab („60 % Duty ≙ ~3 V Mittelwert").
- **Wann es passiert:** Immer. Bei 3,3 V Rail ergeben 153/255 einen Mittelwert von **~1,98 V** statt der beabsichtigten 3 V — der 3-V-Getriebemotor läuft dauerhaft bei rund zwei Dritteln der Nennspannung. Für 3 V wären ~232/255 (91 %) nötig. Ebenso verschiebt sich `MOTOR_PWM_KICK_THRESHOLD 89`: 35 % sind an 3,3 V nur noch 1,15 V, der Kickstart setzt also zu spät ein.
- **Warum es zählt:** Erklärt ein zähes oder gar nicht anlaufendes Mühlrad, ohne dass die Firmware etwas Falsches täte. Gefährlicher ist der Dokumentationswiderspruch selbst: wer den Slider künftig kalibriert oder den Duty-Default anpasst, rechnet je nach gelesener Quelle mit einer anderen Rail. Nebenbei nennt `SysConf_13v00.h:456` die Freilaufdiode `1N4148`, Schaltplan und Stückliste `1N4448`.
- **Fix:** Zuerst mit dem Multimeter klären, welche Rail real anliegt (Motor+ gegen GND bei Duty 100 %). Dann **eine** Quelle korrigieren statt beider: bei 3,3 V `MOTOR_PWM_DUTY` auf ~232 und `MOTOR_PWM_KICK_THRESHOLD` entsprechend anheben, die 5-V-Kommentare in SysConf und `hardware_notesmotor_led_driver.md` auf 3,3 V ziehen. AMS1117-Verlustleistung bleibt unkritisch ((5−3,3) × 0,2 A ≈ 0,34 W).
- **Sicherheit des Befunds:** Bestätigt aus Schaltplan und Stückliste; welche der beiden Quellen den gebauten Aufbau beschreibt, ist ohne Messung am Gerät nicht entscheidbar — der Widerspruch selbst ist es.
- **Notiz (Review 2026-08-14):** Keine substanziellen Einwände.

---

### [Niedrig, umstritten] E3 – Kuckuck-Treiber (E1) ohne Freilaufdiode, Modul an 5 V statt 4,5 V

- **Ort:** `Hardware/Schaltplan/Kuckuck-Treiber.md:4`, Ansteuerung `Wecker_13v00.ino:1468`/`:1477`
- **Was passiert:** Der Kuckuck-Kanal schaltet eine vermutlich induktive Last (Platinenbeschriftung „4V5 115mA") ohne Freilaufpfad, anders als der Motorkanal (1N4448 + 10 Ω/10 nF Snubber). Zusätzlich liegt die volle 5-V-Schiene am Modul.
- **Warum ich es abschwäche:** Der MOSFET klemmt die Abschaltspitze per Lawinendurchbruch; die Energie liegt bei 39 Ω Lastwiderstand selbst mit großzügig angesetzter Induktivität im Bereich von Mikro- bis wenigen Millijoule, also Größenordnungen unter der EAS-Grenze eines SOT-23-Leistungs-MOSFET. Der Vergleich mit E2 trägt nicht: dort sind es 20.000 Abschaltvorgänge pro Sekunde (PWM), hier rund 20 pro Tag. Die Stückliste bildet den Kuckuck-Kanal ohnehin nicht ab (auch MOSFET und Widerstände fehlen dort).
- **Fix:** 1N4448 antiparallel zur Last ergänzen (gute Praxis, EMV neben der DFPlayer-Elektronik). Die 4,5-V-Frage ist bei 0,2 % Einschaltdauer thermisch irrelevant.
- **Sicherheit des Befunds:** Umstritten – ein Prüfer mittel, einer niedrig mit Energierechnung. Ich folge der Rechnung. Offene Annahme in beiden Richtungen: kein Datenblatt des Kuckuck-Moduls vorhanden.
- **Notiz (Review 2026-08-14):** Keine substanziellen Einwände.

---

### [Niedrig, umstritten] E4 – `runWifiConfigServer()` blockiert endlos vor der Task-Erstellung

- **Ort:** `Software/Firmware_aktuell/Wecker_13v00.ino:922` (`runWifiConfigServer`), Aufruf `:2359`
- **Was passiert:** `while (true) { server.handleClient(); delay(5); }` ohne Timeout, aufgerufen vor jeder Task-Erstellung (Z. 2592). Ohne WLAN-Zugangsdaten existieren weder `alarmTask` noch `displayTask` – das Gerät ist AP und kein Wecker.
- **Warum ich den ursprünglichen Fix ablehne:** Weiterbooten würde **nicht** helfen. Ohne Credentials ruft `WiFi.begin("", "")` dauerhaft ins Leere, es gibt nie eine NTP-Synchronisation, und mit den Default-Alarmen (06:00, `a1_on = true`) würde ein weiterlaufendes Gerät auf der 1970-Uhr zu einer beliebigen realen Uhrzeit klingeln – siehe A5. Der aktuelle Zustand ist zudem nicht still: das OLED zeigt dauerhaft „WiFi Einrichtung / WLAN: bTn-Wecker / Browser öffnen: 192.168.4.1".
- **Was real bleibt:** T3 auf der Info-Seite löscht die WLAN-Zugangsdaten ohne Rückfrage, direkt neben T4 (Werksreset). Ein Fehlgriff macht das Gerät bis zur Neukonfiguration zum AP.
- **Fix:** Bestätigungsabfrage für T3 (zweiter Druck innerhalb weniger Sekunden), nicht Timeout auf die Konfigurationsschleife.
- **Sicherheit des Befunds:** Umstritten; ich bewerte den Endlos-Loop als vertretbar, die fehlende Bestätigung als das eigentliche Thema.
- **Notiz (Review 2026-08-14):** Keine substanziellen Einwände.

---

## Nicht bestätigt

**Unbegrenzte Wartepfade in `sendStack()`/`available()` sind nicht die Ursache eines 15-s-TWDT-Hängers.** Es wurde geprüft, ob die innere `while (_serial->available())`-Schleife der Bibliothek den nachgelagerten Zeitcheck bei anhaltendem Rauschen „aushungern" kann. Sie kann es nicht: bei 9600 Baud fließen maximal ~960 Byte/s nach, die Schleife konsumiert ein Byte in Mikrosekunden – der Puffer ist nach wenigen Mikrosekunden leer und der Check `_isSending && millis() - _timeOutTimer >= _timeOutDuration` wird rund 960-mal pro Sekunde erreicht, nicht „nie". Zusätzlich beendet `handleError(WrongStack)` die ACK-Warteschleife, sobald ein 0x7E ohne gültige Folgebytes auftritt, und reines 0x00-Rauschen enthält nie ein 0x7E. Auch die Summe aller begrenzten Wartezeiten eines toten DFPlayers in einem `alarmTask`-Durchlauf liegt bei rund 5,5–6 s gegen 15 s TWDT-Budget.

**Das ist für dich direkt verwertbar:** die 12v14-Rücknahme des Bibliotheks-Patches war richtig, und der 12v13-Backtrace zeigte nur, *wo* der Task im Moment des Timeouts stand, nicht *dass* diese Schleife 15 s blockierte. Die dokumentierte Ursache (roher `Serial2.read()`-Discard, der die Bibliothek desynchronisierte) bleibt die tragfähige Erklärung. Ein erneuter Patch an `DFRobotDFPlayerMini.cpp` ist nicht nötig.

---

## Ungeprüft geblieben

- **Alle acht niedriger eingestuften Befunde wurden aus Kapazitätsgründen nicht gegengeprüft.** Zwei davon halte ich für lohnend genug, um sie hier zu nennen, ohne dass ich sie verifiziert habe:
  - `esp_task_wdt_init()`/`reconfigure()` und die drei `esp_task_wdt_add(NULL)` werten ihren Rückgabewert nicht aus, während Z. 2587 unbedingt „[TWDT] Hardware Watchdog aktiv" ins Log schreibt. Genau der 12v15-Fehler wäre damit symptomfrei.
  - Nach einem NTP-Timeout laufen `webLog("[NTP] Synchronisation OK")` (Z. 2437) und `snapTimeStr(snapNtpTime, …)` (Z. 2438) unbedingt weiter. Das Log behauptet eine erfolgreiche Synchronisation, und weil `snapNtpTime` dann nicht mehr leer ist, greift der 12v04-Nachtrag in `displayTask` (Z. 1867) beim späteren echten Sync nie. Wer nach einem ausgefallenen Alarm ins Log sieht, schließt einen Uhrzeitfehler damit fälschlich aus – **das ist im Zusammenspiel mit A1 und A5 diagnostisch teuer und ein Zweizeiler.**
  - Ebenfalls ungeprüft: `STACK_WATCHDOG = 1344` für einen Fehlerzweig mit `webLogf` (`char buf[128]` + `vsnprintf`) und drei SSD1306-Aufrufen; Race auf `motorRunning`/E2 zwischen `motorStart()`-Kickstart und `/motor`-HTTP-Handler; ungeschützte Snapshot-Strings im HTTP-Handler; `nvs_flash_erase()` beim Werksreset ohne Anhalten des `nvrTask`.
  - Der im Workflow noch als ungeprüft gelistete Punkt „AMS1117-3,3 V im Motorpfad gegen die 5-V-Annahme" wurde nachträglich gegen Schaltplan und Stückliste verifiziert und ist oben als **E5** aufgenommen.
- **Bibliotheksinterna sind grundsätzlich nicht verifizierbar.** `DFRobotDFPlayerMini` und `SSD1306Wire` liegen weder im Repo noch installiert vor. Betroffen sind C2 (`parseStack()` und der 0x41-ACK), C3 (Abbildung 0x40 → `DFPlayerError`), C4 (Semantik 0x48 vs. 0x4E), D1 (`Preferences`-Fehlerverhalten) und die Zeitrechnung in C8. Alle diese Punkte sind entsprechend markiert.
- **Keine Hardware-Verifikation.** Mutex-Haltezeiten, DFPlayer-Antwortlatenzen, Brownout-Verhalten beim Motoranlauf, tatsächliche Stack-High-Water-Marks im Fehlerzweig und die Spannung an Motor+ sind ausschließlich aus dem Code und der Dokumentation abgeleitet.
- **Nicht abgedeckt:** `WEB.h` und der WiFi-Konfigurator wurden nur oberflächlich betrachtet (kein Audit auf Eingabevalidierung der POST-Parameter); die UI-State-Machine (`uiDispatch`, `menu()`, alle `on*`-Handler) wurde nur dort gelesen, wo sie Player- oder NVR-Zustand berührt; die Touch-Kalibrierung wurde nicht auf Fehlauslösungen durch Netzbrummen geprüft; Speicherverhalten über Wochen Laufzeit (Heap-Fragmentierung durch `WebServer`/lwIP) wurde nicht gemessen, nur der `String`-Aufbau im Log-Handler bewertet.

---

## Empfohlene Reihenfolge

1. **A5 + A1 gemeinsam: `timeValid()`-Gate und tagesbezogene Fälligkeitsprüfung.** Höchste Wirkung, kein Kontakt zur DFPlayer-Logik. Das Gate muss zuerst da sein, sonst löst die pegelbasierte Prüfung auf der 1970-Uhr sofort nach Boot aus. Erledigt in einem Zug A1, A4, A5, A6 und macht A2 selbstheilend. Testbar ohne Hardware, indem man die Systemzeit per NTP-Server-Attrappe springen lässt.
2. **A3: Maximallaufzeit für `ALARM_RUNNING`.** Fünf Zeilen, keine Bibliotheksberührung, beseitigt den einzigen Zustand, aus dem das Gerät sich nie wieder erholt. Zusammen mit C1 (S1 wirkt bei `ALARM_RUNNING` unabhängig vom Playerstatus) – auch das ist reine Anwendungslogik.
3. **B2: RTC-Merker vor dem riskanten Abschnitt setzen, nach dem ersten erfolgreichen Poll löschen.** Macht die gesamte Absturz-Wiederherstellung unabhängig davon, welcher Watchdog feuert, und entschärft B1 ohne Timing-Änderung. Nicht vor Schritt 1 durchführen – ohne das Nachholfenster kann ein spät im Alarm liegender Reboot sonst zu einem verspäteten Zweitalarm führen.
4. **C5 + D1: Clamp nur bei `mp3Count > 0`, NVS-Rückgabewerte prüfen und bei Fehlschlag `safeChange` wieder setzen.** Zwei kleine, isolierte Änderungen gegen stillen Konfigurationsverlust. Kein Regressionsrisiko.
5. **B1 sekundär: `WDG_TIMEOUT_MS` = 6000, `WDG_CHECK_MS` = 1000.** Reine SysConf-Änderung, gibt dem Software-Watchdog seine Funktion zurück. Erst nach Schritt 3, damit man am Feldverhalten sieht, ob der neue Schwellwert Fehlalarme erzeugt.
6. **C3: `st == 0` von `st == -1` trennen.** Verhindert zwei sinnlose Neustarts im Alarmfenster und den anschließenden Totalausfall ohne Motor und Licht. Berührt die DFPlayer-Logik, aber nur die Auswertung – kein UART-Verhalten. Getrennt committen.
7. **C4: `readFileCountsInFolder(1)` statt `readFileCounts() - 1`, kein 99-Fallback.** Beseitigt die Ursache für C3. Neuer Bibliotheksaufruf, daher nach C3 und einzeln testen.
8. **C2: `drainSerial2Pre()` auf Fortschrittsprüfung umstellen, `readStateDrained()` vorher drainen.** **Zuletzt und ausdrücklich isoliert.** Dies ist genau der Bereich, der in 12v09–12v14 dreimal hintereinander Regressionen erzeugt hat (Puffer-Desync, Bibliotheks-Patch, ACK-Modus). Vorher unbedingt die Zeilen aus Schritt 1–3 als stabilen Stand festhalten, damit ein Rückbau möglich bleibt. Testprotokoll: Kaltstart, Startsound, Sound-Vorschau beider Alarme, Lautstärkeänderung, echter Alarm, S1-Stopp, Alarm mit gezogener SD-Karte, jeweils mit offenem Web-Log und Blick auf „Serial2 Restbytes".
9. **E5: Rail am Motor messen und danach genau eine Quelle korrigieren.** Kostet fünf Minuten mit dem Multimeter, räumt einen Widerspruch zwischen Schaltplan, Stückliste, Firmware-Kommentar und Hardware-Notiz aus und erklärt möglicherweise ein zähes Mühlrad. Unabhängig von allem anderen, jederzeit machbar.
10. **E1 (Touch-Haltedauer + `player.volume()` in `triggerAlarm()`), E2 (S3 von GPIO0 weg), C6, C7, D2, E4.** Wirkung geringer oder Hardwareeingriff nötig; E2 nur bei der nächsten ohnehin anstehenden Platinenänderung.
11. **Die zwei ungeprüften Zweizeiler aus „Ungeprüft geblieben"** (TWDT-Rückgabewerte, NTP-Erfolgsmeldung nach Timeout) mitnehmen, sobald ohnehin an `setup()` gearbeitet wird – beide kosten nichts und verbessern die Diagnostizierbarkeit aller obigen Fälle.