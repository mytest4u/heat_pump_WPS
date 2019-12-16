# heat_pump_WPS
Wärmepumpe Ersatzschaltung für Buderus WPS90  / Heat pump replacement circuit for Buderus WPS and identical models. 
Getestet mit: Arduino 1.8.10 / Adafruit SSD1306  Version 1.1.2 / MEGA2560

- Programm für Arduino MEGA 2560 / Program for Arduino MEGA 2560
- Schaltplan / circuit diagram
- Dokumentation / documentation

More can you see in my youtubechannel MyTest4u
https://www.youtube.com/MyTest4u

http://www.mytest4u.de/

Arduino in-/out-puts:     
     A0 Aussen Temperatur  
     A1 Temperatur Sole  
     A2 Tempfühler Hz  
     A3 Tempfühler WW
  
Display:   
    (A5 nano) SLC  
    (A4 nano) SDA

Pumpen ( 1 HIGH  = Aus  / 0 LOW = Ein ):

   D2 Verdichter / Kompresssor   
   D3 M16 Ladepumpe Speicher   
   D4 M11 Sole / Außen Wärmekörbe im Boden   
   D5 M18 WW / Heizung umschalter   
   D6 M13 Heizung
   
Sicherheit:   
   D9 Hochdruck Schalter   (Öffner)  
   D8 Niederdruck Schalter (Öffner)

Tastatur / Eingabe:

     D10 Tastatur T2 [Menue]
    
     D11 Tastatur T3 [Enter]
    
     D12 Tastatur T0 [up +]
    
     D13 Tastatur T1 [down -]

Neu in Version 1.13:
  - M13 Abschaltung
  - Pausezeiten verringert 
  - WW mit Prüfung ob Heizung erforderlich ohne Abschaltsequenz

 Neu in Version 1.12:
 - Sicherheitsmenü
 - Frostschutz für Sole
 - EEPROM Speicher für Sommer/Winter
 - Menüstrucktur angepasst

In Version 1.115 Neue Funktionen:
 - Sondermenue Soletemperaturmessung min / max
 - Pumpentest
 - Einstellen Heizen / Warmwasser / Sommer/Winter
 
In Version 1.112 fehlen noch einige Funktionen:
 - Außentemperatur Anpassung
 - Temperatur Umrechnung
 - Uhrzeit
 - EEPROM Speicher Einstellungen
 - Zusatz Menues

Wichtig: Adafruit SSD1306  Version 1.1.2 verwenden, sonst keine 64 Linen Displayauflösung!


