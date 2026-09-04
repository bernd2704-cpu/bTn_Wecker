# Stückliste – bTn Wecker Projekt

## 1. Mechanik

| Bauteil                        | Hersteller           | Artikel                        | Lieferant            | Preis      |
| ------------------------------ | -------------------- | ------------------------------ | -------------------- | ---------: |
| Cuckoo Clock                   | ROKR                 | LC901C                         | Amazon               |    59,45 € |
| Getriebemotor 6mm 3V           | Garosa               | B085NG5ZCY 4-fach              | Amazon               |     9,89 € |

## 2. Elektronik

| Bauteil                        | Hersteller           | Artikel                        | Lieferant            | Preis      |
| ------------------------------ | -------------------- | ------------------------------ | -------------------- | ---------: |
| Netzteil                       | Anker                | 323 Charger (33W)              | Amazon               |    11,99 € |
| ESP32 DEV Kit C V4             | AZ-Delivery          | ESP32WROOM32                   | AZ-Delivery          |    10,99 € |
| OLED 0,96"                     | APKLVSR              | SSD1306OLED                    | Amazon               |     3,54 € |
| DFPlayer Mini                  | DFRobot              | DFR0299                        | AZ-Delivery          |     2,10 € |
| Micro SD-Card 32GB             | Lexar                | LMSESXX032G-B2AEU              | Amazon               |    14,00 € |
| Lautsprecher                   | Visatone             | FRWS 4 ND Breitband 8 Ohm      | Voelkner             |    16,89 € |

## 3. Motor-Treiber

Bauteile des Motor-Treibers (Schaltplan `Hardware/Schaltplan/Motor-Treiber.md`,
Ansteuerung GPIO26/E2 per PWM). Der DC-Motor selbst steht unter „1. Mechanik"
(Getriebemotor 6mm 3V) und liegt an der über MCP1700T-3302E/TO auf 3,3V
geregelten Rail (Hardware 2v0, ersetzt die bisherige Direktversorgung mit 5V
ohne Regler). Einkaufsdaten noch offen – Hersteller/Artikel/Lieferant/Preis
als „–".

| Bauteil                                          | Hersteller           | Artikel                        | Lieferant            | Preis      |
| ------------------------------------------------ | -------------------- | ------------------------------ | -------------------- | ---------: |
| LDO Spannungsregler 3,3V                         | Microchip            | MCP1700T-3302E/TO              | –                    |          – |
| MOSFET IRLML6344 (N-Channel Logic-Level, SOT-23) | –                    | –                              | –                    |          – |
| Widerstand 470 Ω (Gate-Reihe)                    | –                    | –                              | –                    |          – |
| Widerstand 10 kΩ (Gate-Pull-Down)                | –                    | –                              | –                    |          – |
| Widerstand 10 Ω (Snubber, mit 10 nF)             | –                    | –                              | –                    |          – |
| Elko 100 µF (Rail-Stützkondensator)              | –                    | –                              | –                    |          – |
| Keramik-Kondensator 100 nF (Entstörung)          | –                    | –                              | –                    |          – |
| Keramik-Kondensator 10 nF (Snubber, mit 10 Ω)    | –                    | –                              | –                    |          – |
| Diode 1N4448 (Freilaufdiode, schnell)            | –                    | –                              | –                    |          – |

## 4. LED-Treiber

Bauteile des LED-Treibers (Schaltplan `Hardware/Schaltplan/LED-Treiber.md`,
Ansteuerung GPIO26/E3, Licht). Einkaufsdaten noch offen –
Hersteller/Artikel/Lieferant/Preis als „–".

| Bauteil                                          | Hersteller           | Artikel                        | Lieferant            | Preis      |
| ------------------------------------------------ | -------------------- | ------------------------------ | -------------------- | ---------: |
| MOSFET IRLML6344 (N-Channel Logic-Level, SOT-23) | –                    | –                              | –                    |          – |
| Widerstand 470 Ω (Gate-Reihe)                    | –                    | –                              | –                    |          – |
| Widerstand 10 kΩ (Gate-Pull-Down)                | –                    | –                              | –                    |          – |
| Widerstand 47 Ω / 0,25W (LED-Vorwiderstand)      | –                    | –                              | –                    |          – |
| LED-Streifen (2,8V / 48mA)                       | –                    | –                              | –                    |          – |

## 5. Kuckuck-Treiber

Bauteile des Kuckuck-Treibers (Schaltplan `Hardware/Schaltplan/Kuckuck-Treiber.md`,
Ansteuerung GPIO27/E1). Das Kuckuck-Modul selbst steht unter „1. Mechanik"
(Cuckoo Clock). Einkaufsdaten noch offen – Hersteller/Artikel/Lieferant/Preis
als „–".

| Bauteil                                          | Hersteller           | Artikel                        | Lieferant            | Preis      |
| ------------------------------------------------ | -------------------- | ------------------------------ | -------------------- | ---------: |
| MOSFET IRLML6344 (N-Channel Logic-Level, SOT-23) | –                    | –                              | –                    |          – |
| Widerstand 470 Ω (Gate-Reihe)                    | –                    | –                              | –                    |          – |
| Widerstand 10 kΩ (Gate-Pull-Down)                | –                    | –                              | –                    |          – |
| Elko 100 µF (Modul-Stützkondensator)             | –                    | –                              | –                    |          – |
| Keramik-Kondensator 100 nF (Entstörung)          | –                    | –                              | –                    |          – |

## 6. Analoguhrwerk-Spannungsregler

Spannungsversorgung für das 1,5V-Analoguhrwerk (Schaltplan U4, KiCad
`Hardware/Schaltplan/KiCad_Wecker_2v0/`). Das Uhrwerk selbst ist Teil des
ROKR-Kits und steht bereits unter „1. Mechanik" (Cuckoo Clock).

| Bauteil                                          | Hersteller           | Artikel                        | Lieferant            | Preis      |
| ------------------------------------------------ | -------------------- | ------------------------------ | -------------------- | ---------: |
| LDO Spannungsregler 1,5V                         | Microchip            | MCP1702-1502TO                 | reichelt             |     0,99 € |
| Keramik-Kondensator 4,7 µF (Ausgang, X7R)        | –                    | –                              | –                    |          – |

## 7. DFPlayer-BUSY-Filter

RC-Filter für das DFPlayer-BUSY-Signal (Schaltplan
`Hardware/Schaltplan/DFPlayer-BUSY.md`, GPIO34, ab Hardware 2v0). Einkaufsdaten
noch offen – Hersteller/Artikel/Lieferant/Preis als „–".

| Bauteil                                  | Hersteller | Artikel | Lieferant | Preis |
| ---------------------------------------- | ---------- | ------- | --------- | ----: |
| Widerstand 1 kΩ (BUSY-Reihenwiderstand)  | –          | –       | –         |     – |
| Kondensator 100 nF (BUSY-Tiefpass)       | –          | –       | –         |     – |
