# bTn Wecker – Export für Neuaufbau in ESP-IDF

*Stand: Firmware 22v03 (Arduino) · Hardware 2v0 · erstellt 2026-11-22*

## 0. Auftrag und Randbedingungen

Dieses Dokument ist die **alleinige Arbeitsgrundlage** für eine neue Implementierung des bTn Weckers in **ESP-IDF** (kein Arduino-Core). Die ursprüngliche Firmware (Arduino, FreeRTOS, ca. 3500 Zeilen) liegt nicht vor und wird nicht gebraucht.

| Punkt                  | Vorgabe                                                                                                             |
| ---------------------- | ------------------------------------------------------------------------------------------------------------------- |
| Hardware               | **1:1 übernehmen**, keine Pin-, Bauteil- oder Schaltungsänderung (Abschnitt 1)                                      |
| Funktionen             | Alle nach außen sichtbaren Funktionen bleiben erhalten (Abschnitte 3 bis 10)                                        |
| Eingaben               | Touch T0/T2/T3/T4, Taster S1/S2/S3 mit identischer Wirkung, identischem Hold-/Repeat- und Entprell-Verhalten        |
| Display                | **Pixelgleiche Darstellung** auf dem 128×64-OLED: gleiche Schrift, gleiche Koordinaten, gleiche Texte (Abschnitt 4) |
| Web-Log / Konfigurator | Gleiche Endpunkte, gleiche Optik und gleiche Bedienelemente (Abschnitte 9 und 10, Anhang A)                         |
| Freiheit               | Interne Architektur (Tasks, Queues, Mutexe, Stack-Größen, Code-Struktur) ist frei wählbar                           |
| Sprache                | Deutsch (Anzeigetexte, Kommentare, Dokumentation)                                                                   |
| Versionsformat         | `NvNN` (z. B. `30v00`), kein führendes `v`. Versionsstring `bTn_Wecker_<Version>` erscheint auf Display und Web-Log |
| Toolchain              | ESP-IDF 5.x (Ziel: ESP32, nicht S2/S3), CMake-Projekt, `idf.py build`                                               |

Maßgeblich bei Widersprüchen zwischen alter Bedienungsanleitung und diesem Dokument ist **dieses Dokument** (es beschreibt das tatsächliche Verhalten des Codes, siehe Abschnitt 13).

---

## 1. Hardware (unverändert zu übernehmen)

### 1.1 Komponenten

| Komponente | Beschreibung                                                             |
| ---------- | ------------------------------------------------------------------------ |
| MCU        | ESP32 Dev Kit C V4 (ESP32-WROOM-32, Dual-Core)                           |
| Display    | SSD1306 OLED 128×64, I²C, Adresse 0x3C, SDA=GPIO21, SCL=GPIO19           |
| MP3-Player | DFPlayer Mini mit microSD, UART2, 9600 Baud 8N1, zusätzlich BUSY-Leitung |
| Touch      | 4 kapazitive Felder T0/T2/T3/T4                                          |
| Taster     | S1, S2, S3 (S3 ist die BOOT-Taste des DevKit, GPIO0)                     |
| Ausgang E1 | Kuckuck-Modul (digital, MOSFET)                                          |
| Ausgang E2 | Mühlrad-Motor 3 V (PWM, MOSFET, Rail über LDO auf 3,3 V)                 |
| Ausgang E3 | LED-Streifen „Licht“ (digital, MOSFET, 47 Ω Vorwiderstand)               |

### 1.2 Pinbelegung

| GPIO | Name          | Funktion                                 | Betriebsart                                |
| ---- | ------------- | ---------------------------------------- | ------------------------------------------ |
| 0    | S3            | Taster S3 (Info-Seite)                   | Eingang, interner Pull-up, Strapping-Pin   |
| 2    | T2            | Touch T2 (Touch-Pad 2)                   | Touch, Strapping-Pin                       |
| 4    | T0            | Touch T0 (Touch-Pad 0)                   | Touch                                      |
| 13   | T4            | Touch T4 (Touch-Pad 4)                   | Touch                                      |
| 15   | T3            | Touch T3 (Touch-Pad 3)                   | Touch, Strapping-Pin                       |
| 16   | RXD2          | UART2 RX ← DFPlayer TX                   | UART2                                      |
| 17   | TXD2          | UART2 TX → DFPlayer RX                   | UART2                                      |
| 19   | OLED_SCL      | I²C SCL                                  | I²C über GPIO-Matrix                       |
| 21   | OLED_SDA      | I²C SDA                                  | I²C                                        |
| 25   | E2            | Mühlrad-Motor (MOSFET)                   | LEDC-PWM 20 kHz, 8 Bit                     |
| 26   | E3            | LED-Licht (MOSFET)                       | Ausgang digital                            |
| 27   | E1            | Kuckuck (MOSFET)                         | Ausgang digital                            |
| 32   | S2            | Taster S2 (Zugschalter Licht + Mühlrad)  | Eingang, interner Pull-up                  |
| 33   | S1            | Taster S1 (Alarm aus / Kuckuck einmalig) | Eingang, interner Pull-up                  |
| 35   | DFPLAYER_BUSY | DFPlayer BUSY (LOW = Wiedergabe läuft)   | Eingang (input-only), kein Pull-up möglich |

Alle übrigen GPIOs sind nicht beschaltet. Die Taster schalten gegen GND (aktiv LOW, Flanke FALLING).

### 1.3 Beschaltung

- **Touch:** je 510 Ω Reihenwiderstand, keine externe Kapazität (volle Empfindlichkeit), ca. 30 cm twisted pair.
- **Taster S1/S2:** je 47 nF Keramikkondensator nach GND direkt am Pin, ohne Serienwiderstand (Hardware-Entprellung, τ ca. 2 ms mit internem Pull-up).
- **Ausgänge E1 bis E3:** je ein MOSFET IRLML6344TRPBF; Gate über 470 Ω, Pull-Down 10 kΩ (Gate nach GND, an allen drei Gates).
  - E1 (Kuckuck): Modul zwischen 5 V und Drain; 100 µF und 100 nF an 5 V.
  - E2 (Motor): Rail 5 V → LDO MCP1700T-3302E/TO → 3,3 V; Freilaufdiode 1N4448 (Kathode an 3,3 V); 100 nF am Motor; PWM 20 kHz, voller Bereich 0..255 nutzbar (Motor sieht max. 3,3 V).
  - E3 (Licht): 5 V → 47 Ω/0,25 W → LED-Streifen (2,8 V/48 mA) → Drain.
- **DFPlayer-BUSY:** BUSY (Pin 16) → 1 kΩ → GPIO35, 100 nF von GPIO35 nach GND (RC-Filter, τ = 100 µs). BUSY wird aktiv getrieben: LOW = Wiedergabe läuft, HIGH = Idle.
- **DFPlayer:** VCC 5 V, GND als Sternpunkt, Lautsprecher an SPK_1/SPK_2, alle übrigen Pins unbeschaltet.
- **Strapping-Pins:** GPIO0, 2, 15 werden als Taster/Touch genutzt. Beim Boot dürfen sie nicht aktiv getrieben werden.

### 1.4 SD-Karte (DFPlayer)

| Ordner / Datei               | Inhalt                                                               |
| ---------------------------- | -------------------------------------------------------------------- |
| `01/001.mp3`, `01/002.mp3` … | Alarm-Sounds, fortlaufend nummeriert, wählbar auf den Seiten 3 und 4 |
| `02/001.mp3`                 | Startton beim Einschalten (Boot)                                     |

Die Anzahl der Dateien in Ordner 01 wird beim Boot per DFPlayer-Befehl ermittelt (`mp3Count`) und begrenzt die Sound-Auswahl.

---

## 2. Empfohlene ESP-IDF-Bausteine

Nur Empfehlung, nicht bindend. Das sichtbare Verhalten ist maßgeblich.

| Aufgabe    | ESP-IDF-Mittel                                                                                                           |
| ---------- | ------------------------------------------------------------------------------------------------------------------------ |
| OLED       | eigener SSD1306-Treiber über `i2c_master` (Framebuffer 128×64, Seitenadressierung) plus Port der Schrift (Abschnitt 4.1) |
| Touch      | `driver/touch_pad.h` (Legacy-API des ESP32): `touch_pad_init`, `touch_pad_config`, `touch_pad_read`                      |
| Taster     | `gpio` mit ISR (FALLING), Events in eine Queue                                                                           |
| Motor-PWM  | `ledc` (20 kHz, 8 Bit, ein Kanal auf GPIO25)                                                                             |
| DFPlayer   | eigener Treiber über `uart` (Protokoll Abschnitt 11)                                                                     |
| Persistenz | `nvs_flash` / `nvs_*`, Namespaces `varSafe` und `wifiCfg`                                                                |
| WLAN / NTP | `esp_wifi` (STA und AP), `esp_netif`, `esp_sntp`                                                                         |
| Zeitzone   | `setenv("TZ", "CET-1CEST,M3.5.0/02,M10.5.0/03", 1); tzset();`                                                            |
| Webserver  | `esp_http_server` (Port 8080 für Web-Log, Port 80 für den AP-Konfigurator)                                               |
| Watchdog   | Task-Watchdog (TWDT) des IDF; Anwendungs-Watchdog optional                                                               |

Touch-Besonderheit des ESP32: Der Rohwert **sinkt** bei Berührung. Erkennung: `wert < baseline - TOUCH_DROP`.

---

## 3. Eingabeverarbeitung

### 3.1 Touch-Felder

| Feld | GPIO | Touch-Pad | Event  |
| ---- | ---- | --------- | ------ |
| T0   | 4    | NUM0      | EVT_T0 |
| T2   | 2    | NUM2      | EVT_T2 |
| T3   | 15   | NUM3      | EVT_T3 |
| T4   | 13   | NUM4      | EVT_T4 |

Initialisierung: `touch_pad_init()`, Spannungen HVOLT 2,7 V / LVOLT 0,5 V / Dämpfung 1 V, alle vier Pads konfigurieren, **300 ms** einschwingen lassen, dann pro Pad die **Baseline** messen.

Abtastung alle **50 ms** (`TOUCH_POLL_MS`). Schwellwert je Pad:

- Normal (Baseline > 150): `thr = baseline - 150` (`TOUCH_DROP = 150`).
- Fallback (Baseline ≤ 150): `thr = baseline - baseline/5`.
- Pad gilt als berührt, wenn `wert < thr`.

Zustandsautomat (immer nur **ein** Pad aktiv, alle anderen werden ignoriert, bis das aktive losgelassen ist):

| Zustand     | Übergang                                                                                                                                                                                                             |
| ----------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| TS_IDLE     | Pad berührt → **sofort ein Event senden**, weiter nach TS_PRESSED                                                                                                                                                    |
| TS_PRESSED  | losgelassen → TS_IDLE. Nach **750 ms** (`TOUCH_HOLD_MS`) ein weiteres Event, weiter nach TS_REPEAT                                                                                                                   |
| TS_REPEAT   | alle **250 ms** (`TOUCH_REPEAT_MS`) ein Event. Loslassen → TS_IDLE                                                                                                                                                   |
| Zwangsreset | Ist ein Pad **30 s** (`TOUCH_MAX_HOLD_MS`) ununterbrochen aktiv: zurück nach TS_IDLE, alle Baselines neu messen, Log-Zeile `[WARNUNG] Touch <T0/T2/T3/T4> klemmt seit <ms> ms – Zwangs-Reset, Baseline neu gemessen` |

