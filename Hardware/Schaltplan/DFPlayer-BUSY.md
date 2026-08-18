# DFPlayer BUSY-Signal

> **Status:** Aktiv ab Hardware 2v0 (verknüpft mit Firmware ab 20v00).
> Der vorherige eingefrorene Stand Hardware 1v0 / Firmware 13v00 hatte
> das BUSY-Signal nicht verdrahtet.

```
DFPlayer Mini                    ESP32 DEV Kit C V4
BUSY (Pin 16) ──────────────────── GPIO34
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

## Funktion

BUSY dient als Statusabfrage des DFPlayer (Wiedergabe läuft ja/nein),
unabhängig von der seriellen Kommunikation über Serial2 (RX=16, TX=17).
