# bTn Wecker – GPIO-Belegung

Stand: Firmware 21v02 · Hardware 2v0 · ESP32 Dev Kit C V4

Quelle der Belegung: `SysConf_21v02.h` (Pin-Konstanten) und `CLAUDE.md`.
Bei jeder Pin-Änderung diese Liste mitpflegen.

## Pinbelegung

PIN = Pin-Nummer am ESP32 DevKitC V4 (Draufsicht, USB unten): J2 (linke Leiste) Pin 1–19 von oben nach unten,
J3 (rechte Leiste) Pin 20–38 von unten nach oben. `nc` = nicht beschaltet.

| PIN | GPIO | Konstante     | Funktion                                 | Betriebsart          | Anmerkung                                                          |
| --- | ---- | ------------- | ---------------------------------------- | -------------------- | ------------------------------------------------------------------ |
| 1   | –    | –             | 3V3                                      | Versorgung           |                                                                    |
| 2   | –    | –             | EN                                       | Reset-Eingang        |                                                                    |
| 3   | 36   | –             | nc                                       | –                    | input-only (VP), keine Pull-ups                                    |
| 4   | 39   | –             | nc                                       | –                    | input-only (VN), keine Pull-ups                                    |
| 5   | 34   | –             | nc                                       | –                    | input-only, keine Pull-ups                                         |
| 6   | 35   | DFPLAYER_BUSY | DFPlayer BUSY                            | Eingang (input-only) | seit 21v01 (vorher GPIO34), LOW = Wiedergabe                       |
| 7   | 32   | S2            | Taster S2 (Zugschalter Licht + Mühlrad)  | Eingang, Pull-up     | seit 20v28 (vorher GPIO33)                                         |
| 8   | 33   | S1            | Taster S1 (Alarm aus / Kuckuck einmalig) | Eingang, Pull-up     | seit 20v28 (vorher GPIO32)                                         |
| 9   | 25   | E2            | Mühlrad / Motor (MOSFET)                 | PWM 20 kHz, LEDC     | seit 20v29 (vorher GPIO26)                                         |
| 10  | 26   | E3            | LED-Licht (MOSFET)                       | Ausgang digital      | seit 20v29 (vorher GPIO27)                                         |
| 11  | 27   | E1            | Kuckuck (MOSFET)                         | Ausgang digital      | seit 20v29 (vorher GPIO25)                                         |
| 12  | 14   | –             | nc                                       | –                    | Touch T6, Ausgabe von PWM-Signal beim Boot                         |
| 13  | 12   | –             | nc                                       | –                    | Strapping-Pin (Flash-Spannung, MTDI): meiden, kann Boot verhindern |
| 14  | –    | –             | GND                                      | Masse                |                                                                    |
| 15  | 13   | T4            | Touch T4                                 | Touch                |                                                                    |
| 16  | 9    | –             | nc                                       | –                    | Flash (SD2), nicht nutzbar                                         |
| 17  | 10   | –             | nc                                       | –                    | Flash (SD3), nicht nutzbar                                         |
| 18  | 11   | –             | nc                                       | –                    | Flash (CMD), nicht nutzbar                                         |
| 19  | –    | –             | 5V                                       | Versorgung           |                                                                    |
| 20  | 6    | –             | nc                                       | –                    | Flash (CLK), nicht nutzbar                                         |
| 21  | 7    | –             | nc                                       | –                    | Flash (SD0), nicht nutzbar                                         |
| 22  | 8    | –             | nc                                       | –                    | Flash (SD1), nicht nutzbar                                         |
| 23  | 15   | T3            | Touch T3                                 | Touch                | Strapping-Pin (Boot-Log)                                           |
| 24  | 2    | T2            | Touch T2                                 | Touch                | Strapping-Pin, beim Booten nicht HIGH treiben                      |
| 25  | 0    | S3            | Taster S3 (Info-Seite)                   | Eingang, Pull-up     | Strapping-Pin: beim Reset LOW = Download-Modus                     |
| 26  | 4    | T0            | Touch T0                                 | Touch                |                                                                    |
| 27  | 16   | RXD2          | DFPlayer TX                              | UART2 RX             |                                                                    |
| 28  | 17   | TXD2          | DFPlayer RX                              | UART2 TX             |                                                                    |
| 29  | 5    | –             | nc                                       | –                    | Strapping-Pin (SDIO-Timing, Boot-Log), nur mit Vorbehalt           |
| 30  | 18   | –             | nc                                       | –                    | unkritisch                                                         |
| 31  | 19   | OLED_SCL      | OLED SCL                                 | I2C                  | seit 21v02 (vorher GPIO22)                                         |
| 32  | –    | –             | GND                                      | Masse                |                                                                    |
| 33  | 21   | OLED_SDA      | OLED SDA                                 | I2C                  |                                                                    |
| 34  | 3    | –             | nc                                       | –                    | RX0 (USB-Seriell / Flashen)                                        |
| 35  | 1    | –             | nc                                       | –                    | TX0 (USB-Seriell / Debug)                                          |
| 36  | 22   | –             | nc                                       | –                    | bis 21v01 OLED SCL                                                 |
| 37  | 23   | –             | nc                                       | –                    | unkritisch                                                         |
| 38  | –    | –             | GND                                      | Masse                |                                                                    |

## Hinweise

- GPIO34, 35, 36 und 39 sind nur Eingänge und haben keine internen Pull-ups.
- Strapping-Pins (0, 2, 5, 12, 15) legen beim Reset den Boot-Modus fest. Externe
  Beschaltung darf den Pegel beim Einschalten nicht gegen den Default ziehen.
- I2C läuft über die GPIO-Matrix, deshalb darf SCL auf GPIO19 liegen.
