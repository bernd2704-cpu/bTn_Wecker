# Motortreiber

```
5V ──────────────────┬─────────┬────── Motor+ ──────┬────────────┐
                      │         │         │          │    Kathode │
                   [100µF]   [100nF]   [Motor]   [10Ω+10nF]    [1N4148]
                      │         │         │          │      Anode │
                     GND       GND      Motor- ──────┴────────────┘
                                          │
                                          │
                                  Drain ──┘
GPIO26 ─────── [330Ω] ────┬──── Gate             IRLML6344
                          │     Source ─┐
                        [10kΩ]          │
                          │             │
                         GND           GND
```

Motor liegt direkt an 5V (kein Spannungsregler) – per Multimeter verifiziert (2026-08-18).
Freilaufdiode: 1N4148.
