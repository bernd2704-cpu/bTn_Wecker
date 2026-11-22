# bTn Wecker – Logik-Fehler im ESP-IDF-Export

Prüfung von `wecker_export.md` (Firmware 21v13, Stand 2026-11-22) gegen sich selbst. Grundlage ist nur das Dokument, nicht der Originalcode. Ob etwas ein Doku-Fehler oder echtes Code-Verhalten ist, ist noch zu klären. Zeilenangaben beziehen sich auf die Exportdatei.

## A. Echte Widersprüche

1. **Stummer Alarm endet sofort (Z. 435 vs. 440).** Der stumme Alarm ist definiert als „Modul lebt, Status 0“. Die Ende-Regel im laufenden Alarm lautet aber „Player meldet Status 0 → Ende“. Der stumme Alarm würde beim ersten 5-s-Poll enden, obwohl dort steht, nur der 15-min-Ausstieg sei möglich. Es fehlt ein `silent`-Flag in der Spezifikation.
2. **Interrupts „kurz vor Task-Start“ (Z. 151 vs. 564/565/570).** Die Taster-Interrupts werden in Boot-Schritt 13 aktiviert. Die Tasks starten erst in Schritt 19, nach der DFPlayer-Init (bis 5 s) und dem Startton (4 s). Die Queue-Leerung in Schritt 19 fängt es ab, die Beschreibung stimmt trotzdem nicht.
3. **Seite 6: Beschriftung passt nicht zur Funktion (Z. 294 vs. 392).** Die Anzeige zeigt „Taste +“ bei von und „Taste -“ bei bis. T4 erhöht aber die bis-Stunde (23 → 0) und verringert nichts. Bei einer pixelgleichen Kopie bleibt der Text, die Bedienlogik ist aber irreführend.
4. **„Uhr gültig“ ist doppelt definiert (Z. 412 vs. 562).** Der Alarm prüft `time > 1700000000`, der Boot wartet auf `Jahr ≥ 2001`. Praktisch keine Kollision, aber eine einzige Konstante festlegen.
5. **Log-Farben greifen nicht (Z. 649–651).** `[DFPlayer] Verbindung fehlgeschlagen!` (Z. 565) enthält weder `[FEHLER]` noch `failed`, wird also nicht rot. `[WARNUNG]` (Z. 132) passt nicht auf die Regel `Warnung`, wenn der Vergleich Groß- und Kleinschreibung unterscheidet.
6. **RTC-Merker (Z. 704, 699).** Das Dokument schreibt „überlebt Software-Reset“. Im IDF überlebt `RTC_DATA_ATTR` nur Deep-Sleep. Für `esp_restart()` wird `RTC_NOINIT_ATTR` plus Magic-Wert gebraucht, weil der Inhalt nach Power-on Müll ist. Das betrifft den Alarm-Neustart-Merker und den Heartbeat-Zähler. „Gelöscht nach Wiedergabe oder regulärem Ende“ ist zudem redundant, nach bestätigter Wiedergabe ist der Merker schon weg.

## B. Lücken, die zu Fehlverhalten führen

