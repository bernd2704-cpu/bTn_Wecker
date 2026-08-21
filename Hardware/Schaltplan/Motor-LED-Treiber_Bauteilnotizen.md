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
- Versorgung: 5V über LDO MCP1700T-3302E/TO auf 3,3V geregelt, Vnenn = 3V, Istall = 200mA
- Vorwiderstand: keiner (Strom ist lastabhängig → PWM statt Widerstand)
- Freilaufdiode: 1N4448, Kathode zu 3,3V-Rail, Anode zu Drain (schnelle Diode, <4ns)
- Entstörkondensator: 100nF Keramik direkt an den Motoranschlüssen

### PWM
```cpp
#define E2  26
ledcAttach(E2, 20000, 8);   // 20kHz, 8-Bit (über Hörschwelle → kein Surren)
ledcWrite(E2, 153);          // 60% Duty → ~2V Mittelwert (aus geregelten 3,3V)
```

### Gelöst: Kickstart-Überspannung (Stand 2026-08-21, Hardware-Änderung)

Bisherige Prüffrage (Stand 2026-08-20): `motorStart()` legt bei Sollwert unter
`MOTOR_PWM_KICK_THRESHOLD` für `MOTOR_PWM_KICK_MS` (150 ms) Duty 255
(`MOTOR_PWM_KICK_DUTY`) an. Solange der Motor direkt an 5V lag, bedeutete
Duty 255 durchgehend 5V statt der Nennspannung 3V (Faktor ~1,67×) – ungeklärt,
da kein Datenblatt für den verbauten Garosa-Getriebemotor 6mm 3V (ASIN
B085NG5ZCY) mit zulässiger Kurzzeit-/Spitzenspannung vorlag.

**Fix:** LDO MCP1700T-3302E/TO regelt die Rail vor dem MOSFET jetzt fest auf
3,3V. PWM schaltet diese geregelte Spannung statt der rohen 5V – Duty 255
bedeutet damit nur noch ~3,3V (Faktor ~1,1× Nennspannung), auch beim
Kickstart-Vollgasimpuls. Der volle PWM-Regelumfang 0..255 ist damit ohne
Begrenzung nutzbar, keine gesonderte Kickstart-Deckelung mehr nötig.

---

## Hinweise zur Integration
- GPIOs E2 (26) und E3 (27) sind in SysConf_*.h definiert (siehe `Software/Firmware_aktuell/`)
- Stack-Größen und Task-Zuordnung gemäß bestehender SysConf-Konventionen
- Kein vTaskDelay() unter gehaltenem Mutex
- PWM-Initialisierung in setup() vor Task-Start