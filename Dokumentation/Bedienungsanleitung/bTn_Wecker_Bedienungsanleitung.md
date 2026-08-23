# bTn Wecker – Bedienungsanleitung

*Firmware 20v21 · ESP32 / FreeRTOS · Hardware 2v0*

## 1. Übersicht

Der bTn Wecker ist ein ESP32-basierter Wecker mit OLED-Anzeige, MP3-Wiedergabe (DFPlayer Mini), kapazitiven Touch-Feldern, drei Drucktastern und WiFi/NTP-Zeitsynchronisation. Alle Einstellungen werden dauerhaft im internen Flash gespeichert.

| Taste / Element | Funktion                                                                                                         |
| --------------- | ---------------------------------------------------------------------------------------------------------------- |
| Display         | SSD1306 OLED 128×64 Pixel, I²C                                                                                   |
| MP3-Player      | DFPlayer Mini, SD-Karte, Ordner 01 = Alarm-Sounds. Ab Hardware 2v0 zusätzlich mit BUSY-Signal (GPIO34) überwacht |
| Touch-Felder    | T0, T2, T3, T4 – kapazitiv, Hold + Repeat                                                                        |
| Drucktaster     | S1, S2, S3                                                                                                       |
| Ausgänge        | E1 = Kuckuck, E2 = Mühlrad/Motor, E3 = Licht                                                                     |
| WiFi / NTP      | WLAN-Verbindung für automatische Zeitsynchronisation                                                             |
| Web-Log         | Diagnose-Seite im Browser: http://IP:8080, inkl. Mühlrad-Motor-Regler                                            |

## 2. Bedienelemente

| Taste / Element | Funktion                                                                                                                                          |
| --------------- | ------------------------------------------------------------------------------------------------------------------------------------------------- |
| T0  (Touch)     | Menüseite vorwärts: 0 → 1 → 2 → 3 → 4 → 5 → 6 → 0  (zyklisch)                                                                                     |
| T2  (Touch)     | Checkbox Ein/Aus auf der aktuellen Seite<br>auf Seite 3/4: Vorschau-Sound                                                                         |
| T3  (Touch)     | Wert erhöhen: Stunde +, Von-Zeit +, Lautstärke +<br>Hold: Dauerwiederholung                                                                       |
| T4  (Touch)     | Wert erhöhen: Minute +, Bis-Zeit +<br>Hold: Dauerwiederholung  ·  Seite 0: Lautstärke –                                                           |
| S1  (Taster)    | Laufenden Alarm oder Sound stoppen<br>wenn kein Alarm: Kuckuck einmalig auslösen                                                                  |
| S2  (Taster)    | Zugschalter: Licht (E3) und Mühlrad (E2) gleichzeitig Ein/Aus toggeln. Schaltet nach 30 Minuten ohne erneuten Tastendruck automatisch wieder ab   |
| S3  (Taster)    | Info-Seite ein-/ausblenden (von jeder Seite aus erreichbar)<br>Bei ausgeschaltetem Display weckt S3 das Display und öffnet die Info-Seite (11v04) |

**Hold-Funktion:**  Touch-Felder T3 und T4 unterstützen Dauerberührung. Nach 750 ms wechselt das Feld in den Repeat-Modus und sendet alle 250 ms einen weiteren Impuls – nützlich für schnelles Einstellen von Stunden und Minuten. Klemmt ein Touch-Feld dauerhaft (z.B. durch Feuchtigkeit), erzwingt die Firmware nach einigen Sekunden einen Reset des betroffenen Felds, damit z.B. die Lautstärke nicht unbemerkt bis auf 0 heruntergezählt wird.

## 3. Menüstruktur

T0 wechselt die Seiten 0–6 zyklisch. S3 öffnet jederzeit die Info-Seite (Seite 7). Nach 20 Sekunden ohne Eingabe kehrt das Gerät automatisch zu Seite 0 zurück – auch von der Info-Seite. Nach 5 Minuten ohne Touch-Eingabe schaltet das OLED-Display ab; eine Berührung eines Touch-Felds (T0–T4) weckt das Display und verwirft das Event, während S3 das Display weckt und direkt die Info-Seite öffnet (11v04).

