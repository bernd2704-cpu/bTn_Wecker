# bTn Wecker – GPIO-Belegung

Stand: Firmware 21v02 · Hardware 2v0 · ESP32 Dev Kit C V4

Quelle der Belegung: `SysConf_21v02.h` (Pin-Konstanten) und `CLAUDE.md`.
Bei jeder Pin-Änderung diese Liste mitpflegen.

## Belegte Pins

| GPIO | Konstante     | Funktion                                 | Betriebsart          | Anmerkung                                      |
| ---- | ------------- | ---------------------------------------- | -------------------- | ---------------------------------------------- |
| 0    | S3            | Taster S3 (Info-Seite)                   | Eingang, Pull-up     | Strapping-Pin: beim Reset LOW = Download-Modus |
| 2    | T2            | Touch T2                                 | Touch                | Strapping-Pin, beim Booten nicht HIGH treiben  |
| 4    | T0            | Touch T0                                 | Touch                |                                                |
| 13   | T4            | Touch T4                                 | Touch                |                                                |
| 15   | T3            | Touch T3                                 | Touch                | Strapping-Pin (Boot-Log)                       |
| 16   | RXD2          | DFPlayer TX                              | UART2 RX             |                                                |
| 17   | TXD2          | DFPlayer RX                              | UART2 TX             |                                                |
| 19   | OLED_SCL      | OLED SCL                                 | I2C                  | seit 21v02 (vorher GPIO22)                     |
| 21   | OLED_SDA      | OLED SDA                                 | I2C                  |                                                |
| 25   | E2            | Mühlrad / Motor (MOSFET)                 | PWM 20 kHz, LEDC     | seit 20v29 (vorher GPIO26)                     |
| 26   | E3            | LED-Licht (MOSFET)                       | Ausgang digital      | seit 20v29 (vorher GPIO27)                     |
| 27   | E1            | Kuckuck (MOSFET)                         | Ausgang digital      | seit 20v29 (vorher GPIO25)                     |
| 32   | S2            | Taster S2 (Zugschalter Licht + Mühlrad)  | Eingang, Pull-up     | seit 20v28 (vorher GPIO33)                     |
| 33   | S1            | Taster S1 (Alarm aus / Kuckuck einmalig) | Eingang, Pull-up     | seit 20v28 (vorher GPIO32)                     |
| 35   | DFPLAYER_BUSY | DFPlayer BUSY                            | Eingang (input-only) | seit 21v01 (vorher GPIO34), LOW = Wiedergabe   |

## Freie Pins

| GPIO | Status | Anmerkung                                                          |
| ---- | ------ | ------------------------------------------------------------------ |
| 5    | frei   | Strapping-Pin (SDIO-Timing, Boot-Log), nur mit Vorbehalt           |
| 12   | frei   | Strapping-Pin (Flash-Spannung, MTDI): meiden, kann Boot verhindern |
| 14   | frei   | Touch T6, Ausgabe von PWM-Signal beim Boot                         |
| 18   | frei   | unkritisch                                                         |
| 22   | frei   | bis 21v01 OLED SCL                                                 |
| 23   | frei   | unkritisch                                                         |
| 34   | frei   | input-only, keine Pull-ups                                         |
| 36   | frei   | input-only (VP), keine Pull-ups                                    |
| 39   | frei   | input-only (VN), keine Pull-ups                                    |

## Nicht nutzbar

| GPIO | Grund                       |
| ---- | --------------------------- |
| 1    | TX0 (USB-Seriell / Debug)   |
| 3    | RX0 (USB-Seriell / Flashen) |
| 6    | Flash (CLK)                 |
| 7    | Flash (SD0)                 |
| 8    | Flash (SD1)                 |
| 9    | Flash (SD2)                 |
| 10   | Flash (SD3)                 |
| 11   | Flash (CMD)                 |

## Hinweise

- GPIO34, 35, 36 und 39 sind nur Eingänge und haben keine internen Pull-ups.
- Strapping-Pins (0, 2, 5, 12, 15) legen beim Reset den Boot-Modus fest. Externe
  Beschaltung darf den Pegel beim Einschalten nicht gegen den Default ziehen.
- I2C läuft über die GPIO-Matrix, deshalb darf SCL auf GPIO19 liegen.
