# DFPlayer BUSY-Signal

> **Status:** Aktiv ab Hardware 2v0 (verknüpft mit Firmware ab 20v00).
> Der vorherige eingefrorene Stand Hardware 1v0 / Firmware 13v00 hatte
> das BUSY-Signal nicht verdrahtet.

```
DFPlayer Mini                                    ESP32 DEV Kit C V4
BUSY (Pin 16) ──── [1kΩ] ────┬──────────────────── GPIO34
                              │
                           [100nF]
                              │
                             GND
```

## Pegel

DFPlayer Mini VCC = 5V. BUSY-Pin bleibt laut Datenblatt trotzdem auf
3,3V-TTL-Pegel (Modul-interne 3,3V-Logikdomain, unabhängig von VCC):

| Parameter | Wert          | Bedingung  |
| --------- | ------------- | ---------- |
| VOL       | max. 0,33 V   | VDD = 3,3V |
| VOH       | min. 2,7 V    | VDD = 3,3V |

Quelle: `Hardware/Stückliste/DFPlayer Mini/DFPlayer Mini Manual.pdf`,
Abschnitt „6. Note*“, sowie AZ-Delivery-Handbuch (bestätigt 3,3V-Logik
auch bei 5V-Versorgung, Serienwiderstand dort nur für RX empfohlen).

Direkter Anschluss an GPIO34 ohne Pegelwandler/Vorwiderstand zulässig:

- VOH min. 2,7V > ESP32 VIH (≈2,45V bei 3,3V-Logik)
- VOL max. 0,33V < ESP32 VIL (≈0,825V bei 3,3V-Logik)

## GPIO34

Input-only (kein internes Pull-up/-down, kein `INPUT_PULLUP` möglich).
Unkritisch, da BUSY aktiv getrieben wird (kein Open-Drain):
LOW = Wiedergabe läuft, HIGH = Pause/Idle.

## RC-Filter (1kΩ + 100nF)

Firmware liest `dfPlayerBusy()` (GPIO34) an mehreren Stellen als einzelnen,
ungefilterten `digitalRead()` ohne Software-Debounce (anders als Touch-Pads
oder Taster). Ein Hardware-RC-Filter schließt diese Lücke:

- **R = 1 kΩ** (Reihe): konsistent mit Touch-Beschaltung und AZ-Delivery-
  Empfehlung für Serial-Leitungen; Spannungsabfall im stationären Zustand
  vernachlässigbar, da GPIO34-Eingangsleckstrom im nA-Bereich liegt.
- **C = 100 nF** (nach GND, GPIO-seitig): τ = R·C = 100 µs,
  Grenzfrequenz f_c ≈ 1,6 kHz – dämpft kurze EMI-Spitzen/Schaltflanken
  (u.a. vom 20kHz-Motor-PWM an E2, siehe `Motor-Treiber.md`) deutlich.

BUSY wechselt nur beim Start/Ende einer Wiedergabe (Sekundenbereich), nicht
taktend. Die Filter-Zeitkonstante ist damit >1000× schneller als das
schnellste Poll-Intervall der Firmware (200 ms im S1-Handling) – keine
spürbare Verzögerung für die Statusabfrage.

Ladestrom beim Umschalten: ΔV/R ≈ 3,3V / 1kΩ ≈ 3,3 mA – unkritisch für den
aktiv getriebenen DFPlayer-Ausgang (kein Open-Drain).

## Funktion

BUSY dient als Statusabfrage des DFPlayer (Wiedergabe läuft ja/nein),
unabhängig von der seriellen Kommunikation über Serial2 (RX=16, TX=17).
Ab Firmware 20v01/20v02 fließt der Status zusätzlich in Alarm-Polling,
Start-Verifikation (`verifyPlayStarted()`) und S1-Handling ein – siehe
`CHANGELOG.md`.