| Seite | Name               | Inhalt                                                                                                                                                     |
| ----- | ------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 0     | Uhrzeit            | Zeitanzeige (groß), Datum, beide Alarmzeiten mit Checkbox, aktuelle Lautstärke. T3 = Vol+, T4 = Vol–.                                                      |
| 1     | Alarm 1 einstellen | T2 = Alarm 1 Ein/Aus  ·  T3 = Stunde +  ·  T4 = Minute +  (beide mit Überlauf)                                                                             |
| 2     | Alarm 2 einstellen | T2 = Alarm 2 Ein/Aus  ·  T3 = Stunde +  ·  T4 = Minute +                                                                                                   |
| 3     | Sound 1 wählen     | T2 = Vorschau Ein/Aus (spielt Sound sofort)<br>T3 = Datei +  ·  T4 = Datei –                                                                               |
| 4     | Sound 2 wählen     | T2 = Vorschau Ein/Aus  ·  T3 = Datei +  ·  T4 = Datei –                                                                                                    |
| 5     | Funktionen         | T2 = Kuckuck    Ein/Aus<br>T3 = Licht          Ein/Aus<br>T4 = Mühlrad     Ein/Aus                                                                         |
| 6     | Kuckuck-Zeit       | T3 = Von-Stunde +  ·  T4 = Bis-Stunde +<br>(0–23, Mitternacht-Überlauf möglich)                                                                            |
| 7     | Info  (S3)         | Web-Log-Adresse (IP:8080), MP3-Dateianzahl, Reset-Zähler, Warnhinweise „Taste + WiFi Reset" (T3) und „Taste – Full Reset" (T4). Details siehe Abschnitt 7. |

## 4. Alarmfunktion

Der Wecker löst zur eingestellten Zeit aus, wenn der Alarm aktiviert ist (Checkbox auf Seite 1 oder 2). Die MP3-Datei wird aus Ordner 01 der SD-Karte abgespielt.

| Taste / Element   | Funktion                                                                                                                                                                                    |
| ----------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Vorrang           | Alarm 1 hat Vorrang, wenn beide Alarme auf die gleiche Zeit eingestellt sind                                                                                                                |
| Stoppen           | S1 drücken – stoppt den Sound und alle Ausgänge (E2, E3)                                                                                                                                    |
| Licht bei Alarm   | E3 (Licht) wird beim Alarm-Start eingeschaltet, wenn Checkbox auf Seite 5 aktiv                                                                                                             |
| Mühlrad bei Alarm | E2 (Mühlrad) wird beim Alarm-Start eingeschaltet, wenn Checkbox auf Seite 5 aktiv                                                                                                           |
| Mindestlautstärke | Ist die eingestellte Lautstärke sehr niedrig, spielt der Alarm trotzdem mit einer festen Mindestlautstärke – die gespeicherte Einstellung selbst bleibt dabei unverändert                   |
| Wiederholung      | Derselbe Alarm kann am selben Kalendertag nicht erneut auslösen (Tages-Sperre). Wird die Alarmzeit auf der Einstellseite verändert, wird die Sperre für diesen Tag sofort wieder aufgehoben |
| Nachholfenster    | Verpasst der Wecker den genauen Auslösezeitpunkt (z.B. durch einen kurzen Stromausfall), löst der Alarm innerhalb der folgenden Stunde nachträglich noch aus, statt ersatzlos zu entfallen  |
| Display-Wake      | OLED wird beim Alarm-Start automatisch eingeschaltet, falls es zuvor nach 5 Minuten Ruhezeit abgeschaltet wurde                                                                             |

### Weckzeit bei Zeitumstellung (02:00–02:59 Uhr)

Der Wecker weckt in erster Linie, damit ein nachfolgender Termin pünktlich erreicht wird. Deshalb bleibt bei einer Weckzeit **zwischen 02:00 und 02:59 Uhr** der zeitliche Abstand zwischen Wecken und Termin auch an den beiden Tagen der Zeitumstellung erhalten – der Wecker verhält sich an diesen Tagen bewusst anders als eine gewöhnliche Uhr:

- **Umstellung auf Sommerzeit** (letzter Sonntag im März, die Uhr springt von 01:59:59 direkt auf 03:00:00 – die Zeit 02:00–02:59 Uhr gibt es an diesem Tag gar nicht): Eine Weckzeit in diesem Bereich löst bereits **eine Stunde früher**, also schon zwischen 01:00 und 01:59 Uhr, aus. Ohne diese Vorverlegung würde der Wecker sonst erst um 03:00 Uhr klingeln – bis zu 59 Minuten zu spät.
- **Umstellung auf Winterzeit** (letzter Sonntag im Oktober, die Zeit 02:00–02:59 Uhr kommt an diesem Tag **zweimal** vor): Eine Weckzeit in diesem Bereich löst erst beim **zweiten** Durchlauf dieser Stunde aus, nicht schon beim ersten. Ohne diese Verzögerung würde der Wecker sonst bis zu 60 Minuten zu früh klingeln.

Betroffen sind ausschließlich Weckzeiten mit Stunde 02 – alle anderen Weckzeiten sind von der Zeitumstellung nicht berührt.

## 5. Kuckucksfunktion

Der Kuckuck (E1) löst automatisch zur vollen Stunde aus, wenn die Funktion aktiviert und die aktuelle Stunde im eingestellten Zeitfenster liegt.

