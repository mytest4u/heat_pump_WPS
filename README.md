# heat_pump_WPS
Wärmepumpe Ersatzschaltung für Buderus WPS90  / Heat pump replacement circuit for Buderus WPS and identical models. 
Testet: Arduino 1.8.10 / Adafruit SSD1306  Version 1.1.2 / MEGA2560

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
  
Display   
    (A5 nano) SLC  
    (A4 nano) SDA

Pumpen ( 1 HIGH  = Aus  / 0 LOW = Ein )
   D2 Verdichter / Kompresssor   
   D3 M16 Ladepumpe Speicher   
   D4 M11 Sole / Außen Wärmekörbe im Boden   
   D5 M18 WW / Heizung umschalter   
   D6 M13 Heizung
   
Sicherheit   
   D9 Hochdruck Schalter   
   D8 Niederdruck Schalter

Tastatur / Eingabe
  D10 Tastatur T2 [Menue]
  D11 Tastatur T3 [Enter]
  D12 Tastatur T0 [up +]
  D13 Tastatur T1 [down -]
   


 Neue Funktionen:
 - Sondermenue Soletemperaturmessung min / max
 - Pumpentest
 - Einstellen Heizen / Warmwasser / Sommer/Winter
 
 In Version 1.112 fehlen noch einige Funktionen:
 - Außentemperatur Anpassung
 - Temperatur Umrechnung
 - Uhrzeit
 - EEPROM Speicher Einstellungen
 - Zusatz Menues
