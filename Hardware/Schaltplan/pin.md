# bTn Wecker – Pinbelegung ESP32 DevKitC V4

Stand: Firmware 21v03 · Hardware 2v0

Draufsicht, USB-Buchse unten. `*` = vom Wecker belegt, übrige Pins `nc` (nicht beschaltet).
PIN = Pin-Nummer am Board (J2 links 1–19 von oben nach unten, J3 rechts 20–38 von unten nach oben).

```
J2 (links)                                J3 (rechts)
                             .-----------.
                             |   ESP32   |
                             |  DevKitC  |
                     3V3 [ 1]|           |[38] GND
                      EN [ 2]|           |[37] GPIO23
                  GPIO36 [ 3]|           |[36] GPIO22
                  GPIO39 [ 4]|           |[35] GPIO1
                  GPIO34 [ 5]|           |[34] GPIO3
  GPIO35 DFPLAYER_BUSY * [ 6]|           |[33] * GPIO21 OLED_SDA
             GPIO32 S2 * [ 7]|           |[32] GND
             GPIO33 S1 * [ 8]|           |[31] * GPIO19 OLED_SCL
             GPIO25 E2 * [ 9]|           |[30] GPIO18
             GPIO26 E3 * [10]|           |[29] GPIO5
             GPIO27 E1 * [11]|           |[28] * GPIO17 TXD2
                  GPIO14 [12]|           |[27] * GPIO16 RXD2
                  GPIO12 [13]|           |[26] * GPIO4 T0
                     GND [14]|           |[25] * GPIO0 S3
             GPIO13 T4 * [15]|           |[24] * GPIO2 T2
                   GPIO9 [16]|           |[23] * GPIO15 T3
                  GPIO10 [17]|           |[22] GPIO8
                  GPIO11 [18]|           |[21] GPIO7
                      5V [19]|           |[20] GPIO6
                             |           |
                             '---[USB]---'
```

## Belegte Pins

| PIN | GPIO | Konstante     | Funktion                                 | Betriebsart          |
| --- | ---- | ------------- | ---------------------------------------- | -------------------- |
| 6   | 35   | DFPLAYER_BUSY | DFPlayer BUSY                            | Eingang (input-only) |
| 7   | 32   | S2            | Taster S2 (Zugschalter Licht + Mühlrad)  | Eingang, Pull-up     |
| 8   | 33   | S1            | Taster S1 (Alarm aus / Kuckuck einmalig) | Eingang, Pull-up     |
| 9   | 25   | E2            | Mühlrad / Motor (MOSFET)                 | PWM 20 kHz, LEDC     |
| 10  | 26   | E3            | LED-Licht (MOSFET)                       | Ausgang digital      |
| 11  | 27   | E1            | Kuckuck (MOSFET)                         | Ausgang digital      |
| 15  | 13   | T4            | Touch T4                                 | Touch                |
| 23  | 15   | T3            | Touch T3                                 | Touch                |
| 24  | 2    | T2            | Touch T2                                 | Touch                |
| 25  | 0    | S3            | Taster S3 (Info-Seite)                   | Eingang, Pull-up     |
| 26  | 4    | T0            | Touch T0                                 | Touch                |
| 27  | 16   | RXD2          | DFPlayer TX                              | UART2 RX             |
| 28  | 17   | TXD2          | DFPlayer RX                              | UART2 TX             |
| 31  | 19   | OLED_SCL      | OLED SCL                                 | I2C                  |
| 33  | 21   | OLED_SDA      | OLED SDA                                 | I2C                  |

## DFPlayer Mini – Pinbelegung

Draufsicht (Bauteilseite oben, microSD-Slot unten), Pin 1 oben links.
`*` = vom Wecker belegt, übrige Pins `nc` (nicht beschaltet).
PIN = Pin-Nummer am Modul (links 1–8 von oben nach unten, rechts 9–16 von unten nach oben).

```
Pin 1–8 (links)            Pin 9–16 (rechts)
              .-----------.
    VCC * [ 1]|           |[16] * BUSY
     RX * [ 2]|           |[15]   USB-
     TX * [ 3]|           |[14]   USB+
    DAC_R [ 4]| DFPlayer  |[13]   ADKEY2
    DAC_L [ 5]|    Mini   |[12]   ADKEY1
  SPK_1 * [ 6]|           |[11]   IO2
    GND * [ 7]|           |[10] * GND
  SPK_2 * [ 8]|           |[ 9]   IO1
              |           |
              '-[microSD]-'
```

| PIN | DFPlayer | Funktion                  | Verbunden mit                      |
| --- | -------- | ------------------------- | ---------------------------------- |
| 1   | VCC      | Versorgung 3,2 – 5,0 V    | 5V                                 |
| 2   | RX       | UART-Eingang              | GPIO17 (TXD2, PIN 28)              |
| 3   | TX       | UART-Ausgang              | GPIO16 (RXD2, PIN 27)              |
| 4   | DAC_R    | Audio-Ausgang rechts      | nc                                 |
| 5   | DAC_L    | Audio-Ausgang links       | nc                                 |
| 6   | SPK_1    | Lautsprecher (+)          | Lautsprecher                       |
| 7   | GND      | Masse                     | GND (Sternpunkt)                   |
| 8   | SPK_2    | Lautsprecher (−)          | Lautsprecher                       |
| 9   | IO1      | Vorheriger Titel / Lauter | nc                                 |
| 10  | GND      | Masse                     | GND (Sternpunkt)                   |
| 11  | IO2      | Nächster Titel / Leiser   | nc                                 |
| 12  | ADKEY1   | AD-Tastenfeld 1           | nc                                 |
| 13  | ADKEY2   | AD-Tastenfeld 2           | nc                                 |
| 14  | USB+     | USB DP                    | nc                                 |
| 15  | USB−     | USB DM                    | nc                                 |
| 16  | BUSY     | Wiedergabe-Status (LOW)   | GPIO35 (DFPLAYER_BUSY, PIN 6)      |

Hinweis: Die Pinnummern entsprechen dem Datenblatt des DFPlayer Mini; Pin 7 und 10 sind beide GND.
Die Zuordnung der Verbindungen folgt der Firmware (`SysConf_21v03.h`) – bitte gegen den Schaltplan
`Wecker_2v0.kicad_sch` prüfen.

Vollständige Liste inkl. nc-Pins, Strapping-Hinweisen und Änderungshistorie:
[Pin_GPIO-Liste.md](Pin_GPIO-Liste.md). Quelle: `SysConf_21v03.h`.