| Taste / Element | Funktion                                                                                                                                                                              |
| --------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Aktivieren      | Checkbox auf Seite 5 (T2)                                                                                                                                                             |
| Zeitfenster     | Seite 6: Von-Stunde (T3) und Bis-Stunde (T4). Mitternacht-Überlauf möglich (z.B. 22–06 Uhr)                                                                                           |
| Manuell         | S1 drücken, wenn kein Alarm läuft → Kuckuck einmalig auslösen (unabhängig von Zeitfenster und Checkbox)                                                                               |
| Dauer           | 7,5 Sekunden                                                                                                                                                                          |
| Unterdrückt     | Wenn Alarm 1 oder Alarm 2 auf dieselbe volle Stunde eingestellt ist                                                                                                                   |
| Zeitumstellung  | An beiden Zeitumstellungstagen ist die betroffene Stunde 02:00 Uhr durch eine Tag+Stunden-Sperre geschützt – der Kuckuck löst dort auch bei der doppelten Herbststunde nur einmal aus |

## 6. Sound-Vorschau (Seite 3 / 4)

Auf den Seiten 3 und 4 kann der Alarm-Sound vor dem Speichern angehört werden.

| Taste / Element | Funktion                                                                                      |
| --------------- | --------------------------------------------------------------------------------------------- |
| T2              | Vorschau toggeln: Sound wird sofort abgespielt oder gestoppt                                  |
| T3              | Nächste Dateinummer – der neue Sound wird sofort als Vorschau abgespielt, wenn Vorschau aktiv |
| T4              | Vorherige Dateinummer – analog T3                                                             |
| Speichern       | Die gewählte Nummer wird als Alarm-Sound gespeichert, sobald Seite 3/4 verlassen wird         |

## 7. Info-Seite (S3)

Die Info-Seite zeigt Systemdaten und bietet Zugang zu Konfigurations- und Reset-Funktionen. Seit 11v02 werden die gefährlichen Aktionen explizit als Zeilen auf der Info-Seite benannt – die früher dort zusätzlich angezeigten WiFi-/NTP-Zeitstempel sind in den Web-Log umgezogen (siehe Abschnitt 8). Ab 11v05 wurden die WLAN-Reset-Taste von T0 auf T3 verlegt und die Zeilen auf die einheitliche „+ / –"-Bedienung umgestellt (Taste + = T3, Taste – = T4).

| Zeile                        | Inhalt                                                                        |
| ---------------------------- | ----------------------------------------------------------------------------- |
| 1 – Kopfzeile                | Firmware-Kennung (z.B. `bTn_Wecker_20v21`)                                    |
| 2 – IP:8080                  | Adresse des Web-Log-Servers – im Browser öffnen für Diagnoseinformationen     |
| 3 – MP3 *nnn*   RESET *nnnn* | Anzahl gefundener MP3-Dateien  ·  Neustart-Zähler (4-stellig)                 |
| 4 – Taste +  WiFi Reset      | Hinweis: T3 drücken löscht die WLAN-Zugangsdaten und startet den Konfigurator |
| 5 – Taste –  Full Reset      | Hinweis: T4 drücken löscht alle Einstellungen (NVS-Erase) und startet neu     |

**Sicherheit:**  Ist das Display ausgeschaltet, wecken die Touch-Felder T0–T4 nur das Display – das auslösende Event wird verworfen. So kann ein blind getippter Touch auf der Info-Seite nicht versehentlich T3 (WLAN-Reset, seit 11v05; vorher T0) oder T4 (Werksreset) auslösen. S3 weckt das Display und öffnet die Info-Seite direkt (11v04); die Auto-Rückkehr nach 20 s Inaktivität stellt sicher, dass das Display nur von Seite 0 aus abschaltet, der S3-Aufruf also reproduzierbar zur Info-Seite führt.

## 8. Web-Log (http://IP:8080)

Nach der WiFi-Verbindung ist ein Diagnose-Server erreichbar. Die IP-Adresse wird auf der Info-Seite (S3) angezeigt sowie einmalig auf Serial ausgegeben.

| Endpunkt   | Inhalt                                                                                                              |
| ---------- | ------------------------------------------------------------------------------------------------------------------- |
| GET /      | HTML-Seite mit farbiger Darstellung: grün = OK, rot = Fehler/Freeze, gelb = Warnung. Auto-Refresh alle 20 Sekunden. |
| GET /log   | Plain-Text-Ausgabe aller Log-Einträge – für curl, wget oder Browser-Download                                        |
| GET /motor | Setzt die Mühlrad-Pulsweite (siehe Regler unten) direkt über einen Parameter, z.B. für eigene Skripte               |

**Sektionen (in Anzeigereihenfolge):**