Baseline-Rekalibrierung: alle **10 min** (`TOUCH_RECAL_MS`), nur wenn TS_IDLE. Die Baseline-Messung wird für das Web-Log gemerkt (Abschnitt 10).

Die Events werden in eine Queue (Tiefe 32) gelegt. Erst die Eingabe-Verarbeitung wertet sie aus (Abschnitt 5).

### 3.2 Taster

| Taster | GPIO | Event  | Flanke  |
| ------ | ---- | ------ | ------- |
| S1     | 33   | EVT_S1 | FALLING |
| S2     | 32   | EVT_S2 | FALLING |
| S3     | 0    | EVT_S3 | FALLING |

Zwei Entprellstufen:

1. **ISR:** Flanken, die weniger als **30 ms** (`BTN_DEBOUNCE_MS`) nach der letzten akzeptierten Flanke desselben Tasters kommen, werden verworfen.
2. **Verarbeitung:** Nach einer ausgeführten Aktion wird derselbe Taster für **1000 ms** (`BTN_LOCKOUT_MS`) gesperrt (gilt für S1, S2 und S3, je Taster eigener Zeitstempel). Touch-Events haben **keine** Aktionssperre.

Beim Boot werden S1, S2, S3 mit internem Pull-up als Eingang konfiguriert, die Interrupts aber erst kurz vor dem Start der Tasks aktiviert. Vor dem Task-Start wird die Event-Queue geleert. **S2 wird in den ersten 3000 ms** (`S2_BOOT_IGNORE_MS`) **nach dem Task-Start ignoriert** (Schutz vor Störflanken beim Einschalten, die sonst Licht/Motor starten).

### 3.3 Reihenfolge der Auswertung eines Events

1. Ist das Display abgeschaltet (Abschnitt 4.6) und das Event ein Touch (T0/T2/T3/T4): Display einschalten, Timer neu starten, **Event verwerfen**.
2. Ist das Display abgeschaltet und das Event S3: Display einschalten, Timer neu starten, Event **weiter auswerten** (öffnet die Info-Seite).
3. S1 und S2 werden unabhängig vom Display ausgeführt (Abschnitt 6.5 und 6.6).
4. T0 bis T4 und S3 setzen den Zeitstempel `lastTouchMs` (Basis für Auto-Rückkehr und Display-Timeout). S1/S2 setzen ihn **nicht**.
5. Alle übrigen Events laufen durch den UI-Zustandsautomat (Abschnitt 5).

---

## 4. Display

### 4.1 Grundlagen (für pixelgleiche Darstellung zwingend)

