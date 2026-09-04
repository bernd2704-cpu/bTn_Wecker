# Motortreiber

```
5V ── MCP1700T-3302E/TO ── 3,3V ──┬─────────┬────── Motor+ ──────┬────────────┐
      (LDO, In/Out je 100nF)      │         │         │          │    Kathode │
                                [100µF]   [100nF]   [Motor]   [10Ω+10nF]    [1N4448]
                                   │         │         │          │      Anode │
                                  GND       GND      Motor- ──────┴────────────┘
                                                       │
                                                       │
                                               Drain ──┘
GPIO25 ─────── [470Ω] ────┬──── Gate             IRLML6344
                          │     Source ─┐
                        [10kΩ]          │
                          │             │
                         GND           GND
```

Motor liegt an der über MCP1700T-3302E/TO auf 3,3V geregelten Rail (seit Hardware 2v0,
ersetzt die bisherige Direktversorgung mit 5V ohne Regler). Der PWM-Duty-Bereich
0..255 moduliert damit maximal 3,3V statt vormals 5V – voller Regelumfang ohne
Begrenzung nutzbar, keine Überspannung am 3-V-Motor mehr möglich (auch nicht beim
Kickstart-Vollgasimpuls in `motorStart()`).
Freilaufdiode: 1N4448.