| Sektion                                        | Inhalt                                                                                                                                                                                                                                      |
| ---------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Mühlrad-Motor – Pulsweite (Drehzahl)           | Schieberegler 0–100 %, stellt die Motor-Drehzahl live um und speichert den Wert dauerhaft. Unterhalb von 35 % gibt der Motor beim Start automatisch einen kurzen Vollgas-Anlaufimpuls (Kickstart), da der Motor sonst nicht anläuft (12v03) |
| DFPlayer                                       | Eigener Ring-Puffer-Ausschnitt mit allen DFPlayer-Meldungen (Start-Check, Absturz-Neustart, Serial2-Restbytes). Titel lautet „DFPlayer – letzter erfolgreicher Alarm: *&lt;Zeitstempel&gt;*" (12v08)                                        |
| Allg. Log                                      | Ring-Puffer mit den übrigen Systemereignissen (Start, WiFi/NTP-Events, Watchdog, Alarme). Titel lautet „Allgemeines Log – letzter Reset: *&lt;NTP-Zeitstempel&gt;*" (Zeitpunkt des ersten NTP-Syncs nach dem Reset)                         |
| Verbindung – letzter WiFi Reconnect / NTP Sync | Zeigt die Zeitstempel der letzten WiFi-Verbindung und der letzten NTP-Synchronisation (11v02: umbenannt von „Status – Letzter Start" und unter den Ring-Puffer verschoben)                                                                  |
| Touch-Baseline                                 | Letzter Messwert und Schwellwert aller vier Touch-Pads                                                                                                                                                                                      |
| Stack High-Water Marks                         | Speicherauslastung aller 9 Tasks + freier Heap                                                                                                                                                                                              |

**Tipp:**  Der Web-Log ist nur verfügbar, wenn der Wecker mit dem WLAN verbunden ist. Die Adresse lautet z.B. `http://192.168.1.42:8080`.

**Hinweis „Lautstaerke+/- fehlgeschlagen (Mutex belegt)":**  Während ein Alarm läuft, fragt der Wecker den DFPlayer-Status alle 5 Sekunden kurz ab und belegt dafür kurzzeitig den internen Player-Zugriff. Trifft ein Tastendruck auf T3/T4 (Lautstärke +/–) genau in dieses Fenster, wird dieser eine Druck verworfen und im Log als Fehler vermerkt – Anzeige und Lautstärke bleiben für diesen Druck bewusst unverändert, damit nie ein falscher Wert angezeigt oder gespeichert wird. Bei mehrfachem/gehaltenem Tippen (Touch-Repeat) wirken die übrigen Drücke im selben Moment normal, sodass die Lautstärke trotz einzelner Fehlermeldung sichtbar ansteigt. Kein Fehlverhalten – kein Handlungsbedarf.

## 9. WiFi-Konfiguration

**Erstkonfiguration:**  Beim ersten Start (oder nach Werksreset) öffnet der Wecker automatisch einen WLAN-Accesspoint mit der SSID `bTn-Wecker`. Mit Smartphone oder PC verbinden, Browser öffnen und `192.168.4.1` aufrufen. SSID und Passwort eingeben und speichern – der Wecker startet neu und verbindet sich.

**Neue WLAN-Konfiguration:**  Info-Seite (S3) öffnen → T3 (Taste +) drücken. WLAN-Zugangsdaten werden gelöscht und der Konfigurator startet. *(Bis 11v04 lag diese Funktion auf T0; seit 11v05 einheitlich T3, analog zur „Taste –"-Belegung des Werksresets auf T4.)*

| Feld     | Regel                                                                              |
| -------- | ---------------------------------------------------------------------------------- |
| SSID     | 1–32 Zeichen, Groß-/Kleinschreibung beachten                                       |
| Passwort | Leer lassen für offene Netzwerke. Sonst 8–63 Zeichen.                              |
| Fehler   | Bei falschen Zugangsdaten erscheint eine Fehlermeldung auf der Konfigurationsseite |

## 10. Werksreset

Info-Seite (S3) öffnen → T4 drücken. Alle gespeicherten Einstellungen (Alarmzeiten, Sounds, Lautstärke, Funktionen, Kuckuck-Zeitfenster, Mühlrad-Pulsweite) und WiFi-Zugangsdaten werden unwiderruflich gelöscht. Der Wecker startet neu und öffnet den WiFi-Konfigurator.

**Hinweis:**  Der Reset-Zähler wird ebenfalls zurückgesetzt. Ab 11v03 wird der Konfigurator-Boot nach einem Werksreset nicht mehr mitgezählt – nach erfolgreicher WLAN-Einrichtung zeigt der Zähler `0001` (früher `0002`).

---

*bTn Wecker  ·  Bedienungsanleitung  ·  Firmware 20v21*
