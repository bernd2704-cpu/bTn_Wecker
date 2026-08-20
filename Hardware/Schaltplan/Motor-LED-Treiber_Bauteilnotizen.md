# bTn Wecker – Hardware-Erweiterung: LED-Streifen & DC-Motor

Bauteilauswahl und Dimensionierung zu den Schaltplänen [Motor-Treiber.md](Motor-Treiber.md)
und [LED-Treiber.md](LED-Treiber.md).

## Versorgung
- Betriebsspannung Lastseite: 5V
- ESP32 GPIO-Pegel: 3,3V

---

## MOSFET: IRLML6344TRPBF (für beide Lasten)
- Logic-Level N-Channel, Vgs(th) = 0,4–1,0V → 3,3V GPIO tauglich
- Vds(max) = 20V, Id(max) = 5A
- Rds(on) bei Vgs=2,5V: ~30mΩ → Verlustleistung vernachlässigbar
- Bauform: SOT-23 (SMD)

### Beschaltung (gilt für beide Kanäle)
- Gate:  330Ω Reihenwiderstand zum GPIO
- Gate:  10kΩ Pull-Down nach GND (verhindert floating beim Boot/Reset)

---

## Kanal 1: LED-Streifen – GPIO27 (E3, Licht)
- Versorgung: 5V, Vf = 2,8V, If = 48mA
- Vorwiderstand: 47Ω / 0,25W in Serie (High-Side, zwischen 5V und LED+)
- Spannungsaufteilung: 2,2V am Widerstand, 2,8V an LED
- Freilaufdiode: nicht erforderlich (ohmsche Last)


---

## Kanal 2: DC-Motor – GPIO26 (E2, Wasserrad)
- Versorgung: 5V, Vnenn = 3V, Istall = 200mA
- Vorwiderstand: keiner (Strom ist lastabhängig → PWM statt Widerstand)
- Freilaufdiode: 1N4148, Kathode zu 5V, Anode zu Drain (schnelle Diode, <4ns)
- Entstörkondensator: 100nF Keramik direkt an den Motoranschlüssen

### PWM
```cpp
#define E2  26
ledcAttach(E2, 20000, 8);   // 20kHz, 8-Bit (über Hörschwelle → kein Surren)
ledcWrite(E2, 153);          // 60% Duty → ~3V Mittelwert
```

### Offene Prüffrage: Kickstart-Überspannung (Stand 2026-08-20)

`motorStart()` (Wecker_20v19.ino:1503-1508) legt bei Sollwert unter
`MOTOR_PWM_KICK_THRESHOLD` für `MOTOR_PWM_KICK_MS` (150 ms) Duty 255
(`MOTOR_PWM_KICK_DUTY`) an – bei 20 kHz PWM bedeutet Duty 255/255 durchgehend
5V, nicht auf die Motor-Nennspannung 3V begrenzt (Faktor ~1,67× Nennspannung).

**Ungeklärt:** Kein Datenblatt für den verbauten Garosa-Getriebemotor 6mm 3V
(Stückliste, ASIN B085NG5ZCY) mit einer zulässigen Kurzzeit-/Spitzenspannung
geprüft – nur `Vnenn=3V, Istall=200mA` liegt vor. Bleibt das Rad beim
Kickstart blockiert (mechanisch verklemmt), steigt der Strom überschlägig
proportional zur Spannung auf ca. 330mA (unkritisch für den MOSFET,
Id max. 5A, aber potenziell relevant für Wicklung/Bürsten bei wiederholter
Auslösung – der Kickstart läuft unbedingt bei jedem Alarmstart, ohne
Rückmeldung ob der Motor tatsächlich angelaufen ist).

**Möglicher Fix, falls sich die Überspannung nicht als unbedenklich
bestätigt:** `MOTOR_PWM_KICK_DUTY` in SysConf von 255 auf einen niedrigeren
Wert (z.B. ~200 ≙ ca. 4V) reduzieren – Kompromiss zwischen Anlaufsicherheit
und Spannungsmarge zur Nennspannung.

---

## Hinweise zur Integration
- GPIOs E2 (26) und E3 (27) sind in SysConf_*.h definiert (siehe `Software/Firmware_aktuell/`)
- Stack-Größen und Task-Zuordnung gemäß bestehender SysConf-Konventionen
- Kein vTaskDelay() unter gehaltenem Mutex
- PWM-Initialisierung in setup() vor Task-Start