7. **Tages-Sperre in den Fehlerzweigen undefiniert (Z. 435–438).** Gesetzt wird sie nur „nach bestätigtem Start“ und beim S1-Abbruch. Bei „stumm weiter“ und bei „3 Fehlversuche ausgeschöpft, Alarm abgebrochen“ steht nichts dazu. Ohne Sperre bleibt der Alarm im 60-min-Nachholfenster fällig. Das ergibt eine Retrigger-Schleife im 2-s-Takt, die den 40-Zeilen-Log flutet. **Status: in Firmware 22v00 behoben** (Variante B: stummer Alarm mit Licht/Mühlrad plus Kuckuck 3×, Tages-Sperre gesetzt; Export aktualisiert).
8. **Lautstärke nach dem Boot (Z. 565–568).** Schritt 14 setzt `volume(startup_vol)`, danach folgt kein `volume(vol)`. Der Player bleibt auf Startton-Lautstärke (Default 5), das Display zeigt `vol` (Default 9). Die Vorschau auf den Sound-Seiten spielt zu leise. Dasselbe gilt nach einem Alarm mit `max(vol,10)` (Z. 432): Die Player-Lautstärke wird nie zurückgesetzt. **Status: in Firmware 22v01 behoben** (`volume(vol)` vor jedem Vorschaustart; Export aktualisiert).
9. **Default: beide Alarme aktiv, beide 06:00 (Z. 518–520).** Alarm 2 ist im nächsten Takt ebenfalls fällig. Es ist nicht spezifiziert, ob er während ALARM_RUNNING geprüft wird. Wird er nicht geprüft, startet Alarm 2 sofort, wenn Alarm 1 per S1 gestoppt wird (noch im Nachholfenster). Wird er geprüft, übersteuert er Alarm 1. Gleiches gilt nach Werksreset oder Ersteinrichtung zwischen 06:00 und 07:00. **Status: in Firmware 22v02 behoben für identische Weckzeiten** (Variante B: der Start sperrt den anderen Alarm). Bei unterschiedlichen Weckzeiten bleibt das Nachholen nach Ende/S1-Stopp bewusst bestehen.
10. **S2 und Alarm teilen sich die Ausgänge (Z. 440, 484, 488–493).** `S2_SW` wird bei Alarmende oder S1 nicht zurückgesetzt, E2/E3 aber abgeschaltet. Der nächste S2-Druck schaltet „AUS“ ohne Wirkung, man muss zweimal drücken. Umgekehrt beendet S2 „AUS“ Licht und Motor eines laufenden Alarms.
11. **Datum nur bei Seitenaufbau und 00:00:00–00:00:01 (Z. 338, 788).** Fragil: Kommt NTP erst nach dem 30-s-Timeout, steht auf Seite 0 bis Mitternacht „01.01.1970“. Verpasst die Display-Task das 1-s-Fenster, bleibt das Datum 24 h falsch. Ist das Display nachts aus (Z. 364) und wird nicht gezeichnet, bleibt das Datum nach dem Wecken veraltet. Besser: den Tag pro Tick vergleichen und bei Änderung neu zeichnen.
12. **Alarm weckt das Display (Z. 365, 158, 364).** Es steht nicht da, dass `lastTouchMs` gesetzt wird. Ohne das schaltet sich das Display im nächsten 300-ms-Takt wieder ab.
13. **Zeitumstellung (Z. 421–424, 459).** Das Nachholfenster rechnet in Wanduhr-Minuten statt mit Epoch-Differenz und ist um die Umstellung herum um bis zu 60 min verschoben. Die Kuckuck-Unterdrückung prüft die nominelle `a1_hour`, nicht `effH`. Im Frühjahr liegt Alarm 02:00 effektiv auf 01:00, der Kuckuck löst dort trotzdem aus.
14. **Sound-Vorschau und S1 (Z. 484, 503–504).** S1 stoppt einen spielenden Player, setzt `soundN_on` aber nicht zurück, die Checkbox bleibt angehakt. Dasselbe gilt, wenn der Track von selbst endet. Dann braucht es zwei T2-Drücke zum erneuten Abspielen. **Status: S1-Teil in Firmware 22v03 behoben** (`clearSoundPreviewOnStop()`); das natürliche Trackende wird weiterhin nicht erkannt.
15. **T3/T4 „und speichern“ (Z. 399 vs. 506).** Auf den Sound-Seiten ändert T3/T4 nur `soundN_selected`, das nicht persistiert wird. Es gibt nichts zu speichern.
16. **Touch-Zwangsreset (Z. 132).** Hält jemand das Pad 30 s fest, wird die Baseline mit Finger neu gemessen. Das Pad ist dann bis zur nächsten Rekalibrierung (bis 10 min) praktisch tot. Die Neumessung sollte warten, bis kein Pad mehr berührt ist.

## C. Kleinigkeiten

- **Info-Seite ohne WLAN:** Was in der Zeile `<IP>:8080` steht, ist nicht spezifiziert (Z. 304).
- **Web-Log-Refresh:** Der 20-s-Refresh lädt die Seite neu, ein halb verstellter Schieberegler geht verloren (Z. 606 vs. 640).
- **Boot-Schritt 2:** „kurz auf LOW“ und „3000 ms warten“ widersprechen sich, es ist unklar, ob LOW die ganze Zeit anliegt (Z. 553).
- **T0 bis T4:** Es gibt kein T1, gemeint sind T0/T2/T3/T4 (Z. 158, 365).
- **Wecken per Touch:** Die Begründung „Schutz vor Reset auf der Info-Seite“ (Z. 791) greift ins Leere. Die Auto-Rückkehr nach 20 s liegt immer vor dem Display-Aus nach 300 s.
- **Werksreset:** T4 auf der Info-Seite löst ihn ohne Bestätigung beim ersten Touch-Event aus.

## Stichproben ohne Befund

DFPlayer-Befehlscodes und Prüfsumme, NVS-Schlüssellängen (maximal 15 Zeichen), Rundung bei der Motor-Prozentumrechnung, Kickstart-Schwellwert, Pin- und Touch-Zuordnung.