- Controller SSD1306, 128×64, I²C 0x3C, **Bild um 180° gedreht** (Original: `flipScreenVertically()`, entspricht Segment-Remap 0xA0 und COM-Scan 0xC0 statt 0xA1/0xC8). Am Gerät prüfen: Bild muss im verbauten Gehäuse aufrecht stehen.
- Monochrom, Vordergrund WHITE auf schwarzem Grund. Koordinatenursprung oben links, x 0..127, y 0..63. Zeichnen außerhalb des Bildes wird abgeschnitten (z. B. 16-Punkt-Text bei y=49 ragt bis y=67 und wird unten beschnitten).
- Schriften **ArialMT_Plain_10** (Zellhöhe 13 px) und **ArialMT_Plain_16** (Zellhöhe 19 px) aus der Bibliothek *ThingPulse esp8266-oled-ssd1306* (`OLEDDisplayFonts.h`, MIT-Lizenz, https://github.com/ThingPulse/esp8266-oled-ssd1306). Die Schriftdaten **unverändert übernehmen** (proportionale Schrift, Zeichen 32..255). Ohne diese Schriftdaten ist keine pixelgleiche Darstellung möglich.
- `drawString(x, y, text)` wie in `OLEDDisplay::drawStringInternal`: `y` ist die **Oberkante** der Zelle (keine Grundlinie). Ausrichtung LEFT: Text beginnt bei `x`. CENTER: Text wird um `x` zentriert (`x - breite/2`). RIGHT: Text endet bei `x` (`x - breite`). Breite = Summe der Zeichenbreiten der Schrift.
- Umlaute stehen im Quelltext als UTF-8 (`ä ö ü`). Die Bibliothek wandelt sie in die Zeichencodes der Schrift (Latin-1-Belegung, ä=0xE4, ö=0xF6, ü=0xFC). Den gleichen Mapping-Mechanismus mitportieren.
- Änderungen werden im Framebuffer gesammelt und mit **einem** `display()` (Gesamtübertragung) ausgegeben.

Hilfsfunktionen (Original, mit Kurzform für dieses Dokument):

| Kürzel         | Bedeutung                                                 |
| -------------- | --------------------------------------------------------- |
| `Z10L(x,y,t)`  | Schrift 10, linksbündig                                   |
| `Z10C(x,y,t)`  | Schrift 10, zentriert                                     |
| `Z10R(x,y,t)`  | Schrift 10, rechtsbündig                                  |
| `Z16L(x,y,t)`  | Schrift 16, linksbündig                                   |
| `Z16C(x,y,t)`  | Schrift 16, zentriert                                     |
| `CLR(x,y,w,h)` | Rechteck mit Schwarz füllen (löschen), danach Farbe WHITE |
| `BOX(x,y)`     | leeres Rechteck 10×10 (`drawRect`)                        |
| `TICK(x,y)`    | gefülltes Rechteck 4×4 (`fillRect`)                       |

### 4.2 Textpuffer

| Variable                    | Format           | Beispiel     | Bedeutung                                       |
| --------------------------- | ---------------- | ------------ | ----------------------------------------------- |
| `zeit`                      | `%02u:%02u:%02u` | `07:05:09`   | aktuelle Ortszeit                               |
| `datum`                     | `%02u.%02u.%04u` | `22.11.2026` | aktuelles Datum                                 |
| `str_vol`                   | `%02u`           | `09`         | Lautstärke                                      |
| `str_a1/a2`                 | `%02u:%02u`      | `06:00`      | Weckzeit 1 / 2                                  |
| `str_s1/s2`                 | `%03u`           | `001`        | auf den Sound-Seiten **gewählte** Dateinummer   |
| `str_s1_play`/`str_s2_play` | `%03u`           | `001`        | zuletzt **abgespielte/zugewiesene** Dateinummer |
| `str_cot/coff`              | `%02u`           | `06`         | Kuckuck von-/bis-Stunde                         |
| `str_mp3`                   | `%03u`           | `012`        | Anzahl MP3 in Ordner 01 (Info-Seite)            |
| `str_reset`                 | `%04lu`          | `0007`       | Neustart-Zähler (Info-Seite)                    |

### 4.3 Seiten (vollständige Zeichenoperationen)

Beim Seitenwechsel wird die Seite wie unten beschrieben gezeichnet (Seiten 2 und 4 ohne vorheriges Löschen, siehe dortige Hinweise).

**Seite 0 – Uhrzeit** (`display.clear()` zuerst)

| Op    | x   | y   | Text                       |
| ----- | --- | --  | -------------------------- |
| Z16C  | 64  | 0   | `zeit`                     |
| Z10L  | 1   | 17  | `datum`                    |
| Z10L  | 77  | 17  | `Volume`                   |
| Z10L  | 115 | 17  | `str_vol`                  |
| Z16C  | 30  | 32  | `Alarm 1`                  |
| Z16C  | 30  | 49  | `Alarm 2`                  |
| Z16C  | 105 | 32  | `str_a1`                   |
| Z16C  | 105 | 49  | `str_a2`                   |

Checkboxen: `BOX(67,37)` (Alarm 1) und `BOX(67,54)` (Alarm 2); ist der Alarm aktiv, zusätzlich `TICK(70,40)` bzw. `TICK(70,57)`. Beide Checkboxen werden gezeichnet.

**Seite 1 – Alarm einstellen** (`display.clear()` zuerst)

| Op    | x   | y   | Text                 |
| ----- | --- | --  | -------------------- |
| Z16C  | 64  | 0   | `Alarm einstellen`   |
| Z16C  | 5   | 32  | `>`                  |
| Z16C  | 15  | 32  | `>`                  |
| Z16C  | 40  | 32  | `A1`                 |
| Z16C  | 40  | 49  | `A2`                 |
| Z16C  | 105 | 32  | `str_a1`             |
| Z16C  | 105 | 49  | `str_a2`             |

Checkbox: **nur** `BOX(67,37)` (+ `TICK(70,40)` falls Alarm 1 aktiv). Die Box von Alarm 2 wird auf dieser Seite nicht gezeichnet.

**Seite 2 – Alarm 2 einstellen** (Teilaktualisierung der Seite 1, **kein** `clear()`)

1. `CLR(0,32,25,15)` (löscht die beiden Pfeile in Zeile A1)
2. `Z16C(5,49,">")`, `Z16C(15,49,">")`
3. `Z16C(105,49,str_a2)`
4. Beide Checkbox-Felder `(67,37,10,10)` und `(67,54,10,10)` schwarz löschen, dann **nur** `BOX(67,54)` (+ `TICK(70,57)` falls Alarm 2 aktiv).

Titel „Alarm einstellen“, „A1“, „A2“ und `str_a1` bleiben aus Seite 1 stehen. Sichtbares Ergebnis: Zeile A1 ohne Pfeile und ohne Checkbox, Zeile A2 mit Pfeilen und Checkbox.

**Seite 3 – Sound 1 wählen** (`display.clear()` zuerst)

| Op    | x   | y   | Text               |
| ----- | --- | --  | ------------------ |
| Z16C  | 64  | 0   | `Sound wählen`     |
| Z10L  | 0   | 17  | `Alarm 1:`         |
| Z10L  | 66  | 17  | `Alarm 2:`         |
| Z10L  | 42  | 17  | `str_s1_play`      |
| Z10L  | 108 | 17  | `str_s2_play`      |
| Z16C  | 5   | 32  | `>`                |
| Z16C  | 15  | 32  | `>`                |
| Z16C  | 40  | 32  | `A1`               |
| Z16C  | 40  | 49  | `A2`               |
| Z16C  | 105 | 32  | `str_s1`           |
| Z16C  | 105 | 49  | `str_s2`           |

Beim Betreten: `sound1_on = false; sound2_on = false`. Checkbox: beide Felder löschen, **nur** `BOX(67,37)` (leer, da Vorschau aus). Ist dabei der Player noch beschäftigt (BUSY LOW), wird er gestoppt (Abschnitt 6.8).

**Seite 4 – Sound 2 wählen** (Teilaktualisierung der Seite 3, **kein** `clear()`)

1. `CLR(108,17,20,10)`, `Z10L(108,17,str_s2_play)`
2. `CLR(0,32,25,15)`, `Z16C(5,49,">")`, `Z16C(15,49,">")`
3. `Z16C(105,49,str_s2)`
4. `sound2_on = false`; beide Checkbox-Felder löschen, **nur** `BOX(67,54)` zeichnen. Player ggf. stoppen.

**Seite 5 – Funktion wählen** (vorher: laufende Wiedergabe stoppen, Abschnitt 6.8; dann `clear()`)

| Op    | x   | y   | Text               |
| ----- | --  | --  | ------------------ |
| Z10C  | 64  | 0   | `Funktion wählen`  |
| Z16C  | 5   | 15  | `>`                |
| Z16C  | 15  | 15  | `>`                |
| Z16L  | 50  | 15  | `Kuckuck`          |
| Z16C  | 5   | 32  | `>`                |
| Z16C  | 15  | 32  | `>`                |
| Z16L  | 50  | 32  | `Licht`            |
| Z16C  | 5   | 49  | `>`                |
| Z16C  | 15  | 49  | `>`                |
| Z16L  | 50  | 49  | `Mühlrad`          |

Checkboxen (alle drei werden immer gezeichnet): `BOX(32,20)`, `BOX(32,37)`, `BOX(32,54)`; bei aktiver Funktion `TICK(35,23)` (Kuckuck), `TICK(35,40)` (Licht), `TICK(35,57)` (Mühlrad).

**Seite 6 – Kuckuck-Zeit** (`clear()` zuerst)

| Op    | x   | y   | Text               |
| ----- | --- | --  | ------------------ |
| Z16C  | 64  | 0   | `Kuckuck aktiv`    |
| Z10L  | 1   | 36  | `Taste +`          |
| Z16C  | 60  | 32  | `von`              |
| Z16C  | 91  | 32  | `str_cot`          |
| Z16L  | 100 | 32  | `:00`              |
| Z10L  | 1   | 52  | `Taste -`          |
| Z16C  | 60  | 49  | `bis`              |
| Z16C  | 91  | 49  | `str_coff`         |
| Z16L  | 100 | 49  | `:00`              |

**Seite 7 – Info** (`clear()` zuerst)

| Op    | x   | y   | Text                                                           |
| ----- | --- | --  | -------------------------------------------------------------- |
| Z10C  | 63  | 0   | `bTn_Wecker_<Version>`                                         |
| Z10C  | 63  | 16  | `<IP>:8080` (z. B. `192.168.1.42:8080`)                        |
| Z10L  | 1   | 28  | `MP3`                                                          |
| Z10L  | 28  | 28  | `str_mp3`                                                      |
| Z10C  | 78  | 28  | `RESET`                                                        |
| Z10R  | 127 | 28  | `str_reset`                                                    |
| Z10L  | 1   | 40  | `Taste +`                                                      |
| Z10R  | 127 | 40  | `WiFi RESET`                                                   |
| Z10L  | 1   | 54  | `Taste -`                                                      |
| Z10R  | 127 | 54  | `Werks-RESET`                                                  |

### 4.4 Teilaktualisierungen im Betrieb

Jeweils `CLR` + neu zeichnen + `display()`:

| Anlass                               | Löschen `CLR(x,y,w,h)` | Zeichnen                   |
| ------------------------------------ | ---------------------- | -------------------------- |
| Uhr (jede Sekunde, nur Seite 0)      | `(20,0,120,16)`        | `Z16C(64,0,zeit)`          |
| Lautstärke ändert sich (Seite 0)     | `(115,17,15,10)`       | `Z10L(115,17,str_vol)`     |
| Alarm-1-Zeit ändert sich (Seite 1)   | `(82,34,46,13)`        | `Z16C(105,32,str_a1)`      |
| Alarm-2-Zeit ändert sich (Seite 2)   | `(82,51,46,13)`        | `Z16C(105,49,str_a2)`      |
| Sound-1-Nummer ändert sich (Seite 3) | `(82,34,46,13)`        | `Z16C(105,32,str_s1)`      |
| Sound-2-Nummer ändert sich (Seite 4) | `(82,51,46,13)`        | `Z16C(105,49,str_s2)`      |
| Vorschau Sound 1 startet (Seite 3)   | `(42,17,20,10)`        | `Z10L(42,17,str_s1_play)`  |
| Vorschau Sound 2 startet (Seite 4)   | `(108,17,20,10)`       | `Z10L(108,17,str_s2_play)` |
| Kuckuck von-Stunde (Seite 6)         | `(78,35,23,13)`        | `Z16C(91,32,str_cot)`      |
| Kuckuck bis-Stunde (Seite 6)         | `(78,52,23,13)`        | `Z16C(91,49,str_coff)`     |

Checkbox-Aktualisierung (Seiten 0 bis 5): Feld(er) schwarz löschen, `BOX` neu zeichnen, `TICK` falls aktiv, `display()`.

- Seite 0: Felder `(67,37,10,10)` und `(67,54,10,10)` löschen, beide Boxen neu.
- Seite 1: beide Felder löschen, nur Alarm-1-Box. Seite 2: beide Felder löschen, nur Alarm-2-Box.
- Seite 3: beide Felder löschen, nur Sound-1-Box (an der Stelle `(67,37)`). Seite 4: beide löschen, nur Sound-2-Box `(67,54)`.
- Seite 5: Felder `(32,20,10,10)`, `(32,37,10,10)`, `(32,54,10,10)` löschen, alle drei neu.

Der Mitternachts-Neuaufbau: Auf Seite 0 wird um 00:00:00 bis 00:00:01 **einmalig** die komplette Seite 0 neu gezeichnet (damit das Datum wechselt). Das Datum wird sonst nicht aktualisiert.

### 4.5 Boot- und Systemanzeigen

Alle Texte zentriert (`C`), vor jeder dieser Bildschirme `clear()` außer wo „CLR“ angegeben. Jeweils mit `display()` ausgeben.

| Situation                     | Zeichenoperationen                                                                                                                                        |
| ----------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Start (nach Display-Init)     | `Z10C(64,16,"bTn_Wecker_<Version>")`                                                                                                                      |
| Warten auf WLAN               | `Z10C(64,8,<Version>)`, `Z16C(64,32,"warte auf")`, `Z16C(64,49,"WLAN ...")`                                                                               |
| WLAN-Timeout (30 s)           | `CLR(0,32,128,32)`, `Z16C(64,32,"kein WLAN")`, `Z10C(64,49,"weiter ohne NTP")`, 2 s halten                                                                |
| Warten auf NTP                | `CLR(0,49,128,15)`, `Z16C(64,49,"NTP ...")`                                                                                                               |
| NTP-Timeout (30 s)            | `CLR(0,49,128,15)`, `Z10C(64,49,"NTP Timeout")`, 2 s halten                                                                                               |
| DFPlayer-Initialisierung      | `CLR(0,49,128,15)`, `Z16C(64,49,"Sound ...")`                                                                                                             |
| WLAN-Konfigurator (AP aktiv)  | `Z10C(64,2,<Version>)`, `Z10C(64,16,"WiFi Einrichtung")`, `Z10C(64,28,"WLAN: bTn-Wecker")`, `Z10C(64,40,"Browser oeffnen:")`, `Z10C(64,52,"192.168.4.1")` |
| Konfigurator gespeichert      | `Z16C(64,22,"Gespeichert!")`, `Z16C(64,42,"Neustart ...")`, 0,5 s, Neustart                                                                               |
| Info-Seite: T3 (WLAN-Reset)   | `Z16C(64,16,"WiFi-Setup")`, `Z16C(64,32,"Neustart ...")`, 1,5 s, Neustart                                                                                 |
| Info-Seite: T4 (Werksreset)   | `Z16C(64,10,"Werksreset")`, `Z16C(64,26,"NVS wird")`, `Z16C(64,42,"gelöscht ...")`, 2 s, NVS löschen, Neustart                                            |
| Task-Freeze (Watchdog)        | `Z10C(64,8,"WATCHDOG")`, `Z10C(64,24,"Task Freeze!")`, `Z10C(64,40,"Neustart...")`, 2 s, Neustart                                                         |
| Interner Fehler (RTOS-Objekt) | `Z10C(64,8,"RTOS FEHLER")`, `Z10C(64,24,<Text>)`, `Z10C(64,40,"Neustart in 3s")`, 3 s, Neustart                                                           |

Nach dem Boot erscheint Seite 0.

### 4.6 Display-Timeout und Auto-Rückkehr

- **Auto-Rückkehr:** Ist die Seite ≠ 0 und seit dem letzten Touch/S3-Event (`lastTouchMs`) sind **20 s** (`AUTO_RETURN_MS`) vergangen, wechselt das Gerät zu Seite 0 (auch von der Info-Seite).
- **Display aus:** Sind seit `lastTouchMs` **300 s** (`DISPLAY_TIMEOUT_MS`) vergangen, wird das OLED abgeschaltet (`displayOff`, Befehl 0xAE). Beim Aufwecken `displayOn` (0xAF); der Framebuffer-Inhalt bleibt erhalten.
- **Aufwecken:** Touch T0 bis T4 weckt nur (Event verworfen). S3 weckt und öffnet die Info-Seite. Ein Alarmstart weckt das Display ebenfalls.
- Die Anzeige-Task läuft im 300-ms-Takt (`DISPLAY_UPDATE_MS`); die Uhr wird nur neu gezeichnet, wenn sich die Sekunde geändert hat.
- Beim Start ist `lastTouchMs = 0`: ohne Bedienung schaltet das Display 300 s nach dem Boot ab.

---

## 5. UI-Zustandsautomat

Seiten 0 bis 7 entsprechen den Zuständen UI_CLOCK(0), UI_ALARM1(1), UI_ALARM2(2), UI_SOUND1(3), UI_SOUND2(4), UI_FUNCS(5), UI_CUCKOO_TIME(6), UI_INFO(7).

Globale Events:

| Event | Wirkung                                                                              |
| ----- | ------------------------------------------------------------------------------------ |
| T0    | Seite zyklisch weiter: 0 → 1 → 2 → 3 → 4 → 5 → 6 → 0. **Auf Seite 7 ohne Wirkung.**  |
| S3    | Info-Seite ein/aus: von jeder Seite ≠ 7 nach 7, von 7 nach 0. Aktionssperre 1000 ms. |

Seitenspezifische Events (T2/T3/T4):

| Seite | T2                       | T3                                      | T4                          |
| ----- | ------------------------ | --------------------------------------- | --------------------------- |
| 0     | –                        | Lautstärke + (bis max. 25)              | Lautstärke – (bis min. 0)   |
| 1     | Alarm 1 Ein/Aus          | Stunde + (23 → 0)                       | Minute + (59 → 0)           |
| 2     | Alarm 2 Ein/Aus          | Stunde + (23 → 0)                       | Minute + (59 → 0)           |
| 3     | Vorschau Sound 1 Ein/Aus | Datei + (nach mp3Count → 1)             | Datei – (nach 1 → mp3Count) |
| 4     | Vorschau Sound 2 Ein/Aus | Datei +                                 | Datei –                     |
| 5     | Kuckuck Ein/Aus          | Licht Ein/Aus                           | Mühlrad Ein/Aus             |
| 6     | –                        | von-Stunde + (23 → 0)                   | bis-Stunde + (23 → 0)       |
| 7     | –                        | **WLAN-Zugangsdaten löschen**, Neustart | **Werksreset**, Neustart    |

Details:

- **Lautstärke (Seite 0):** `vol` wird geändert, an den DFPlayer gesendet (`volume`), `str_vol` neu gezeichnet. Gelingt der Zugriff auf den Player nicht (Player belegt, Timeout 50 ms), bleibt `vol` unverändert und es wird ins Log geschrieben: `[FEHLER] Lautstaerke+ fehlgeschlagen (Mutex belegt), vol unveraendert bei <n>` bzw. `Lautstaerke-`.
- **Alarmzeit (Seite 1/2):** T3/T4 ändern die Zeit sofort in der Anzeige und merken, dass die Zeit editiert wurde. Alarm-Logik siehe 6.1.
- **Sound-Seiten (Seite 3/4):** T3/T4 ändern die **gewählte** Nummer (`soundN_selected`), schalten die Vorschau aus (`soundN_on = false`, Player stoppen falls BUSY) und speichern. T2 schaltet die Vorschau um (6.8).
- **Funktionen (Seite 5):** Umschalten der Flags `cuckoo_on`, `light_on`, `wheel_on`, Checkbox neu, speichern.
- **Info (Seite 7):** nur T3 und T4 wirken. Beide Aktionen werden erst nach Freigabe des Display-Zugriffs ausgeführt (Meldung anzeigen, warten, Neustart, Abschnitt 4.5).
- Bleibt der Zustand gleich, wird nur partiell neu gezeichnet. Beim Seitenwechsel läuft die Seitenzeichnung (Abschnitt 4.3). Beim Verlassen der Alarm-Seiten gilt die Sperrlogik aus 6.1.

---

## 6. Funktionen

### 6.1 Wecker (Alarm 1 und Alarm 2)

Zwei unabhängige Alarme, je mit Ein/Aus, Stunde, Minute und zugewiesenem Sound (Datei in Ordner 01). **Alarm 1 hat Vorrang**: in einem Takt wird höchstens ein Alarm ausgelöst, Alarm 1 wird zuerst geprüft. **Gleiche Weckzeit (22v02):** Sind beide Alarme aktiv und haben dieselbe Stunde und Minute (Default: beide 06:00), sperrt der erfolgreiche Start des einen Alarms den anderen für heute (Tages-Sperre setzen und persistieren). Er startet dann weder im selben noch in einem späteren Takt (nach Ende oder S1-Stopp des ersten). Alarme mit unterschiedlicher Weckzeit bleiben unabhängig; ein während eines laufenden Alarms fällig gewordener Zweitalarm wird nach dessen Ende im Nachholfenster weiterhin nachgeholt.

Prüftakt **500 ms**. Ohne gültige Uhrzeit (Systemzeit ≤ 1700000000, d. h. noch kein NTP-Sync) wird weder Alarm noch Kuckuck ausgewertet.

**Fälligkeit** (`alarmDue`), pegelbasiert statt Flanke:

```
alarmDue(on, h, m, hour, min, yday, lastDay, isdstNow):
  wenn nicht on oder yday == lastDay:           nicht fällig      (Tages-Sperre)
  effH = h
  wenn heuteFrühjahrsumstellung und h == 2:     effH = 1          (Stunde 02 existiert heute nicht)
  wenn heuteHerbstumstellung und h == 2 und isdstNow == 1:  nicht fällig   (erster Durchlauf der doppelten Stunde)
  diff = (hour*60 + min) - (effH*60 + m)
  fällig wenn 0 <= diff <= 60                                     (Nachholfenster ALARM_CATCHUP_MIN = 60 min)
```

Zeitumstellungstage werden einmal pro Kalendertag **zeitzonen-generisch** erkannt: für den heutigen Tag mit `mktime()` die Stunden 01:00 und 04:00 mit `tm_isdst = -1` auflösen und `tm_isdst` vergleichen: `isdst(01:00)=0` und `isdst(04:00)=1` → Frühjahrsumstellung; `isdst(01:00)=1` und `isdst(04:00)=0` → Herbstumstellung.

Sinn: Der Wecker sichert einen Termin. Weckzeit 02:xx wird im Frühjahr um eine Stunde vorgezogen (01:xx) und im Herbst erst beim **zweiten** Durchlauf (Normalzeit) ausgelöst. Alle anderen Stunden bleiben unberührt.

**Auslösen** (`triggerAlarm`):

1. Lautstärke senden: `max(vol, 10)` (Mindestlautstärke `ALARM_MIN_VOL = 10`; `vol` selbst bleibt unverändert).
2. DFPlayer-Befehl `playFolder(1, datei)`.
3. Start verifizieren: bis zu 3 Versuche im Abstand von 500 ms. Erfolg, wenn Status „spielt“ (readState > 0) **oder** BUSY-Pin LOW.
   - Ergebnis „Modul lebt, Datei startet aber nicht“ (Status 0 nach 3 Versuchen): Alarm läuft **stumm** weiter (Licht/Motor), Log `[FEHLER] ... Datei <n> startet aber nicht (st=0)`.
   - Ergebnis „keine Antwort“ (Status −1 nach 3 Versuchen): Alarm in einem RTC-Merker ablegen, Neustart (`esp_restart`), nach dem Neustart wird derselbe Alarm erneut ausgelöst. Nach 3 Fehlversuchen kein weiterer Neustart: Der Alarm läuft **stumm** weiter (Licht/Motor, wie „Datei startet nicht“), zusätzlich wird als akustischer Ersatz der **Kuckuck 3×** ausgelöst (`ALARM_NODFP_CUCKOO_COUNT`, je 7500 ms E1 HIGH, 2000 ms Pause `ALARM_NODFP_CUCKOO_GAP_MS`; unabhängig von `cuckoo_on` und Zeitfenster). Tages-Sperre wird gesetzt. Log `[FEHLER] <Alarm>: DFPlayer antwortet nach 3 Versuchen weiterhin nicht (Datei <n>) – Alarm laeuft stumm weiter (Licht, Muehlrad, Kuckuck 3x), <Zeitstempel>`. (22v00; bis 21v13 wurde hier ohne Tages-Sperre abgebrochen.)
   - Hat der Nutzer während der Verifikation mit S1 gestoppt: Abbruch ohne Neustart, Tages-Sperre trotzdem setzen.
4. Nach bestätigtem Start: Tages-Sperre für diesen Alarm auf **heute** setzen (und persistieren), Zeitstempel „letzter erfolgreicher Alarm“ für das Web-Log merken, **Licht** (E3 HIGH, falls `light_on`), danach **Motor** (`motorStart`, falls `wheel_on`), Display aufwecken, Zustand ALARM_RUNNING.

**Laufender Alarm (ALARM_RUNNING):** alle 5 s (`ALARM_POLL_MS`) Player-Status abfragen. Ende, wenn der Player „gestoppt“ meldet (Status 0) **oder** bei fehlender Antwort der BUSY-Pin dreimal im Abstand von 5 ms HIGH liest, **oder** nach **15 min** (`ALARM_MAX_RUN_MS`, Log `[FEHLER] Alarm nach <ms> ms ohne Endebestaetigung abgebrochen`). Beim stummen Alarm (Datei startet nicht oder DFPlayer ohne Antwort, Merker `alarmSilentFallback`) endet der Alarm nicht über den Player-Status, sondern nur nach 15 min oder per S1; S1 beendet auch den Ersatz-Kuckuck. Bei Ende: Motor aus, Licht aus (falls `light_on`). Die Tages-Sperre bleibt bestehen.

**Tages-Sperre** (`lastA1Day`, `lastA2Day` = Tag im Jahr 0..365 oder 0xFFFF = frei): Derselbe Alarm löst am selben Kalendertag nicht erneut aus. Sie wird im NVS gespeichert und übersteht Neustart/Stromausfall.

**Weckzeit verstellen:**

- Solange auf Seite 1 (bzw. 2) die Weckzeit per T3/T4 **editiert** wurde, wird dieser Alarm nicht ausgewertet (Zwischenwerte beim Durchscrollen dürfen nicht auslösen).
- Beim **Verlassen** der Alarm-Seite (T0, S3 oder Auto-Rückkehr), **wenn** die Zeit tatsächlich per T3/T4 geändert wurde: Tages-Sperre aufheben (0xFFFF), danach sofort wieder auf heute setzen, falls die neue Weckzeit heute bereits verstrichen ist (innerhalb 60 min, also kein Nachholen), und persistieren. Ein bloßer Seitenbesuch ohne Änderung lässt die Sperre bestehen.
- Wird ein Alarm per T2 **eingeschaltet** und ist seine Weckzeit heute schon verstrichen (innerhalb 60 min), wird er nicht nachgeholt (Tages-Sperre sofort setzen).

### 6.2 Kuckuck (E1)

Ausgang E1 HIGH für **7500 ms** (`CUCKOO_DURATION_MS`), dann LOW.

Automatisch zur **vollen Stunde** (Minute 0, Sekunde 0 im 500-ms-Takt), wenn alle Bedingungen gelten:

- `cuckoo_on` aktiv und Uhrzeit gültig,
- Stunde im Fenster `cuckoo_onTime` bis `cuckoo_offTime` (einschließlich beider Grenzen); ist `onTime > offTime`, gilt das Fenster über Mitternacht: `hour >= onTime` **oder** `hour <= offTime`,
- **kein** aktiver Alarm auf dieser vollen Stunde (`(a1_on und a1_min == 0 und hour == a1_hour)` oder dasselbe für Alarm 2),
- nicht bereits ausgelöst in dieser Stunde dieses Tages (Sperre auf **Tag + Stunde**: schützt auch vor einem zweiten Kuckuck bei der doppelten Herbststunde 02:00).

Manuell: S1 ohne laufenden Alarm löst den Kuckuck einmalig aus (unabhängig von Fenster und Checkbox) und setzt dieselbe Tag+Stunde-Sperre.

### 6.3 Mühlrad-Motor (E2) und Licht (E3)

**Motor** (LEDC 20 kHz, 8 Bit, Sollwert `motor_duty` 0..255, Default 153 = 60 %):

- `motorStart()`: nur wenn `wheel_on` **und** `motor_duty > 0`, sonst PWM 0. Ist `motor_duty < 89` (Schwelle `MOTOR_PWM_KICK_THRESHOLD`, ca. 35 %), zuerst **150 ms Vollgas** (Duty 255), dann Sollwert (Kickstart).
- `motorStop()`: Duty 0.
- Der Sollwert ist zur Laufzeit über das Web-Log verstellbar (Abschnitt 10) und wird gespeichert; läuft der Motor, wirkt er sofort.

**Licht:** E3 HIGH/LOW (digital). Wird bei Alarm und über S2 nur eingeschaltet, wenn `light_on` aktiv ist.

**Einschaltreihenfolge:** immer erst Licht, dann Motor.

### 6.4 Lautstärke

`vol` 0..25 (`MAX_VOL = 25`), Default 9. DFPlayer-Lautstärke 0..30, hier auf 25 begrenzt. Der Alarm nutzt mindestens 10 (6.1). Gespeichert wird `vol`. Die Player-Lautstärke wird beim Boot auf `startup_vol` gesetzt (Startton), vor jedem Alarm auf `max(vol, 10)` und vor jeder Sound-Vorschau auf `vol`.

### 6.5 Taster S1 – Stopp / Kuckuck

Aktionssperre 1000 ms. Ablauf:

1. Player-Status abfragen (bis 200 ms wiederholen; bleibt die Antwort aus, entscheidet der BUSY-Pin: LOW = spielt, HIGH = Idle).
2. Läuft ein Alarm (ALARM_RUNNING) **oder** spielt der Player: Stopp. Abbruch-Merker für eine laufende Start-Verifikation setzen, `stop` an den DFPlayer, **Motor aus, E3 aus, Zustand ALARM_IDLE**. Die Tages-Sperre bleibt bestehen. Seit 22v03 zusätzlich: laufende Sound-Vorschau beenden (`sound1_on = sound2_on = false`); ist Seite 3 oder 4 sichtbar, die Checkbox leer zeichnen (nur die Box, kein weiteres `stop`). Endet ein Track von selbst, bleibt die Checkbox angehakt (bekannte Einschränkung, zwei T2-Drücke zum erneuten Abspielen).
3. Sonst: Kuckuck einmalig auslösen (6.2).

### 6.6 Taster S2 – Zugschalter Licht + Mühlrad

Aktionssperre 1000 ms, in den ersten 3000 ms nach Task-Start ignoriert (3.2). S2 schaltet einen Merker `S2_SW` um:

- EIN: Zeitstempel merken; E3 HIGH falls `light_on`; dann `motorStart()` (respektiert `wheel_on`).
- AUS: `motorStop()`, E3 LOW.
- **Automatisch AUS nach 30 min** (`S2_TIMEOUT_MS = 1800000`) ohne erneuten Tastendruck. Die Checkbox-Einstellungen bleiben unverändert.

S2 ist unabhängig von Alarm und Displayzustand.

### 6.7 Taster S3 – Info-Seite

Siehe Abschnitt 5. S3 wirkt auch bei abgeschaltetem Display (weckt und öffnet die Info-Seite).

### 6.8 Sound-Vorschau (Seiten 3 und 4)

- **T2 (an):** `soundN_on = true`, Checkbox mit Tick zeichnen und `display()`, **dann** `soundN_assigned = soundN_selected` setzen, die Player-Lautstärke auf `vol` setzen (`volume(vol)`, seit 22v01 – der Player steht sonst nach Boot auf `startup_vol`, nach einem Alarm auf `max(vol, 10)`) und `playFolder(1, soundN_assigned)` senden. Anschließend `str_sN_play` mit der gewählten Nummer zeichnen (Tabelle 4.4). Nach 1500 ms wird die Wiedergabe verifiziert (Status > 0 oder BUSY LOW; sonst nur Log-Hinweis).
- **T2 (aus):** `soundN_on = false`, leere Box zeichnen. Läuft der Player (BUSY LOW), `stop` senden. Schlägt das wegen belegtem Player fehl, wird der Stopp außerhalb des Display-Zugriffs nachgeholt (Timeout 300 ms).
- **T3/T4:** Nummer ändern, Vorschau **aus** (siehe oben).
- **Wichtig:** Der dem Alarm zugewiesene Sound (`soundN_assigned`) ändert sich **nur**, wenn die Vorschau gestartet wird (T2 an). Nur T3/T4 ohne Vorschau ändert die Zuweisung nicht (Abweichung zur alten Anleitung, Abschnitt 13).
- Seite 5 betreten, Seite 3/4 betreten oder Menü-Navigation stoppt eine laufende Vorschau. Betrifft dies einen bereits hörbar laufenden Alarm, gilt es als Abbruch des Alarms (kein Neustart).
- Nummernbereich: 1..`mp3Count`. Ist `mp3Count` unbekannt (0), bleibt die Auswahl bei 1.

---

## 7. Persistenz (NVS)

Namespace **`varSafe`** (Schreibzugriff über eine Hintergrund-Task):

| Schlüssel         | Typ    | Default | Bedeutung / Wertebereich                                        |
| ----------------- | ------ | ------- | --------------------------------------------------------------- |
| `state`           | bool   | false   | Erststart-Flag (nach erstem Boot true)                          |
| `a1_on`           | bool   | true    | Alarm 1 aktiv                                                   |
| `a1_hour`         | int    | 6       | 0..23                                                           |
| `a1_min`          | int    | 0       | 0..59                                                           |
| `a2_on`           | bool   | true    | Alarm 2 aktiv                                                   |
| `a2_hour`         | int    | 6       | 0..23                                                           |
| `a2_min`          | int    | 0       | 0..59                                                           |
| `sound1_assigned` | int    | 1       | Datei Alarm 1, ≥ 1, bei Boot auf 1 zurücksetzen wenn > mp3Count |
| `sound2_assigned` | int    | 1       | Datei Alarm 2                                                   |
| `cuckoo_on`       | bool   | false   | Kuckuck aktiv                                                   |
| `cuckoo_on_h`     | int    | 6       | von-Stunde 0..23                                                |
| `cuckoo_off_h`    | int    | 22      | bis-Stunde 0..23                                                |
| `light_on`        | bool   | true    | Licht bei Alarm / S2                                            |
| `wheel_on`        | bool   | false   | Mühlrad bei Alarm / S2                                          |
| `motor_duty`      | int    | 153     | 0..255                                                          |
| `startup_vol`     | int    | 5       | Startton-Lautstärke 1..25                                       |
| `vol`             | int    | 9       | 0..25                                                           |
| `lastA1Day`       | int    | 0xFFFF  | Tages-Sperre Alarm 1 (Tag im Jahr, 0xFFFF = frei)               |
| `lastA2Day`       | int    | 0xFFFF  | Tages-Sperre Alarm 2                                            |
| `resetCount`      | uint32 | 0       | Neustart-Zähler (wird bei jedem regulären Boot erhöht)          |

Namespace **`wifiCfg`**: `valid` (bool), `ssid` (String ≤ 32), `psk` (String ≤ 63).

Regeln:

- Beim Lesen alle Werte auf den Wertebereich begrenzen (korrupte Daten abfangen). `lastAxDay` > 365 und ≠ 0xFFFF → 0xFFFF.
- **Verzögertes Schreiben:** Jede persistierbare Änderung setzt einen Zeitstempel. Erst wenn **2000 ms** (`NVR_COMMIT_DELAY_MS`) keine weitere Änderung erfolgte, wird gespeichert (Flash-Schonung bei Touch-Repeat). Schlägt das Öffnen des Namespace fehl: Log `[FEHLER] NVR: ...` und erneut versuchen.
- **Boot:** Erststart: Defaults als Baseline schreiben. `resetCount` wird erst **nach** dem Laden der WLAN-Zugangsdaten erhöht, damit der Konfigurator-Boot nach einem Werksreset nicht mitzählt (nach Einrichtung zeigt der Zähler `0001`).
- **WLAN löschen** (Info-Seite T3): nur `wifiCfg/valid = false`. **Werksreset** (T4): gesamte NVS-Partition löschen und neu initialisieren (inkl. WLAN-Daten und Zähler).

---

## 8. Boot-Ablauf

1. **Als Allererstes** E1, E3 auf LOW und E2 (LEDC) auf Duty 0 setzen (Ausgänge sicher aus).
2. GPIO17 (DFPlayer RX) kurz auf LOW treiben, **3000 ms** warten.
3. UART2 (9600, 8N1, RX=16, TX=17) starten, 2000 ms warten.
4. Log-Puffer anlegen, Reset-Ursache loggen: `[RESET] Ursache: <Text>` (Texte: `Power-on`, `externer Reset (EN-Pin)`, `Software-Reset (ESP.restart())`, `Panic/Exception`, `Interrupt-Watchdog`, `Task-Watchdog`, `sonstiger Watchdog`, `Brownout (Unterspannung)`, `SDIO-Reset`, `Aufwachen aus Deep-Sleep`, `unbekannt`).
5. NVS laden (Abschnitt 7).
6. Display initialisieren (gedreht), Start-Bildschirm (Abschnitt 4.5).
7. WLAN-Daten laden. **Keine gültigen Daten:** Konfigurator starten (Abschnitt 9.2), kehrt nicht zurück.
8. `resetCount` erhöhen, Log `[WiFi] Credentials geladen: SSID=<ssid>`.
9. SNTP-Callback registrieren, `pool.ntp.org`, Zeitzone setzen.
10. WLAN verbinden (STA), Wartebildschirm. Timeout 30 s: Meldung „kein WLAN“ 2 s, **Betrieb ohne WLAN/NTP** (Alarm und Kuckuck bleiben gesperrt, bis die Uhr gültig ist).
11. Bei Verbindung: Serial-Ausgabe `WiFi connected – IP: <ip>  Log: http://<ip>:8080`, Log `[WiFi] connected`, `[WiFi] IP: <ip>`, NTP-Wartebildschirm, Log `[NTP] warte auf Synchronisation ...`. Warten bis Jahr ≥ 2001 (Timeout 30 s, dann `[NTP] Timeout – Uhr nicht gestellt` und „NTP Timeout“ 2 s). Erfolg: `[NTP] Synchronisation OK` und Zeitstempel „letzter Reset“ merken.
12. GPIO-Eingänge S1, S2, S3 (Pull-up), BUSY (Eingang) konfigurieren.
13. Queue, Mutexe anlegen. Interrupts der Taster aktivieren.
14. **DFPlayer:** Bildschirm „Sound ...“. Initialisieren (mit Reset und Quittungsmodus). Gelingt das nicht: Log `[DFPlayer] Verbindung fehlgeschlagen!`. Sonst Log `[DFPlayer] Serial2 OK`; Dateizahl Ordner 01 abfragen (bis 5 s wiederholen, bei Timeout `mp3Count = 0` und Log `[FEHLER] DFPlayer-Dateizahl unbekannt (Timeout) – Sound-Auswahl bleibt gesperrt`); `volume(startup_vol)`; EQ **Bass**; **Startton** `playFolder(2, 1)`; **4000 ms** warten.
15. Ist `mp3Count > 0`: Zuweisungen > mp3Count auf 1 setzen. Sonst Log `[FEHLER] DFPlayer-Dateizahl unbekannt – Sound-Zuordnung unveraendert`. `str_mp3`, `str_reset` füllen, Log `[DFPlayer] mp3Count: <n>`.
16. Offenen Alarm-Neustart-Merker auswerten: wurde vor dem Neustart ein Alarm wegen DFPlayer-Absturz abgebrochen, wird er jetzt erneut ausgelöst (Log `[DFPlayer] Nach Neustart: <Alarm> wird erneut ausgelöst (Datei <n>, Versuch <k>/3)`).
17. Textpuffer füllen (`sound*_selected = sound*_assigned`), Seite 0 zeichnen.
18. Watchdog konfigurieren (Hardware-TWDT 15 s, Log `[TWDT] Hardware Watchdog aktiv (15000 ms)`).
19. Event-Queue leeren, S2-Ignorierfenster (3000 ms) starten, alle Tasks starten.

---

## 9. WLAN, NTP und Konfigurator

### 9.1 Normalbetrieb

- WLAN-Station mit den gespeicherten Zugangsdaten. Fällt die Verbindung aus, versucht eine Hintergrund-Task alle **3 s** erneut zu verbinden (Zeitstempel des Reconnect-Versuchs für das Web-Log merken, Format siehe 10.3).
- NTP-Server `pool.ntp.org`, Zeitzone `CET-1CEST,M3.5.0/02,M10.5.0/03`. Bei jedem erfolgreichen Sync Zeitstempel für das Web-Log merken (Format `yyyymmdd` und `hh:mm:ss` der Ortszeit, Abschnitt 10.3).
- Web-Log-Server (Port 8080) startet, sobald das WLAN verbunden ist.

### 9.2 Konfigurator (Access Point)

Start beim ersten Boot (keine gültigen Daten), nach Werksreset und nach WLAN-Reset (Info-Seite T3). Blockiert den weiteren Ablauf bis zum Speichern.

- Offener Access Point, SSID **`bTn-Wecker`**, Kanal 1, kein Passwort, IP **192.168.4.1**.
- Webserver auf **Port 80**. OLED zeigt den Konfigurator-Bildschirm (Abschnitt 4.5).
- `GET /` → Konfigurationsseite (Anhang A, `WIFI_CONFIG_PAGE`).
- `POST /save` (Felder `ssid`, `psk`): SSID wird getrimmt, das Passwort **nicht**. Prüfung:
  - SSID 1..32 Zeichen, sonst `SSID ungueltig: 1-32 Zeichen erforderlich.`
  - Passwort leer (offenes Netz) oder 8..63 Zeichen, sonst `Passwort ungueltig: Leer lassen oder 8-63 Zeichen.` bzw. `Passwort zu lang (max. 63 Zeichen).`
  - Fehler: Antwort ist eine Seite mit Meta-Refresh auf `/?err=<Text>` (Leerzeichen als `%20`); die Konfigurationsseite zeigt den Text im roten Fehlerfeld (Anhang A, `wifiErrorRedirect`).
  - Erfolg: SSID/Passwort im NVS speichern, `valid = true`, Erfolgsseite senden (Anhang A, `WIFI_SUCCESS_PAGE`), **3500 ms** warten, OLED „Gespeichert! / Neustart ...“, 500 ms, Neustart.
- Jeder andere Pfad antwortet mit `302` nach `/`.

---

## 10. Web-Log (HTTP Port 8080)

Erreichbar nach WLAN-Verbindung unter `http://<IP>:8080`. Die Adresse steht auf der Info-Seite (Seite 7) und einmalig auf der seriellen Konsole.

### 10.1 Endpunkte

| Endpunkt                    | Antwort                                                                                                                                                                                      |
| --------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `GET /`                     | HTML-Seite (10.2), `text/html; charset=UTF-8`, Auto-Refresh 20 s                                                                                                                             |
| `GET /log`                  | Ring-Puffer als Plain-Text, eine Zeile je Eintrag, `text/plain; charset=UTF-8`                                                                                                               |
| `GET /motor?duty=<0..100>`  | Pulsweite in Prozent setzen: `duty8 = (pct*255 + 50)/100`, begrenzen 0..100, live übernehmen falls Motor läuft, speichern, Log `[MOTOR] Duty -> <pct> % (<d>/255)`, Antwort **303** nach `/` |
| `GET /startvol?vol=<1..25>` | Startton-Lautstärke setzen, begrenzen auf 1..25, speichern, Log `[STARTVOL] Startton-Lautstaerke -> <v>`, Antwort **303** nach `/`                                                           |

### 10.2 Aufbau der Seite `/` (Reihenfolge)

HTML-Kopf mit `<meta http-equiv='refresh' content='20'>` und `<title>bTn Wecker Log</title>`.

CSS (unverändert übernehmen):

```css
body{font-family:monospace;background:#1a1a2e;color:#e0e0e0;margin:0;padding:16px}
h2{color:#BDD7EE;margin-bottom:8px;font-size:1.6rem}
h3{color:#888;font-weight:normal;font-size:1rem;margin:0 0 12px}
.snap-wrap{margin-bottom:16px}
.snap-title{font-size:1rem;color:#78909c;margin-bottom:4px}
.snap-title .ts{color:#4A9EFF;font-weight:bold}
.snap-box{background:#0d0d1a;border:1px solid #333;border-radius:6px;padding:10px;white-space:pre;overflow-x:auto;font-size:19px;color:#e0e0e0}
#log,#dflog{background:#0d0d1a;border:1px solid #333;border-radius:6px;padding:12px;white-space:pre;overflow-x:auto;max-height:60vh;overflow-y:auto;font-size:19px}
#log,#dflog{margin-bottom:16px}
.ok{color:#6BCB77}.err{color:#FF6B6B}.warn{color:#FFD93D}
.sec-title{font-size:1rem;color:#78909c;margin:16px 0 4px}
```

Inhalt in dieser Reihenfolge:

1. `<h2>` = Uhr-Emoji (`&#x1F553;`) + ` bTn Wecker <Version> &ndash; Web-Log`
2. `<h3>` = `IP: <ip>:8080 &nbsp;|&nbsp; Auto-Refresh: 20 s &nbsp;|&nbsp; Aktualisiert: <span id='upd'></span>` (Uhrzeit per Skript am Seitenende: `document.getElementById('upd').textContent=new Date().toLocaleTimeString();`)
3. **Allgemeines Log**: Titel `Allgemeines Log – letzter Reset: <Zeitstempel>` (Zeitstempel in `#4A9EFF`, Platzhalter `–`), Box `#log` mit allen Log-Zeilen, die **nicht** DFPlayer-Zeilen sind.
4. **Verbindung**: Titel `Verbindung – letzter WiFi Reconnect / NTP Sync`, Box `.snap-box` mit zwei Zeilen: `  WiFi  <yyyymmdd>  <hh:mm:ss>` und `  NTP   <yyyymmdd>  <hh:mm:ss>` (fehlende Werte: `–`).
5. **Touch Baseline**: Titel `Touch Baseline – letzte Kalibrierung: <Zeitstempel>`, Box mit vier Zeilen im Format `  Pad <i>  Baseline: <n>  Threshold: <n>` (i = 0..3 für T0, T2, T3, T4).
6. **Stack High-Water Marks**: Titel `Stack High-Water Marks – letzte Messung: <Zeitstempel>`, Box mit einer Zeile je Task (`  <name>: <Bytes frei>`, Namen auf 12 Zeichen aufgefüllt) und letzter Zeile `  Freier Heap : <n> Bytes`. Aktualisierung alle 60 s. Task-Namen sind frei, die Struktur bleibt.
7. **DFPlayer**: Titel `DFPlayer – letzter erfolgreicher Alarm: <Zeitstempel>`, Box `#dflog` mit allen Log-Zeilen, die „DFPlayer“ enthalten. Direkt unter der `mp3Count`-Zeile steht die Zeile `[DFPlayer] Heartbeat: <dd.mm.yyyy hh:mm:ss>` (Platzhalter `–`; steht die mp3Count-Zeile nicht mehr im Puffer, kommt die Heartbeat-Zeile an den Anfang).
8. **Mühlrad-Motor – Pulsweite (Drehzahl)** (Titel mit `&uuml;`): Formular (GET auf `/motor`, Klasse `snap-box`): `Duty: <output id='dv'>NN</output> %` + Schieberegler 0..100 (Name `duty`, Breite 55 %, `oninput='dv.value=this.value'`) + Schaltfläche `Setzen` + Hinweiszeile (`#78909c`, 0,85 rem) `< 35 % → Kickstart-Anlaufimpuls · Wert wird in NVS gespeichert`. Anzeigewert `NN = (motor_duty*100 + 127)/255`.
9. **Startton – Lautstärke**: Formular (GET auf `/startvol`): `Lautstärke: <output id='sv'>N</output> / 25` + Schieberegler (Name `vol`, min 1, max 25) + `Setzen` + Hinweis `Wirkt beim nächsten Boot · Wert wird in NVS gespeichert`.

### 10.3 Log-Puffer und Darstellung

- Ring-Puffer mit **40 Zeilen** à höchstens 127 Zeichen. Ältester Eintrag wird überschrieben. Zugriff thread-sicher.
- Zeitstempel allgemein: `dd.mm.yyyy hh:mm:ss` (Ortszeit); ist die Uhr noch ungültig, stattdessen `+<Sekunden seit Boot>s`.
- Jede Zeile beginnt mit einem Tag in eckigen Klammern. Tags werden auf **12 Zeichen** mit Leerzeichen aufgefüllt (`[WiFi]       connected`).
- Farbe je Zeile (erste zutreffende Regel):
  - rot (`.err`): enthält `[WATCHDOG]`, `[PANIC]`, `[FEHLER]` oder `failed`
  - grün (`.ok`): enthält `OK`, `ready` oder `connected`
  - gelb (`.warn`): enthält `Timeout` oder `Warnung`
  - sonst Standardfarbe
- `<` und `>` im Text werden maskiert (`&lt;`, `&gt;`).
- Zuordnung: Zeilen mit „DFPlayer“ gehören in den DFPlayer-Abschnitt, alle anderen in das Allgemeine Log. Die Zeile `[RESET] Ursache: …` wird im Allgemeinen Log **direkt hinter** die Zeile `[RESET] resetCount: <n>` einsortiert, unabhängig vom Zeitpunkt.
- `[RESET] resetCount: <n>` wird geschrieben, sobald der Web-Log-Server gestartet ist.
- Wichtige Meldungen (sinngemäß übernehmen, Tag und Wortlaut möglichst gleich): `[RESET]`, `[WiFi]`, `[NTP]`, `[DFPlayer]`, `[TWDT]`, `[WATCHDOG]`, `[FEHLER]`, `[WARNUNG]`, `[MOTOR]`, `[STARTVOL]` mit den in diesem Dokument genannten Texten.

---

## 11. DFPlayer Mini: Protokoll und Robustheit

### 11.1 Rahmen

UART 9600 Baud, 8N1. Befehlsrahmen (10 Byte):

```
7E FF 06 <CMD> <FB> <P1> <P2> <CHK_H> <CHK_L> EF
```

`FB` = 1, wenn eine Quittung (ACK) verlangt wird, sonst 0. Prüfsumme: `chk = 0 - (FF + 06 + CMD + FB + P1 + P2)` als 16 Bit, `CHK_H` = oberes, `CHK_L` = unteres Byte. Das Original arbeitet mit **aktivierter Quittung** (ACK-Frame `0x41`) und wartet vor jedem neuen Befehl auf die Quittung des vorherigen.

| Befehl                       | CMD  | P1 / P2        | Verwendung                                           |
| ---------------------------- | ---- | -------------- | ---------------------------------------------------- |
| Reset                        | 0x0C | 0 / 0          | Initialisierung beim Boot                            |
| Lautstärke                   | 0x06 | 0 / 0..30      | `vol`, `startup_vol`, Alarmlautstärke                |
| Equalizer                    | 0x07 | 0 / 5 (Bass)   | einmal beim Boot                                     |
| Ordner/Datei abspielen       | 0x0F | Ordner / Datei | `playFolder(1, n)` Alarm/Vorschau, `(2, 1)` Startton |
| Stopp                        | 0x16 | 0 / 0          | Alarm stoppen, Vorschau stoppen                      |
| Status abfragen              | 0x42 | 0 / 0          | Antwort: 0 = gestoppt, 1 = spielt, 2 = Pause         |
| Dateizahl in Ordner abfragen | 0x4E | 0 / Ordner (1) | `mp3Count` beim Boot                                 |

Antwortrahmen haben dasselbe Format (`7E FF 06 <CMD> 00 <P1> <P2> ... EF`). Beim Boot meldet das Modul nach dem Reset Karte/Initialisierung (0x3F). Der Reset dauert; erst nach Bereitschaft (Dateizahl-Abfrage antwortet) sind weitere Befehle sicher.

### 11.2 Zugriffsregeln

- Alle Player-Zugriffe laufen **seriell unter einem Mutex** (Befehl und Antwortlesen gehören zusammen).
- **Niemals** ein Delay/`vTaskDelay` unter gehaltenem Mutex. Die 1-ms-Pause zwischen zwei Statusabfragen liegt außerhalb des Mutex.
- **Vor jedem Befehl** den UART-Empfangspuffer leeren (verspätete Antworten, Restbytes), aber die Bibliothekslogik nicht mitten im Rahmen abreißen: Rahmen vollständig lesen und verwerfen, höchstens 200 Byte (Schutz vor Dauerrauschen bei defekter Leitung). Meldet das Modul einen Fehlerrahmen (0x40), den Fehlercode loggen.
- Statusabfrage (`readState`): nach der Antwort bis zu **100 ms** auf den echten Antwortrahmen warten, auch wenn zuvor ein unaufgeforderter Rahmen (z. B. „Wiedergabe beendet“ 0x3D) kam. Es zählt der zuletzt empfangene Antwortwert. Liefert die Abfrage −1 (Timeout), ist das bei diesem Chip nicht selten; der BUSY-Pin klärt dann.
- **BUSY-Pin:** `busy = (digitalRead == LOW)`. Idle wird entprellt: dreimal im Abstand von 5 ms HIGH.
- Eine Alarmdatei, die nicht existiert, darf nicht auswählbar sein (Begrenzung auf `mp3Count`). Zusätzliche Dateien außerhalb von Ordner 01 beeinflussen die Zählung nicht (ordnerbezogene Zählung).

### 11.3 Heartbeat (Leerlaufüberwachung, empfohlen)

Im Alarm-Task, nur wenn kein Alarm läuft, kein Start-Versuch und keine offene Vorschau-Verifikation:

- Erster Heartbeat 60 s nach Boot, danach alle 5 min. Pro Heartbeat bis zu **3 Abfragen** im Abstand von 400 ms (außerhalb Mutex).
- OK, wenn eine Abfrage antwortet (Status ≠ −1) und nicht gilt „BUSY LOW und Status ≤ 0“. Bei OK: Zeitstempel für die Web-Log-Zeile `[DFPlayer] Heartbeat: …` merken.
- Fehlgeschlagen (alle Abfragen schlecht): Log `[FEHLER] DFPlayer-Heartbeat <n>/3 fehlgeschlagen (st=<s>, BUSY=<LOW/HIGH>)`, Wiederholung nach 30 s. Nach **3** Fehlschlägen in Folge: `esp_restart()` (DFPlayer wird beim Boot neu initialisiert); Zähler im RTC-Speicher, höchstens **3** Heartbeat-Neustarts in Folge (Zähler wird beim ersten erfolgreichen Heartbeat zurückgesetzt). Danach Log `[FEHLER] DFPlayer antwortet auch nach <n> Heartbeat-Neustarts nicht – keine weiteren Neustarts, <Zeitstempel>`.

### 11.4 Weitere Absicherungen (aus der Original-Firmware, empfohlen)

- **Watchdog:** Hardware-TWDT 15 s auf Eingabe-, Anzeige- und Alarm-Task. Zusätzlich Anwendungs-Watchdog: Prüfung jede Sekunde, Task gilt als eingefroren nach 10 s ohne Lebenszeichen (Start der Prüfung erst nach 10 s). Reaktion: Log `[WATCHDOG] Task-Freeze erkannt!` (mit Zeile je betroffenem Task `  <task>: <ms> ms ohne Lebenszeichen`), Display-Meldung (4.5), 2 s warten, Neustart. Fror die Alarm-Task mitten im Alarmstart, den Alarm für den Neustart vormerken.
- **Alarm-Neustart-Merker** im RTC-Speicher (überlebt Software-Reset, **nicht** EN-Reset/Neuflashen): wird **beim Alarmstart gesetzt** und erst nach bestätigter Wiedergabe oder regulärem Ende gelöscht.
- **Ausgänge E1 bis E3** müssen so früh wie möglich nach dem Reset definiert abgeschaltet sein (Abschnitt 8, Schritt 1).

---

## 12. Konstanten

| Name                      | Wert                             | Bedeutung                                             |
| ------------------------- | -------------------------------- | ----------------------------------------------------- |
| FW_VERSION                | `NvNN`                           | Versionsstring (Display, Web-Log, Konfiguratorseite)  |
| WIFI_AP_SSID              | `bTn-Wecker`                     | SSID des Konfigurator-Access-Points                   |
| WIFI_AP_CHANNEL           | 1                                | Kanal des Access-Points                               |
| MY_NTP_SERVER             | `pool.ntp.org`                   | NTP-Server                                            |
| MY_TZ                     | `CET-1CEST,M3.5.0/02,M10.5.0/03` | Zeitzone (POSIX)                                      |
| WEBLOG_PORT               | 8080                             | Port des Web-Logs                                     |
| WEBLOG_LINES              | 40                               | Zeilen im Ring-Puffer                                 |
| WEBLOG_LINE_LEN           | 128                              | maximale Zeilenlänge (inkl. Terminator)               |
| WEBLOG_TAG_WIDTH          | 12                               | Tag-Spaltenbreite im Allgemeinen Log                  |
| TOUCH_DROP                | 150                              | Mindest-Absenkung gegenüber der Baseline              |
| TOUCH_POLL_MS             | 50                               | Touch-Abtastrate                                      |
| TOUCH_HOLD_MS             | 750                              | Haltezeit bis Repeat                                  |
| TOUCH_REPEAT_MS           | 250                              | Wiederholrate                                         |
| TOUCH_RECAL_MS            | 600000                           | Baseline-Neumessung (nur im Leerlauf)                 |
| TOUCH_MAX_HOLD_MS         | 30000                            | Zwangsreset bei klemmendem Pad                        |
| BTN_DEBOUNCE_MS           | 30                               | ISR-Entprellung                                       |
| BTN_LOCKOUT_MS            | 1000                             | Aktionssperre je Taster                               |
| S2_BOOT_IGNORE_MS         | 3000                             | S2 nach Task-Start ignorieren                         |
| S2_TIMEOUT_MS             | 1800000                          | S2-Zugschalter automatisch AUS nach 30 min            |
| DISPLAY_UPDATE_MS         | 300                              | Takt der Anzeige-Task                                 |
| AUTO_RETURN_MS            | 20000                            | Rückkehr zu Seite 0                                   |
| DISPLAY_TIMEOUT_MS        | 300000                           | OLED aus nach 5 min                                   |
| CUCKOO_DURATION_MS        | 7500                             | Kuckuck-Laufzeit                                      |
| ALARM_POLL_MS             | 5000                             | Statusabfrage im laufenden Alarm                      |
| ALARM_MAX_RUN_MS          | 900000                           | harte Obergrenze Alarmdauer (15 min)                  |
| ALARM_CATCHUP_MIN         | 60                               | Nachholfenster in Minuten                             |
| ALARM_MIN_VOL             | 10                               | Mindestlautstärke des Alarms                          |
| VERIFY_PLAY_DELAY_MS      | 500                              | Wartezeit je Start-Prüfung                            |
| VERIFY_PLAY_RETRIES       | 3                                | Anzahl der Start-Prüfungen                            |
| ALARM_MAX_RESTARTS        | 3                                | max. Neustarts je Alarm bei DFPlayer-Absturz          |
| ALARM_NODFP_CUCKOO_COUNT  | 3                                | Ersatz-Kuckuck-Auslösungen bei totem DFPlayer (22v00) |
| ALARM_NODFP_CUCKOO_GAP_MS | 2000                             | Pause zwischen Ersatz-Kuckuck-Auslösungen (22v00)     |
| DFP_PREVIEW_VERIFY_MS     | 1500                             | Prüfung der Vorschau                                  |
| DFP_HB_FIRST_MS           | 60000                            | erster Heartbeat nach Boot                            |
| DFP_HB_INTERVAL_MS        | 300000                           | Heartbeat-Intervall                                   |
| DFP_HB_RETRY_MS           | 30000                            | Wiederholung nach Fehlschlag                          |
| DFP_HB_PROBES             | 3                                | Abfragen je Heartbeat                                 |
| DFP_HB_PROBE_GAP_MS       | 400                              | Pause zwischen den Abfragen                           |
| DFP_HB_FAILS_MAX          | 3                                | Fehlschläge bis Neustart                              |
| DFP_HB_MAX_RESTARTS       | 3                                | max. Heartbeat-Neustarts in Folge                     |
| SETUP_WIFI_TIMEOUT_MS     | 30000                            | Wartezeit auf WLAN beim Boot                          |
| SETUP_NTP_TIMEOUT_MS      | 30000                            | Wartezeit auf erste NTP-Zeit                          |
| SETUP_MP3_TIMEOUT_MS      | 5000                             | Wartezeit auf Dateizahl                               |
| WIFI_RECONNECT_MS         | 3000                             | Reconnect-Takt                                        |
| NVR_COMMIT_DELAY_MS       | 2000                             | Ruhezeit vor dem Speichern                            |
| STACK_MON_INTERVAL_MS     | 60000                            | Aktualisierung der Stack-Anzeige                      |
| WDG_TIMEOUT_MS            | 10000                            | Anwendungs-Watchdog                                   |
| WDG_CHECK_MS              | 1000                             | Prüfintervall                                         |
| WDT_HARDWARE_MS           | 15000                            | Hardware-TWDT                                         |
| MAX_VOL                   | 25                               | höchste einstellbare Lautstärke                       |
| STARTUP_VOL               | 5                                | Default Startton-Lautstärke                           |
| STARTUP_VOL_MIN           | 1                                | Mindestwert Startton-Lautstärke                       |
| MOTOR_PWM_FREQ            | 20000                            | PWM-Frequenz in Hz                                    |
| MOTOR_PWM_RES             | 8                                | Auflösung in Bit                                      |
| MOTOR_PWM_DUTY            | 153                              | Default-Sollwert (60 %)                               |
| MOTOR_PWM_KICK_THRESHOLD  | 89                               | unter diesem Sollwert Kickstart                       |
| MOTOR_PWM_KICK_DUTY       | 255                              | Duty des Kickstart-Impulses                           |
| MOTOR_PWM_KICK_MS         | 150                              | Dauer des Kickstarts                                  |
| I2C                       | 0x3C                             | OLED-Adresse                                          |

---

## 13. Abweichungen und Eigenheiten

Die alte Bedienungsanleitung (Firmware 21v11) weicht in einigen Punkten vom Code (22v03) ab. **Es gilt das Verhalten des Codes**, wie in diesem Dokument beschrieben:

| Thema                | Alte Anleitung                                                | Tatsächliches Verhalten (maßgeblich)                                                                                                     |
| -------------------- | ------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------- |
| Sound-Vorschau T3/T4 | „neuer Sound spielt sofort als Vorschau, wenn Vorschau aktiv“ | T3/T4 schalten die Vorschau **aus**; Abspielen nur über T2                                                                               |
| Sound speichern      | „gespeichert beim Verlassen der Seite“                        | Zuweisung (`soundN_assigned`) wird beim **Starten der Vorschau** (T2 an) gesetzt                                                         |
| Weckzeit verstellen  | Sperre wird beim Verlassen aufgehoben                         | Zusätzlich: Alarm ist während des Editierens nicht scharf; Sperre wird persistiert und ggf. sofort neu gesetzt (21v13)                   |
| Versionsstand        | 21v11                                                         | 22v03                                                                                                                                    |
| Seiten 2 und 4       | nicht beschrieben                                             | Teilaktualisierung der Vorgängerseite: Checkbox der Zeile A1 verschwindet, nur die Box des gewählten Alarms/Sounds wird gezeichnet (4.3) |
| Seite 1 und 3        | nicht beschrieben                                             | Es wird nur **eine** Checkbox gezeichnet (Alarm 1 bzw. Sound 1)                                                                          |

Weitere Eigenheiten, die bewusst so sind:

- Das **Datum** auf Seite 0 wird nur beim Seitenaufbau (und Mitternacht) aktualisiert, nicht jede Sekunde.
- Beim Alarmstart erscheint **keine eigene Alarmanzeige**: das Display zeigt die aktuelle Seite (wird bei Bedarf aufgeweckt).
- T0 auf der Info-Seite hat keine Funktion. Die Info-Seite wird nur durch S3 oder die 20-s-Auto-Rückkehr verlassen.
- Touch-Events bei abgeschaltetem Display wecken nur und werden verworfen (Schutz vor blindem Auslösen von WLAN-Reset/Werksreset).
- Die Lautstärke-Tasten fallen weg, wenn der Player beim Tastendruck gerade belegt ist (Log-Zeile, Anzeige und Wert bleiben konsistent). Kein Fehlverhalten.
- Es gibt keine weiteren Bedienelemente, keine Sekunden-Einstellung, keine Datums- oder Zeitzonenwahl am Gerät. Die Zeit kommt ausschließlich per NTP.
- Der Wecker hat **keine Pufferbatterie/RTC**: ohne WLAN/NTP bleibt die Uhr ungültig (Anzeige zeigt 1970), und Alarm und Kuckuck sind gesperrt.

---

## 14. Abnahme-Checkliste

Die neue Firmware gilt als gleichwertig, wenn Folgendes am echten Gerät erfüllt ist:

1. **Boot:** Reihenfolge der Bildschirme wie in 4.5, Startton in der Lautstärke `startup_vol`, danach Seite 0. Ausgänge E1 bis E3 sind beim Einschalten aus.
2. **Seite 0:** Alle Texte an den Koordinaten aus 4.3, Uhr aktualisiert sich jede Sekunde ohne Flackern, Lautstärke ± mit T3/T4 inklusive Halten (Repeat nach 750 ms, dann alle 250 ms).
3. **Seiten 1 bis 7:** Jede Seite pixelgleich zur Tabelle (Fotovergleich mit Original empfohlen), T0-Zyklus, S3-Info, Auto-Rückkehr nach 20 s, Display aus nach 5 min, Wecken ohne Auslösen der Aktion.
4. **Alarm:** Auslösung zur Weckzeit, Licht dann Motor, S1 stoppt alles, Tages-Sperre, Nachholfenster (Test: Gerät kurz vor Weckzeit aus-/einschalten), Weckzeit verstellen löst beim Durchscrollen nicht aus, Zeitumstellung 02:xx (Simulation per Zeitvorgabe).
5. **Kuckuck:** automatisch zur vollen Stunde im Fenster (inkl. Mitternachtsüberlauf), 7,5 s, unterdrückt bei Alarm auf dieser Stunde, S1 manuell.
6. **S2:** Licht dann Mühlrad, Aus nach erneutem Druck und nach 30 min, in den ersten 3 s nach dem Boot wirkungslos.
7. **Sound-Vorschau:** T2 spielt, T2 stoppt, T3/T4 stoppen, Seitenwechsel stoppt, Alarm verwendet die zuletzt vorgespielte Datei.
8. **Persistenz:** Alle Einstellungen überstehen Neustart und Stromausfall, Speichern erst 2 s nach der letzten Änderung. WLAN-Reset und Werksreset wie beschrieben, `resetCount` korrekt.
9. **WLAN:** Konfigurator-AP, Validierungsmeldungen, Neustart, Reconnect alle 3 s.
10. **Web-Log:** `/`, `/log`, `/motor`, `/startvol` wie 10.1, Farben und Abschnitte wie 10.2/10.3, Auto-Refresh 20 s.
11. **Robustheit:** DFPlayer-Heartbeat, Watchdog, Reset-Ursache im Log.

---

## Anhang A: HTML des WLAN-Konfigurators (unverändert aus `WEB.h`)

Die drei Teile werden vom Konfigurator-Server ausgeliefert. In `WIFI_CONFIG_PAGE` steht an der Stelle `<<FW_VERSION>>` die Versionsnummer (Footer `bTn_Wecker_<Version> • 192.168.4.1`).

### A.1 `WIFI_CONFIG_PAGE` (GET /)

```html
<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>bTn Wecker – WiFi Einrichtung</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0}
  body{font-family:Arial,sans-serif;background:#1a1a2e;color:#e0e0e0;
       min-height:100vh;display:flex;align-items:center;justify-content:center;padding:16px}
  .card{background:#16213e;border-radius:12px;padding:28px 24px;
        width:100%;max-width:380px;box-shadow:0 8px 32px rgba(0,0,0,.4)}
  h1{font-size:1.3rem;color:#4fc3f7;text-align:center;margin-bottom:4px}
  .sub{font-size:.8rem;color:#78909c;text-align:center;margin-bottom:24px}
  label{font-size:.85rem;color:#b0bec5;display:block;margin-bottom:4px;margin-top:14px}
  input[type=text],input[type=password]{
    width:100%;padding:10px 12px;border-radius:7px;border:1.5px solid #37474f;
    background:#0f3460;color:#e0e0e0;font-size:1rem;outline:none;transition:border .2s}
  input:focus{border-color:#4fc3f7}
  .pw-wrap{position:relative}
  .pw-wrap input{padding-right:44px}
  .eye{position:absolute;right:10px;top:50%;transform:translateY(-50%);
       background:none;border:none;color:#78909c;cursor:pointer;font-size:1.1rem;
       padding:4px;line-height:1}
  .eye:hover{color:#4fc3f7}
  .hint{font-size:.75rem;color:#546e7a;margin-top:3px}
  .err{background:#b71c1c;color:#fff;border-radius:7px;padding:10px 14px;
       font-size:.85rem;margin-top:16px;display:none}
  button[type=submit]{
    width:100%;margin-top:22px;padding:12px;border-radius:7px;
    background:linear-gradient(135deg,#0077b6,#4fc3f7);
    color:#fff;font-size:1rem;font-weight:bold;border:none;
    cursor:pointer;transition:opacity .2s}
  button[type=submit]:hover{opacity:.88}
  .footer{font-size:.72rem;color:#37474f;text-align:center;margin-top:18px}
</style>
</head>
<body>
<div class="card">
  <h1>&#x1F4F6; bTn Wecker</h1>
  <div class="sub">WiFi-Einrichtung</div>

  <form id="frm" method="POST" action="/save" onsubmit="return validate()">
    <label for="ssid">WLAN-Name (SSID)</label>
    <input type="text" id="ssid" name="ssid" maxlength="32"
           placeholder="Ihr WLAN-Name" autocomplete="off" autocorrect="off"
           autocapitalize="none" spellcheck="false" required>
    <div class="hint">1 – 32 Zeichen</div>

    <label for="psk">Passwort</label>
    <div class="pw-wrap">
      <input type="password" id="psk" name="psk" maxlength="63"
             placeholder="Leer = offenes Netz" autocomplete="new-password">
      <button type="button" class="eye" onclick="togglePw()" title="Passwort anzeigen">&#x1F441;</button>
    </div>
    <div class="hint">Leer lassen f&uuml;r offene Netzwerke &bull; sonst 8 – 63 Zeichen</div>

    <div class="err" id="errBox"></div>

    <button type="submit">Speichern &amp; Neu starten</button>
  </form>

  <div class="footer">bTn_Wecker_<<FW_VERSION>> &nbsp;&bull;&nbsp; 192.168.4.1</div>
</div>

<script>
function togglePw(){
  var f=document.getElementById('psk');
  f.type=(f.type==='password')?'text':'password';
}
function showErr(msg){
  var b=document.getElementById('errBox');
  b.textContent=msg; b.style.display='block';
  b.scrollIntoView({behavior:'smooth',block:'nearest'});
}
function validate(){
  var ssid=document.getElementById('ssid').value.trim();
  var psk =document.getElementById('psk').value;
  if(ssid.length<1||ssid.length>32){
    showErr('SSID ung\u00fcltig: 1\u201332 Zeichen erforderlich.'); return false;
  }
  if(psk.length>0&&psk.length<8){
    showErr('Passwort ung\u00fcltig: Leer lassen oder 8\u201363 Zeichen.'); return false;
  }
  if(psk.length>63){
    showErr('Passwort zu lang (max. 63 Zeichen).'); return false;
  }
  document.getElementById('errBox').style.display='none';
  return true;
}
// URL-Parameter ?err= als Fehlermeldung anzeigen
(function(){
  var p=new URLSearchParams(window.location.search);
  if(p.get('err')) showErr(decodeURIComponent(p.get('err')));
})();
</script>
</body>
</html>
```

### A.2 `WIFI_SUCCESS_PAGE` (Antwort auf erfolgreiches POST /save)

```html
<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>bTn Wecker – Gespeichert</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0}
  body{font-family:Arial,sans-serif;background:#1a1a2e;color:#e0e0e0;
       min-height:100vh;display:flex;align-items:center;justify-content:center;padding:16px}
  .card{background:#16213e;border-radius:12px;padding:28px 24px;
        width:100%;max-width:380px;text-align:center;box-shadow:0 8px 32px rgba(0,0,0,.4)}
  .icon{font-size:3rem;margin-bottom:12px}
  h1{font-size:1.3rem;color:#66bb6a;margin-bottom:8px}
  p{font-size:.9rem;color:#b0bec5;line-height:1.5}
  .countdown{font-size:2rem;font-weight:bold;color:#4fc3f7;margin:18px 0 4px}
  .sub{font-size:.78rem;color:#546e7a}
</style>
</head>
<body>
<div class="card">
  <div class="icon">&#x2705;</div>
  <h1>Gespeichert!</h1>
  <p>Die WLAN-Zugangsdaten wurden im Flash gespeichert.</p>
  <div class="countdown" id="ct">3</div>
  <div class="sub">Sekunden bis zum Neustart</div>
</div>
<script>
var n=3;
var t=setInterval(function(){
  n--;document.getElementById('ct').textContent=n;
  if(n<=0){clearInterval(t);}
},1000);
</script>
</body>
</html>
```

### A.3 Fehlerantwort (`wifiErrorRedirect`)

Antwort auf ein fehlgeschlagenes `POST /save` (Leerzeichen der Meldung werden als `%20` kodiert, Umlaute bleiben):

```html
<!DOCTYPE html><html><head><meta charset='UTF-8'><meta http-equiv='refresh' content='0;url=/?err=<Meldung>'></head><body></body></html>
```
