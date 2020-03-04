
/*
  V1.14 24.2.2020 Watchdog an Digital pin 30
  V1.13b Zeitverhalten ein / aus verkürzt
  V1.13 neu Tsolewarn 
    Neu: 
  - M13 Abschaltung
  - Pausezeiten verringert 
  - WW mit Prüfung ob Heizung erforderlich ohne Abschaltsequenz
 
  Umgebung:
  Arduino 1.8.10
  Adafruit SSD1306  Version 1.1.2
  MEGA2560

  Steuerung für Wearmepumpe (Buderus WP90)   14.12.2019
  www.mytest4u.de
  YouTube-Kanal: mytest4u

  Testwerte
  WW     455 Ein   440 Aus
  Heizen 640 Ein   620 Aus

  Arduino in-/out-puts:
  A0 Aussen Temperatur
  A1 Temp Sole
  A2 Tempfühler Hz
  A3 Tempfühler WW

  Display
  (A5 nano) SLC
  (A4 nano) SDA

  Pumpen ( 1 HIGH  = Aus  / 0 LOW = Ein )
  D2 Verdichter / Kompresssor
  D3 M16 Ladepumpe Speicher
  D4 M11 Sole / Außen Wärmekörbe im Boden
  D5 M18 WW / Umschalten auf Heizung oder Warmwasser
  D6 M13 Heizung

  Sicherheit
  D9 Hochdruck Schalter
  D8 Niederdruck Schalter

  D10 Tastatur T2 [Menue]
  D11 Tastatur T3 [Enter]
  D12 Tastatur T0 [up +]
  D13 Tastatur T1 [down -]
*/
#include <EEPROM.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
//Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Adafruit_SSD1306 display(OLED_RESET);
//Adafruit_SSD1306 display = Adafruit_SSD1306(128, 64);
//Adafruit_SSD1306 display = Adafruit_SSD1306(128, 64, &Wire);

// -- interner Datenspeicher
int address = 0;
byte value;

int i; // Zählr für Verzögerungsschleife

int HzsT = 640;     // soll Heizungstemp 30  NTC Ein bei 640 und Aus bei 620 EIN
int deltaHzsT = 4;  // Delta zum abschalten Heizpumpe M13 Hysterresse gegen Pulen bei unterer Grenze
int WWsT = 465;     // soll Warmwassertemp Ein
int hysthz = 10;    // Hysterese 1Kelvin  NTC -  PTC +  -20 Digits
int hystww = 10;    // Hysterese 1Kelvin  NTC -  PTC +  -20 Digits

//---Sensoren:
int TLade = 25;     // Temperatur Ladespeicher

int Thz;            // Temperatur Heizung  Vorbelegung
int Thzmax = 0;     // Vorbelegung Neustart. Wird aus EEPROM gelesen!
int Thzmin = 1024;  // Vorbelegung Neustart. Wird aus EEPROM gelesen!

int Tww;            // Temperatur Warmwasser Vorbelegung
int Twwmax = 0;     // Vorbelegung Neustart. Wird aus EEPROM gelesen!
int Twwmin = 1024;  // Vorbelegung Neustart. Wird aus EEPROM gelesen!

int korf = 1;           // Korekturfaktor Temperaturfühler 1-xx
int Tout;               // Temperatur Draussen
int Toutmax = 0;        // Vorbelegung Neustart. Wird aus EEPROM gelesen!
int Toutmin = 1024;     // Vorbelegung Neustart. Wird aus EEPROM gelesen!

int Tsole;                 // Temperatur Sole Außen
int Tsolemax = 0;          // Vorbelegung Neustart. Wird aus EEPROM gelesen!
int Tsolemin = 1024;       // Vorbelegung Neustart. Wird aus EEPROM gelesen!
int Tsolewarn = 900;       // Eingefrierschutz minimal zulässige Soletemperatur
int Tsolewarncount=0;      // Zähler für Anzahl Warnungen
boolean WarnTsole = false; // Warnungsmerker Tsole min erreicht

int vdcount = 0;     // Laufzeit Verdichter Schleife
int maxcount = 2000; // max Laufzeit Verdichter wenn aktiv, Sicherheitsabschaltung zu lange Laufzeit
int vdcountlastww;   // Speicher für letzte Laufzeit WW
int vdcountlasthz;   // Speicher für letzte Laufzeit Heizung
int PTime = 500;     // Pausezeit gegen neuer Anlauf aktiv Sicherheitsabschaltung
int auszeit = 400;   // 100 Verzögerung beim booten Wichtig! Bei Erstinbetriebnahme Werte einstellen!

int akt = 0;         // Aktiv Zähler
int pas = 0;         // Pasiv Zähler

unsigned long mtime; // Zeitstempel für Menü

int ausw = 1;        // Auswahlsteuerung im Menü

int Sthd = 0;            // Störung Hochdruck
int Stnd = 0;            // Störung Niederdruck

unsigned long zeitm;     // Zeit Minuten
unsigned long zeith;     // Zeit Stunden
unsigned long vdn = 0;   // Verdichter Start Anzahl
unsigned long vdnww = 0; // Verdichter Start Anzahl WarmWasser
unsigned long vdnhz = 0; // Verdichter Start Anzahl Heizen

boolean stoerung = false; // Störungsspeicher merker
boolean sommer = true;    // Sommer Heizung aus / Winter Heizung ein 1=Sommer / 0 Winter

void setup()   {
  Serial.begin(9600);

  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  // init done

  // --------- Reference für Temp Vorgabe
  //   analogReference(INTERNAL); // Reference auf 1.1V einstellen +++

  pinMode(2, OUTPUT); // WP
  pinMode(3, OUTPUT); // M16 Ladepumpe
  pinMode(4, OUTPUT); // M11 Sole
  pinMode(5, OUTPUT); // M18 WW oder Heizungswahl
  pinMode(6, OUTPUT); // M13 Heizung

  pinMode(30, OUTPUT); // Sicherheit Watchdog !!!!!!!!!!!

  pinMode(8, INPUT);  // Hochdruck Öffner
  pinMode(9, INPUT);  // Niederdruck Öffner



  // ---- Tasten Belegung -----------------
  pinMode(12, INPUT_PULLUP);  // T0 (Menue)
  pinMode(13, INPUT_PULLUP);  // T1 (Enter)
  pinMode(10, INPUT_PULLUP);  // T2 (up  + )
  pinMode(11, INPUT_PULLUP);  // T3 (down -)

  // -- alle Pumpen aus
  digitalWrite(2, HIGH);
  digitalWrite(3, HIGH);
  digitalWrite(4, HIGH);
  digitalWrite(5, HIGH);
  digitalWrite(6, HIGH);
  
  // ---- Watchdog
  watchdog();  // Sicherheit Watchdog reset !!!!!!!

  // --- Displaying a simple splash screen
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print("WP WPS90");
  display.setTextSize(1);
  display.setCursor(0, 25);
  display.println(" Stand: 24.2.2020");
  display.setCursor(0, 40);
  display.print(" Youtube => MyTest4u ");
  display.setCursor(0, 52);
  display.print(" Version 1.14 ");
  display.display();

  delay(1000); // 1 Sekunden
  watchdog();  // Sicherheit Watchdog reset !!!!!!!
  delay(1000); // 1 Sekunden
  watchdog();  // Sicherheit Watchdog reset !!!!!!!
  delay(1000); // 1 Sekunden
  watchdog();  // Sicherheit Watchdog reset !!!!!!!
  delay(1000); // 1 Sekunden
  watchdog();  // Sicherheit Watchdog reset !!!!!!!
  
 
//*************** Werte einlesen min max & letzte Einstellungen
//.... Beim Erststart sind alle Inhalte im EEPROM auf #FF .....
//.... Deshalb erst die Werte für Heizen und WW einstellen ....
byte lb; // low Byte
byte hb; // hi Byte

   hb = EEPROM.read(0);
   lb = EEPROM.read(1);
   HzsT=  word(hb,lb); // Zusammensetzen zu einem Wert
   hysthz = EEPROM.read(3);

   hb = EEPROM.read(5);
   lb = EEPROM.read(6);
   WWsT=  word(hb,lb);
   hystww = EEPROM.read(8); 
  
//...   
   hb = EEPROM.read(10);
   lb = EEPROM.read(11);
   Thzmax=  word(hb,lb);

   hb = EEPROM.read(12);
   lb = EEPROM.read(13);
   Thzmin=  word(hb,lb);
 
   hb = EEPROM.read(14);
   lb = EEPROM.read(15);
   Twwmax=  word(hb,lb);
 
   hb = EEPROM.read(16);
   lb = EEPROM.read(17);
   Twwmin=  word(hb,lb);

   hb = EEPROM.read(18);
   lb = EEPROM.read(19);
   Toutmax=  word(hb,lb);
    
   hb = EEPROM.read(20);
   lb = EEPROM.read(21);
   Toutmin=  word(hb,lb);
 
   hb = EEPROM.read(22);
   lb = EEPROM.read(23);
   Tsolemax=  word(hb,lb);
 
   hb = EEPROM.read(24);
   lb = EEPROM.read(25);
   Tsolemin=  word(hb,lb);  
   
// neu Version 1.1.2 / Eingabe & Speicher
   hb = EEPROM.read(26);
   lb = EEPROM.read(27);
   Tsolewarn=  word(hb,lb);  

   sommer = EEPROM.read(28); // 1=Sommer / 0 Winter
// ------

}

void loop() {
  watchdog();  // Sicherheit Watchdog reset !!!!!!!

  // ----- Tempreferenz einlesen
  delay(700); //  Verzögerung ca. 1 Sekunde (Test 10)

  Tmessen();
  //========= Auf Störung prüfen ========================

  if (stoerung) {
    StoerAnzeige(); // Anzeige der Störung
  }
  else {
    Stnd = digitalRead(8); // Störung Niederdruck ?
    Sthd = digitalRead(9); // Störung Hochdruck ?   (soll anderer Eingang sein geht aber nicht !!!!

    Lzeit(); // zeit stunden, minuten holen

    if ((Sthd == 1) || (Stnd == 1)) { // Ist Störung in aktiver Phase aufgetretten?
      stoerung = true;
    }

    if (stoerung) {
      StoerAnzeige(); // Anzeige der Störung
    }
    else
    {
      // ==========================
      // -------------- Steuerung 1

      if (auszeit > PTime) { //  Schaltsperre für Verdichter

        if (Tww > WWsT) { //WWsT(-hyst?)  Test poti oben Vergleich umkehren  ">" für NTC ... "<" für PTC
          akt = akt + 1;
          //  einaus=true;
          WWStart();
        }
        else
        {
          if (Thz > HzsT) { //  Test poti mitte Vergleich umkehren  ">" für NTC ... "<" für PTC
            akt = akt + 1;
            //    einaus=true;
            HzStart();
          }
        }
      }

      // ---- Normal keine Aktion

      if (digitalRead(12) == LOW) {
        Menue();    // Menue aufrufen
      }

      // pas=pas+1;
      // einaus=false;

      //  Heizen ein aus in Abhängigkeit Sommer
      if (sommer) {  
        digitalWrite(6, HIGH);   //  Heizung aus M13 // nur im Winter Sommer Abfrage
      } else {
              // Nur Wenn Heizwasser Mindesttemp HzsT + Delta 2 nicht unterschritten
              if (Thz > HzsT+deltaHzsT ) {digitalWrite(6, HIGH); // M13 aus
                               deltaHzsT=0;       }  // hysterresse gegen wieder einschalten
              else            {digitalWrite(6, LOW);    // M13 ein
                               deltaHzsT=4;       }  // hysterresse gegen wieder ausschalten
             }
             
      Anzeige();
      AnzeigeDaten();
      // delay(700); //  Verzögerung ca. 1 Sekunde (Test 10)

      if (auszeit < PTime + 1) { // nur von 0 auf max zeit zählen sonst überlauf
        auszeit = auszeit + 1; // Auszeit hochzählen
      }

    } // Ende keine Störung
  }
}

//------------------------------------- UP ----------------------------------
//WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW
// ========== WP starten warmes Wasser ===========
void WWStart() {
  auszeit = 0; // Auszeit auf 0
  vdcount = 0; // Laufzeit Verdichter Schleife
  vdn = vdn + 1;
   vdnww = vdnww + 1;

  watchdog();  // Sicherheit Watchdog reset !!!!!!!  
  AnzeigeWW();
  //---------- WP start
  // -- alle Pumpen aus
  digitalWrite(2, HIGH);
  digitalWrite(3, HIGH);
  digitalWrite(4, HIGH);
  digitalWrite(5, HIGH);
  digitalWrite(6, HIGH);

  digitalWrite(5, LOW);   // Umschalten auf WW laden

  //.....Wait 5 sec
  for (int i = 0; i <= 50; i++) {
    Tmessen();
    AnzeigeWW();
    delay(100);
    watchdog();  // Sicherheit Watchdog reset !!!!!!!
  }
  // delay(5000); //
  // -- Startsequenz
  digitalWrite(4, LOW); // M11 Sole

  //.....Wait 1 sec
  for (int i = 0; i <= 10; i++) {
    Tmessen();
    AnzeigeWW();
    delay(100);
    watchdog();  // Sicherheit Watchdog reset !!!!!!!
  }
  // delay(30000); // warte ca. 30 s

  digitalWrite(3, LOW); //  M16 Lade

  //.....Wait 0,5 sec
  for (int i = 0; i <= 5; i++) {
    Tmessen();
    AnzeigeWW();
    delay(100);
    watchdog();  // Sicherheit Watchdog reset !!!!!!!
  }
  // delay(5000);

  digitalWrite(2, LOW); //  Verdichter WP

  //-- -- Wasser aufheizen -- --
  do {
    //========= Auf Störung prüfen =======================
    Stnd = digitalRead(8); // Störung Niederdruck ?
    Sthd = digitalRead(9); // Störung Hochdruck ?   (soll anderer Eingang sein geht aber nicht !!!!
    if ((Sthd == 1) || (Stnd == 1)) {
      stoerung = true;
    }
    //=====================================================

    vdcount = vdcount + 1; // 100 test sonst 1
    // ----- Tempreferenz einlesen
    Tmessen();
    if (Tsole>Tsolewarn) {
      WarnTsole = true;
      Tsolewarncount++; // Zähler um 1 erhöhen
      } // Tsolewarn überprüfen
    AnzeigeWW();
    delay(500);
    watchdog();  // Sicherheit Watchdog reset !!!!!!!
    
  } while ((Tww > WWsT - hystww) && (vdcount < maxcount) && (stoerung == false) && (Tsolewarn>Tsole));
// Ende bei Temp erreicht oder WP Zeitüberschreitung oder Eingefrierschutz

  vdcountlastww=vdcount;


// Prüfen ob Heizung erforderlich um Ein Aussequenz zu sparen !!!
    // ----- Temp messen
    Tmessen();
    if (Thz > HzsT) {
      auszeit = 0; // Auszeit auf 0
      vdcount = 0; // Laufzeit Verdichter Schleife
      // vdn = vdn + 1;
      vdnhz = vdnhz + 1; // Anzahl der HzStarts
      watchdog();  // Sicherheit Watchdog reset !!!!!!!
      AnzeigeHz();
      HeizenEin(); // WP weiter ohne Unterbrechung mit heizen
    }
  

  //---------- WP Aussequenz
  digitalWrite(2, HIGH); //  Verdichter WP
  //.....Wait 5 sec
  for (int i = 0; i <= 50; i++) {
    Tmessen();
    AnzeigeWW();
    delay(100);
    watchdog();  // Sicherheit Watchdog reset !!!!!!!    
  }
  //delay(10000); // Optional Zeitschleife mit Temp??

  digitalWrite(3, HIGH); //  M16 Lade
  digitalWrite(5, HIGH);  // Umschalten auf WW laden

  //.....Wait 20 sec
  for (int i = 0; i <= 200; i++) {
    Tmessen();
    AnzeigeWW();
    delay(100);
    watchdog();  // Sicherheit Watchdog reset !!!!!!!  
  }
  //delay(20000); // Optional Zeitschleife mit Temp?? unterprogramm mit zeitübergabe

  digitalWrite(4, HIGH); // M11 Sole
}


//HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH
// ========== WP starten heizen ===========
void HzStart() {
  //---------- WP start
  // -- alle Pumpen aus
  auszeit = 0; // Auszeit auf 0
  vdcount = 0; // Laufzeit Verdichter Schleife
  vdn = vdn + 1;
  vdnhz = vdnhz + 1; // Anzahl der HzStarts
  AnzeigeHz();
  watchdog();  // Sicherheit Watchdog reset !!!!!!!
  digitalWrite(2, HIGH);
  digitalWrite(3, HIGH);
  digitalWrite(4, HIGH);
  digitalWrite(5, HIGH);
  digitalWrite(6, HIGH);

  // --Start
  digitalWrite(4, LOW); // M11 Sole
  // delay(30000);
  //.....Wait 1 sec
  for (int i = 0; i <= 10; i++) {
    Tmessen();
    AnzeigeHz();
    delay(100);
    watchdog();  // Sicherheit Watchdog reset !!!!!!!
  }

  digitalWrite(3, LOW); //  M16 Lade
  // delay(5000);
  //.....Wait 1 sec
  for (int i = 0; i <= 10; i++) {
    Tmessen();
    AnzeigeHz();
    delay(100);
    watchdog();  // Sicherheit Watchdog reset !!!!!!!  
  }

/*
  digitalWrite(2, LOW); //  Verdichter WP
  //-- bis ist --

  do {
    //========= Auf Störung prüfen ========================
    Stnd = digitalRead(8); // Störung Niederdruck ?
    Sthd = digitalRead(9); // Störung Hochdruck ?   (soll anderer Eingang sein geht aber nicht !!!!
    if ((Sthd == 1) || (Stnd == 1)) {
      stoerung = true;
    }
    //=====================================================

    // ----- Temp messen
    Tmessen();
    if (Tsole>Tsolewarn) {
          WarnTsole = true;
          Tsolewarncount++; // Zähler um 1 erhöhen
    } // Tsolewarn überprüfen
    
    vdcount = vdcount + 1;
    AnzeigeHz();
    delay(500);

  } while ((Thz > HzsT - hysthz) && (vdcount < maxcount) && (stoerung == false) && (Tsolewarn>Tsole)); 
 // Ende bei Temp erreicht oder WP Zeitüberschreitung .....Vergleich umkehren  ">" für NTC ... "<" für PTC

  vdcountlasthz=vdcount;
*/

HeizenEin(); // entspricht obigem Kommentar
 
  //---------- WP aus
  digitalWrite(2, HIGH); //  Verdichter WP
  //delay(5000);
  //.....Wait 5 sec
  for (int i = 0; i <= 50; i++) {
    Tmessen();
    AnzeigeHz();
    delay(100);
    watchdog();  // Sicherheit Watchdog reset !!!!!!!  
  }

  digitalWrite(3, HIGH); //  M16 Lade
  //delay(20000);
  //.....Wait 5 sec
  for (int i = 0; i <= 200; i++) {
    Tmessen();
    AnzeigeHz();
    delay(100);
    watchdog();  // Sicherheit Watchdog reset !!!!!!!
  }

  digitalWrite(4, HIGH); // M11 Sole
}


// aktiver Anteil Heizen ohne Pumpensyncro kann direkt von WW angesprungen werden
void HeizenEin() {
  digitalWrite(5, HIGH);  //  WW Umschalter auf Heizen
  digitalWrite(2, LOW);   //  Verdichter WP ein
  //-- bis ist --

  do {
    //========= Auf Störung prüfen ========================
    Stnd = digitalRead(8); // Störung Niederdruck ?
    Sthd = digitalRead(9); // Störung Hochdruck ?   (soll anderer Eingang sein geht aber nicht !!!!
    if ((Sthd == 1) || (Stnd == 1)) {
      stoerung = true;
    }
    //=====================================================

    // ----- Temp messen
    Tmessen();
    if (Tsole>Tsolewarn) {
          WarnTsole = true;
          Tsolewarncount++; // Zähler um 1 erhöhen
    } // Tsolewarn überprüfen
    
    vdcount = vdcount + 1;
    AnzeigeHz();
    delay(500);
    watchdog();  // Sicherheit Watchdog reset !!!!!!!
  } while ((Thz > HzsT - hysthz) && (vdcount < maxcount) && (stoerung == false) && (Tsolewarn>Tsole)); 
 // Ende bei Temp erreicht oder WP Zeitüberschreitung .....Vergleich umkehren  ">" für NTC ... "<" für PTC

  vdcountlasthz=vdcount;

}



//AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
// -------------------------- Anzeige Betrieb ------------------------------
void Anzeige() {
  display.clearDisplay();
  display.setCursor(10, 0);
  display.println("");
  //  display.print("---------------------");
  display.setTextSize(1);
  display.setCursor(0, 10);
  display.print("T Sole: ");
  display.print(Tsole);
  display.print("C");
  if (sommer) {
    display.print("  Sommer");  //  Heizung aus M13
  } else {
    display.print("  Winter");  //  Heizung ein M13
  }
  display.setCursor(0, 0);
  display.print("Aussentemp:  ");
  display.print(Tout);  // Solltemperatur
  display.print(" C ");
}

// -------- Anzeige Heizen
void AnzeigeHz() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setCursor(10, 18);
  display.setTextSize(2);
  display.print(Tsole);
  display.setTextSize(1);
  display.print(" Celsius");
  display.setCursor(0, 0);
  display.print("Temp soll: ");
  display.print(HzsT);  // Solltemperatur
  display.println(" C ");
  display.print("Temp ist : ");
  display.print(Thz);  // ist Temp
  display.print(" C ");
  display.setCursor(0, 38);
  display.println(vdcount);
  display.setTextSize(1);
  display.print("Heizen");
  //  display.setCursor(10,17);
  display.display();
}

// -------- Anzeige Warm Wasser
void AnzeigeWW() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setCursor(10, 18);
  display.setTextSize(2);
  display.print(Tsole);
  display.setTextSize(1);
  display.print(" Celsius");
  display.setCursor(0, 0);
  display.print("Temp soll: ");
  display.print(WWsT);  // Solltemperatur
  display.println(" C ");
  display.print("Temp ist : ");
  display.print(Tww);  // ist Temp
  display.print(" C ");
  display.setCursor(0, 38);
  display.println(vdcount);
  display.setTextSize(1);
  display.print("Warm Wasser");
  //  display.setCursor(10,17);
  display.display();
}

void AnzeigeDaten() {

  //------- Zeit anzeigen -------------
  display.setCursor(0, 22);
  display.print("BetrZeit: ");
  Lzeit();
  display.print(zeith);
  display.print(":");

  //...... Minuten richtig darstellen
  if (zeitm > 9) {
    display.println(zeitm);
  } else {
    display.print("0");       // führende 0 einfügen
    display.println(zeitm);
  }

  display.print("WP Pause: ");
  display.print(PTime - auszeit + 1);
  display.print("s ");
  display.println(vdn);  // Anzahl der Anläufe VD
  display.setCursor(0, 42);
  display.print("Hz: ");
  display.print(Thz );
  display.print("C ");
  display.print("Soll: ");
  display.print(HzsT);
  display.println("C ");
  display.print("WW: ");
  display.print(Tww );
  display.print("C ");
  display.print("Soll: ");
  display.print(WWsT );
  display.print("C ");
  display.display();
}

// !!!!!!!! STOERUNG !!!!!!!!!!!!!

void  StoerAnzeige() { // Anzeige der Störung
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setCursor(10, 0);
  display.setTextSize(2);
  display.print("Stoerung");
  display.setTextSize(1);
  // display.println("");
  display.setCursor(0, 20);
  display.print(" H-Druck : ");
  display.println(Sthd);  // Störung
  display.print(" N-Druck : ");
  display.println(Stnd);  // störung
  display.setCursor(0, 40);
  display.print("Laufzeit: ");
  display.print(zeith);
  display.print(":");
  display.println(zeitm);
  display.print("Anzahl VD: ");
  display.print(vdn);  // Anzahl VD Starts
  display.display();
  if (digitalRead(13) == LOW) { // Störung zurücksetzen
    stoerung = false;
  }
}

// -------------------- Zeit --------------------
void  Lzeit() { // Laufzeitberechnung
  zeitm = millis() / 60000; // Minuten
  zeith = zeitm / 60; // Stunden /3600000
  zeitm = zeitm - 60 * zeith;
}






//wwwwwwwwwwwwwwwwwww------------  Menue   --------------wwwwwwwwwwwwwwwww

void  Menue()  {
  watchdog();  // Sicherheit Watchdog reset !!!!!!!
  mtime=millis()+120000; // 2 Minute bis Rücksprung
  int ausw = 1;
  delay(500); // 0,2 Sekunden
  do {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.print("---- Haupt-Menue ---");
    display.setCursor(0, 12);
    display.print(" Einstellungen");
    display.setCursor(0, 22);
    display.print("   Pumpen-Test");
    display.setCursor(0, 32);
    display.print("     Messwerte");
    display.setCursor(0, 42);
    display.print("  Werte L-Zeit");
    display.setCursor(0, 52);
    display.print("SonderFunktion");

    display.display();
    // auf ab Positionszähler
    if (digitalRead(11) == LOW) {
      ausw++;
    }
    if (digitalRead(10) == LOW) {
      ausw--;
    }
    if (ausw < 1) {
      ausw = 5;
    }
    if (ausw > 5) {
      ausw = 1;
    }

    display.setTextSize(1);
    display.setCursor(90, 10 * ausw + 2);
    display.println ("<==");

    display.display();

    if (digitalRead(13) == LOW) {
      if (ausw == 1) {Meinstell();}
      if (ausw == 2) {pumpen();}
      if (ausw == 3) {messwert();}
      if (ausw == 4) {werte();}
      if (ausw == 5) {sonder();}    
    }

    delay(200); // 0,2 Sekunden
    watchdog();  // Sicherheit Watchdog reset !!!!!!!
  }  while (digitalRead(12) == HIGH && mtime > millis()); // Ausstieg
}

// +++++++ Einstellen von T heizen + - / WW + - / Sommer Winter / Hystererse
void  Meinstell()  {
  watchdog();  // Sicherheit Watchdog reset !!!!!!!
  mtime=millis()+120000; // 2 Minute bis Rücksprung
  int ausw = 1;
  delay(500); // 0,2 Sekunden
  do {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.print("-- Einstell-Menue --");
    display.setCursor(0, 12);
    display.print("        Heizung");
    display.setCursor(0, 22);
    display.print(" Hz Hysterresse");
    display.setCursor(0, 32);
    display.print("    Warmwassere");
    display.setCursor(0, 42);
    display.print("  WW Hysterrese");
    display.setCursor(0, 52);
    display.print("  Sommer/Winter");

    display.display();
    // auf ab Positionszähler
    if (digitalRead(11) == LOW) {
      ausw++;
    }
    if (digitalRead(10) == LOW) {
      ausw--;
    }
    if (ausw < 1) {
      ausw = 5;
    }
    if (ausw > 5) {
      ausw = 1;
    }

    display.setTextSize(1);
    display.setCursor(93, 10 * ausw + 2);
    display.println ("<==");

    display.display();

    if (digitalRead(13) == LOW) {
      if (ausw == 1) {
        Mheizen();
      }
      if (ausw == 2) {
        Mhyhz();
      }
      if (ausw == 3) {
        Mww();
      }
      if (ausw == 4) {
        Mhyww();
      }
      if (ausw == 5) {
        somzeit();
      }
    }
    watchdog();  // Sicherheit Watchdog reset !!!!!!!
    delay(200); // 0,2 Sekunden

  }  while (digitalRead(12) == HIGH && mtime > millis()); // Ausstieg
}

//hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh
void  Mheizen()  {
  watchdog();  // Sicherheit Watchdog reset !!!!!!!
  mtime=millis()+120000; // 2 Minute bis Rücksprung
  delay(500); // 0,2 Sekunden
  do {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.println("Einstellen Heiztemp");
    display.display();

    if (digitalRead(10) == LOW) {
      HzsT++;
    }
    if (digitalRead(11) == LOW) {
      HzsT--;
    }
    display.setTextSize(1);
    display.setCursor(0, 15);
    display.println ("Heizung +/- :");

    display.setCursor(0, 30);
    display.print("Soll: ");
    display.setTextSize(2);
    display.print(HzsT);
    display.setTextSize(1);
    display.println("C ");

    display.setCursor(0, 52);
    display.print("Ist-Temperatur: ");
    display.print(Thz );
    display.print("C ");
/* 
byte lb;
byte hb;
    display.print(" lb: ");
    lb=lowByte(HzsT);
    display.print(lb );
    display.print(" hb: ");
    hb=highByte(HzsT);
    display.print(hb );  
    display.print(" w: ");   
    display.print(word(hb,lb)); 
 */   
    display.display(); 

    delay(200); // 0,4 Sekunden
    watchdog();  // Sicherheit Watchdog reset !!!!!!!
  }  while (digitalRead(12) == HIGH && mtime > millis() ); // Ausstieg

// --- Abspeichern in EEPROM mit Testausgabe
    display.clearDisplay();
    display.setCursor(0, 0);
byte lb;
byte hb;
    display.println("Schreibe in Adr0 und 1");   
    display.print("Adr 1 lb: ");   
    lb=lowByte(HzsT);
    display.println(lb );
    display.print("adr0  hb: ");
    hb=highByte(HzsT);
    display.println(hb );  
    display.print(" word: ");   
    display.print(word(hb,lb)); // zum Test zusammensetzen
    display.display(); 
    
  EEPROM.update(0, hb); // in Adresse highbyte schreiben
  EEPROM.update(1, lb); // in Adresse lowbyte schreiben
 
 /*** The function EEPROM.update(address, val) is equivalent to the following:
    if( EEPROM.read(address) != val ){EEPROM.write(address, val); }
  ***/
     watchdog();  // Sicherheit Watchdog reset !!!!!!!
     delay(200); // 0,4 Sekunden 
     
// --- Ende EEPROM Schreiben

}

//wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww
void  Mww()  {
 mtime=millis()+120000; // 2 Minute bis Rücksprung  
  watchdog();  // Sicherheit Watchdog reset !!!!!!!  
  delay(500); // 0,2 Sekunden
  do {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.println("Einstellen Wassertemp");
    display.display();

    if (digitalRead(10) == LOW) {
      WWsT++;
    }
    if (digitalRead(11) == LOW) {
      WWsT--;
    }
    display.setTextSize(1);
    display.setCursor(0, 15);
    display.println ("Warmwasser +/- :");

    display.setCursor(0, 30);
    display.print("Soll: ");
    display.setTextSize(2);
    display.print(WWsT);
    display.setTextSize(1);
    display.println("C ");

    display.setCursor(0, 52);
    display.print("Ist-Temperatur: ");
    display.print(Tww );
    display.print("C ");
    display.display();

    delay(200); // 0,4 Sekunden
    watchdog();  // Sicherheit Watchdog reset !!!!!!!
  }  while (digitalRead(12) == HIGH && mtime > millis() ); // Ausstieg


// --- Abspeichern in EEPROM mit Testausgabe
    display.clearDisplay();
    display.setCursor(0, 0);
byte lb;
byte hb;
    display.println("Schreibe in Adr5 und 6");   
    display.print("Adr 6 lb: ");   
    lb=lowByte(WWsT);
    display.println(lb );
    display.print("adr5  hb: ");
    hb=highByte(WWsT);
    display.println(hb );  
    display.print(" word: ");   
    display.print(word(hb,lb)); // zum Test zusammensetzen
    display.display(); 
    
  EEPROM.update(5, hb); // in Adresse highbyte schreiben
  EEPROM.update(6, lb); // in Adresse lowbyte schreiben
 
 /*** The function EEPROM.update(address, val) is equivalent to the following:
    if( EEPROM.read(address) != val ){EEPROM.write(address, val); }
  ***/
     watchdog();  // Sicherheit Watchdog reset !!!!!!!
     delay(200); // 0,4 Sekunden 
 
// --- Ende EEPROM Schreiben

}

//hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh
void  Mhyww()  {
  mtime=millis()+120000; // 2 Minute bis Rücksprung
  watchdog();  // Sicherheit Watchdog reset !!!!!!! 
  delay(500); // 0,2 Sekunden
  do {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.println("Einstellen Wasserhyst");
    display.display();

    if (digitalRead(10) == LOW) {
      hystww++;
    }
    if (digitalRead(11) == LOW) {
      hystww--;
    }
    display.setTextSize(1);
    display.setCursor(0, 15);
    display.println ("Warmwasser Hyst +/- :");

    display.setCursor(0, 30);
    display.print("Soll: ");
    display.setTextSize(2);
    display.print(hystww);
    display.setTextSize(1);
    display.println("C delta");

    display.setCursor(0, 52);
    display.print("Ist-Temperatur: ");
    display.print(Tww );
    display.print("C ");
    display.display();
    
    watchdog();  // Sicherheit Watchdog reset !!!!!!!
    delay(200); // 0,4 Sekunden
  }  while (digitalRead(12) == HIGH && mtime > millis() ); // Ausstieg

// --- Abspeichern in EEPROM mit Testausgabe
    display.clearDisplay();
    display.setCursor(0, 0);
byte lb;
    display.println("Schreibe in Adr3 ");   
    display.print("Adr 3 lb: ");   
    lb=lowByte(hystww);
    display.println(lb );
    display.display(); 
    EEPROM.update(3, lb); // in Adresse lowbyte schreiben
  /*** The function EEPROM.update(address, val) is equivalent to the following:
    if( EEPROM.read(address) != val ){EEPROM.write(address, val); }
  ***/
     watchdog();  // Sicherheit Watchdog reset !!!!!!!   
     delay(200); // 0,4 Sekunden 
// --- Ende EEPROM Schreiben

}

//hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh
void  Mhyhz()  {
  mtime=millis()+120000; // 2 Minute bis Rücksprung
  watchdog();  // Sicherheit Watchdog reset !!!!!!!
  delay(500); // 0,2 Sekunden
  do {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.println("Einstellen Heizhyst");
    display.display();

    if (digitalRead(10) == LOW) {
      hysthz++;
    }
    if (digitalRead(11) == LOW) {
      hysthz--;
    }
    display.setTextSize(1);
    display.setCursor(0, 15);
    display.println ("Heizen Hyst +/- :");

    display.setCursor(0, 30);
    display.print("Soll: ");
    display.setTextSize(2);
    display.print(hysthz);
    display.setTextSize(1);
    display.println("C delta");

    display.setCursor(0, 52);
    display.print("Ist-Temperatur: ");
    display.print(Thz );
    display.print("C ");
    display.display();
    watchdog();  // Sicherheit Watchdog reset !!!!!!!
    delay(200); // 0,4 Sekunden
  }  while (digitalRead(12) == HIGH && mtime > millis() ); // Ausstieg

// --- Abspeichern in EEPROM mit Testausgabe
    display.clearDisplay();
    display.setCursor(0, 0);
byte lb;
    display.println("Schreibe in Adr8 ");   
    display.print("Adr 8 lb: ");   
    lb=lowByte(hysthz);
    display.println(lb );
    display.display(); 
    EEPROM.update(8, lb); // in Adresse lowbyte schreiben
  /*** The function EEPROM.update(address, val) is equivalent to the following:
    if( EEPROM.read(address) != val ){EEPROM.write(address, val); }
  ***/
     watchdog();  // Sicherheit Watchdog reset !!!!!!!
     delay(200); // 0,4 Sekunden 
// --- Ende EEPROM Schreiben


}

//ssssssssssssssssssssssssssssssssssssssssssssssssssssss
void  somzeit()  {
   mtime=millis()+120000; // 2 Minute bis Rücksprung
  watchdog();  // Sicherheit Watchdog reset !!!!!!!
  delay(500); // 0,2 Sekunden
  do {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setCursor(0, 5);
    display.setTextSize(1);
    display.println("Einstellen Sommer");

    if (digitalRead(10) == LOW) {
      sommer = true;
    }
    if (digitalRead(11) == LOW) {
      sommer = false;
    }
    display.setTextSize(1);

    display.setCursor(0, 20);
    display.print("Auswahl ist:  ");

    display.setCursor(10, 35);
    display.setTextSize(2);
    if (sommer) {
      display.print("Sommer");
      display.setTextSize(1);
      display.setCursor(10, 52);
      display.print("Heizung aus!"); //  Heizung aus M13
    } else {
      display.print("Winter");  //  Heizung ein M13
      display.setTextSize(1);
      display.setCursor(10, 52);
      display.print("Heizung ein!");
    }

    display.display();
    watchdog();  // Sicherheit Watchdog reset !!!!!!!
    delay(200); // 0,4 Sekunden
  }  while (digitalRead(12) == HIGH && mtime > millis() ); // Ausstieg

// in EEPROM Speichern next Version

// --- Abspeichern in EEPROM mit Testausgabe
    display.clearDisplay();
    display.setCursor(0, 0);
byte lb;
    display.println("Schreibe in Adr28 ");   
    display.print("Adr 28 lb: ");   
    lb=lowByte(sommer);
    display.println(lb );
    display.display(); 
    EEPROM.update(28, lb); // in Adresse lowbyte schreiben
  /*** The function EEPROM.update(address, val) is equivalent to the following:
    if( EEPROM.read(address) != val ){EEPROM.write(address, val); }
  ***/
     watchdog();  // Sicherheit Watchdog reset !!!!!!!
     delay(200); // 0,4 Sekunden 
// --- Ende EEPROM Schreiben

}


// Sonder ssssssssssssssssssssssssssssssssssssssssssss
void  sonder()  {
  mtime=millis()+120000; // 2 Minute bis Rücksprung
  int ausw = 1;
  watchdog();  // Sicherheit Watchdog reset !!!!!!!
  delay(500); // 0,2 Sekunden
  do {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.print("-- Sonder-Menue --");
    display.setCursor(0, 12);
    display.print("Sole Temp messen");
    display.setCursor(0, 22);
    display.print("      Sicherheit"); // neu in Version 1.12
    display.setCursor(0, 32);
    display.print("    EEPROM lesen");
    display.setCursor(0, 42);
    display.print("Max/Min MemReset");
    display.setCursor(0, 52);
    display.print("    Celsius Test");

    display.display();
    // auf ab Positionszähler
    if (digitalRead(11) == LOW) {
      ausw++;
    }
    if (digitalRead(10) == LOW) {
      ausw--;
    }
    if (ausw < 1) {
      ausw = 5;
    }
    if (ausw > 5) {
      ausw = 1;
    }

    display.setTextSize(1);
    display.setCursor(98, 10 * ausw + 2);
    display.println ("<==");

    display.display();

    if (digitalRead(13) == LOW) {
      if (ausw == 1) {
        soletemp();
      }
      if (ausw == 2) {
        sicherheit();
      }
      if (ausw == 3) {
        speicherlesen();
      }
      if (ausw == 4) {
        maxminmemreset();
      }
      if (ausw == 5) {
       TCelsius();
      }
    }
    watchdog();  // Sicherheit Watchdog reset !!!!!!!
    delay(200); // 0,2 Sekunden

  }  while (digitalRead(12) == HIGH && mtime > millis()); // Ausstieg
}


//sssssssssssssssssssssssssssssssssssssssssccsssssssssSicherheit
void  sicherheit()  {
  mtime=millis()+120000; // 2 Minute bis Rücksprung
 // int ausw = 1;
  watchdog();  // Sicherheit Watchdog reset !!!!!!!
  delay(500); // 0,5 Sekunden
  do {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.println("!! Sicherheit !!");
    display.println("Sole mit +/- anpassen");
    
    if (digitalRead(10) == LOW) {
      Tsolewarn++;
    }
    if (digitalRead(11) == LOW) {
      Tsolewarn--;
    }
    
    display.setCursor(0, 25);
    display.print("Sole min ist: ");
    display.println(Tsolewarn);    

    display.print("So oft: ");
    display.println(Tsolewarncount);    

    display.setTextSize(1);
    if (WarnTsole) {
       display.print("Sole min erreicht");} else 
      {display.print("Sole OK");  //  
    }

   display.display();
    watchdog();  // Sicherheit Watchdog reset !!!!!!!
    delay(100); // 0,2 Sekunden  
    }  while (digitalRead(12) == HIGH && mtime > millis()); // Ausstieg  

     
   WarnTsole=false; // Warnung zurücksetzen

   // --- Abspeichern in EEPROM mit Testausgabe
    display.clearDisplay();
    display.setCursor(0, 0);
byte lb;
byte hb;
    display.println("Schreibe in Adr26 und 27");   
    display.print("Adr 27 lb: ");   
    lb=lowByte(Tsolewarn);
    display.println(lb );
    display.print("adr 26  hb: ");
    hb=highByte(Tsolewarn);
    display.println(hb );  
    display.print(" word: ");   
    display.print(word(hb,lb)); // zum Test zusammensetzen
    display.display(); 
    watchdog();  // Sicherheit Watchdog reset !!!!!!!
    delay(200); // 0,2 Sekunden  
    
  EEPROM.update(26, hb); // in Adresse highbyte schreiben
  EEPROM.update(27, lb); // in Adresse lowbyte schreiben

}



//CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCccccccccccccccccccccCelsius
void  TCelsius()  {
  int cthz=0;
  int ctww=0;
  
  mtime=millis()+120000; // 2 Minute bis Rücksprung
  watchdog();  // Sicherheit Watchdog reset !!!!!!!
  delay(500); // 0,2 Sekunden
  do {

  // ----- Tempreferenz einlesen
  Tout = analogRead(A0); // Test Einstellbereich 0 bis yy Celsius (5V an Poti gegen GND)  Ausentemperatur
  Tsole = analogRead(A1); // Soletemperatur
  Thz = analogRead(A2) / korf; // T Heizen
  Tww = analogRead(A3) / korf; // T WarmWasser
    
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setCursor(0,0);
    display.setTextSize(1);
   
    display.println("Temp Celsius ");
    
    display.print("T Hz: ");
    cthz= 122 - ((Thz * 7) / 50);
    display.print(cthz);  // Anzahl VD Starts
    display.print(" : ");
    display.println(Thz);  // max VD Laufzeit heizung 
    
    display.print("T WW: ");
    ctww= 132 - ((9 * Tww) / 50); 
    display.print(ctww);  // Anzahl VD Starts
    display.print(" : ");
    display.println(Tww);  // max VD Laufzeit ww

    display.print("T sole: ");
    ctww= 132 - ((9 * Tsole) / 50); 
    display.print(ctww);  // Anzahl VD Starts
    display.print(" : ");
    display.println(Tsole);  // max VD Laufzeit ww
    
    display.print("T out: ");
    ctww= 132 - ((9 * Tout) / 50); 
    display.print(ctww);  // Anzahl VD Starts
    display.print(" : ");
    display.println(Tout);  // max VD Laufzeit ww
    

    display.display();
    watchdog();  // Sicherheit Watchdog reset !!!!!!!
    delay(400); // 0,4 Sekunden
  }  while (digitalRead(12) == HIGH && mtime > millis() ); // Ausstieg

}








//soletemp wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww
void  soletemp()  {
 mtime=millis()+120000; // 2 Minute bis Rücksprung
  watchdog();  // Sicherheit Watchdog reset !!!!!!!
  delay(500); // 0,2 Sekunden
  int miin = 1024;
  int maax = 0;
  boolean m16 = false;

  digitalWrite(4, LOW);  //M11

  do {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.println("Sonder Soletemp ");

    Tsole = analogRead(A1); // Soletemperatur
    if (Tsole > maax) {
      maax = Tsole;
    }
    if (Tsole < miin) {
      miin = Tsole;
    }
    display.setCursor(0, 10);
    display.setTextSize(2);

    display.print("ist: ");

    display.println(Tsole);

    //  display.setCursor(0,40);
    display.print("min: ");
    display.println(miin);
    display.print("max: ");
    display.println(maax);

    display.display();
    watchdog();  // Sicherheit Watchdog reset !!!!!!!  
    delay(200); // 0,4 Sekunden
  }  while (digitalRead(12) == HIGH && mtime > millis() ); // Ausstieg


  digitalWrite(4, HIGH);
}



//wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww
void  werte()  {
  mtime=millis()+120000; // 2 Minute bis Rücksprung
  watchdog();  // Sicherheit Watchdog reset !!!!!!!
  delay(500); // 0,2 Sekunden
  do {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
   
    display.setCursor(0,0);
    display.print("Laufzeit: ");
    display.print(zeith);
    display.print(":");

  //...... Minuten richtig darstellen
  if (zeitm > 9) {
    display.println(zeitm);
  } else {
    display.print("0");       // führende 0 einfügen
    display.println(zeitm);
  }
    
    display.print("Anzahl VD Hz: ");
    display.println(vdnhz);  // Anzahl VD Starts
    display.print("Anzahl VD WW: ");
    display.println(vdnww);  // Anzahl VD Starts
       
  display.println("Max last Laufzeit:");
    display.print("Hz:");
    display.print(vdcountlasthz);  // max VD Laufzeit heizung
    display.print(" WW:");
    display.println(vdcountlastww);  // max VD Laufzeit ww

    display.print("Anz. Sole Min: ");  // neu in V1.12
    display.println(Tsolewarncount);  // Anzahl Min Temp Sole erreicht
 
    display.display();
    watchdog();  // Sicherheit Watchdog reset !!!!!!!
    delay(200); // 0,4 Sekunden
  }  while (digitalRead(12) == HIGH && mtime > millis() ); // Ausstieg

}


//wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww
void  speicherlesen()  {
 mtime=millis()+120000; // 2 Minute bis Rücksprung
  watchdog();  // Sicherheit Watchdog reset !!!!!!!
  delay(500); // 0,2 Sekunden
  int ii=0;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print("EEPROM length: ");
  display.println(EEPROM.length()); // Speichergrösse max

  
  do {

//--- Testausgabe EEPROM erste 36 Byte ---
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print("EEPROM length: ");
  display.println(EEPROM.length()); // Speichergrösse max

   if (digitalRead(11) == LOW) { ii=ii-5;    display.display();}
   if (digitalRead(10) == LOW) { ii=ii+5;    display.display();}

   if (ii<0) {ii=0;}
   if (ii>=EEPROM.length()) {ii=EEPROM.length()-5;}
   
  display.setCursor(0, 8);
  for (int address = ii; address <= ii+4 ; address++) {
    value = EEPROM.read(address);

      display.println(" ");
      display.print(address);
      display.print(" : ");
      display.print(value, HEX); // HEX / DEC

  //  display.display();
  //  delay(1); 
   }

   display.display();
  watchdog();  // Sicherheit Watchdog reset !!!!!!!
  delay(50); 
  }  while (digitalRead(12) == HIGH && mtime > millis() ); // Ausstieg

}

//mreset mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
void maxminmemreset(){
  mtime=millis()+120000; // 2 Minute bis Rücksprung
byte lb;
byte hb; 
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(" Max / Min Reset ");
  display.print(" Ja mit up ");
  display.display();
  watchdog();  // Sicherheit Watchdog reset !!!!!!! 
  delay(1000); // 0,4 Sekunden 
  
  do {
    if (digitalRead(11) == LOW) { 
 Thzmax = 0;
  EEPROM.update(10, 0); // in Adresse highbyte schreiben
  EEPROM.update(11, 0); // in Adresse lowbyte schreiben
 
 Thzmin = 1024;
  EEPROM.update(12, 4); // in Adresse highbyte schreiben
  EEPROM.update(13, 0); // in Adresse lowbyte schreiben
 
 Twwmax = 0;
  EEPROM.update(14, 0); // in Adresse highbyte schreiben
  EEPROM.update(15, 0); // in Adresse lowbyte schreiben

 Twwmin = 1024;
  EEPROM.update(16, 4); // in Adresse highbyte schreiben
  EEPROM.update(17, 0); // in Adresse lowbyte schreiben
 
 Toutmax = 0;
  EEPROM.update(18, 0); // in Adresse highbyte schreiben
  EEPROM.update(19, 0); // in Adresse lowbyte schreiben
 
 Toutmin = 1024;
  EEPROM.update(20, 4); // in Adresse highbyte schreiben
  EEPROM.update(21, 0); // in Adresse lowbyte schreiben
 
 Tsolemax = 0;
  EEPROM.update(22, 0); // in Adresse highbyte schreiben
  EEPROM.update(23, 0); // in Adresse lowbyte schreiben
 
 Tsolemin = 1024;
  EEPROM.update(24, 4); // in Adresse highbyte schreiben
  EEPROM.update(25, 0); // in Adresse lowbyte schreiben
    }

// --- Auslesen und Anzeigen
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("-- Max / Min Reset --");
  display.print(" Ja mit [up] ");
  display.setCursor(0, 25);
  
   hb = EEPROM.read(10);
   lb = EEPROM.read(11);
  Thzmax=  word(hb,lb);
  display.print("hzmax:");
  display.print(Thzmax); 

   hb = EEPROM.read(12);
   lb = EEPROM.read(13);
  Thzmin=  word(hb,lb);
  display.print(" mi ");
  display.println(Thzmin); 

   hb = EEPROM.read(14);
   lb = EEPROM.read(15);
  Twwmax=  word(hb,lb);
  display.print("wwmax:");
  display.print(Twwmax);  

   hb = EEPROM.read(16);
   lb = EEPROM.read(17);
  Twwmin=  word(hb,lb);
  display.print(" mi ");
  display.println(Twwmin);  

   hb = EEPROM.read(18);
   lb = EEPROM.read(19);
  Toutmax=  word(hb,lb);
  display.print("outmax:");
  display.print(Toutmax);
    
   hb = EEPROM.read(20);
   lb = EEPROM.read(21);
  Toutmin=  word(hb,lb);
  display.print(" mi ");
  display.println(Toutmin);

   hb = EEPROM.read(22);
   lb = EEPROM.read(23);
  Tsolemax=  word(hb,lb);
  display.print("solmax:");
  display.print(Tsolemax);

   hb = EEPROM.read(24);
   lb = EEPROM.read(25);
  Tsolemin=  word(hb,lb);
  display.print(" mi ");
  display.println(Tsolemin);     
  
  display.display();
  watchdog();  // Sicherheit Watchdog reset !!!!!!!
  delay(200); // 0,4 Sekunden 
  
 }  while (digitalRead(12) == HIGH && mtime > millis() ); // Ausstieg
 
}




//PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
void  pumpen()  {
  mtime=millis()+120000; // 2 Minute bis Rücksprung
  boolean m11 =  false;
  boolean m13 = false;
  boolean m16 =  false;
  boolean m18 = false;

  // -- alle Pumpen aus
  digitalWrite(2, HIGH);
  digitalWrite(3, HIGH);
  digitalWrite(4, HIGH);
  digitalWrite(5, HIGH);
  digitalWrite(6, HIGH);
  watchdog();  // Sicherheit Watchdog reset !!!!!!!
  delay(500); // 0,5 Sekunden
  do {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.setTextSize(1);

    display.print("-- Pumpentest --");
    display.setCursor(0, 12);
    display.print(" M11");
    if (m11) {
      digitalWrite(4, LOW); // M11
      display.print(" ein ");
    }
    else
    {
      digitalWrite(4, HIGH); // M11
      display.print(" aus ");
    }

    display.setCursor(0, 22);
    display.print(" M13");
    if (m13) {
      digitalWrite(6, LOW); // M13
      display.print(" ein ");
    }
    else
    {
      digitalWrite(6, HIGH); // M13
      display.print(" aus ");
    }

    display.setCursor(0, 32);
    display.print(" M16");
    if (m16) {
      digitalWrite(3, LOW); // M16
      display.print(" ein ");
    }
    else
    {
      digitalWrite(3, HIGH); // M16
      display.print(" aus ");
    }

    display.setCursor(0, 42);
    display.print(" M18");
    if (m18) {
      digitalWrite(5, LOW); // M18
      display.print(" ein ");
    }
    else
    {
      digitalWrite(5, HIGH); // M18
      display.print(" aus ");
    }

    display.display();
    // auf ab Positionszähler
    if (digitalRead(11) == LOW) {
      ausw++;
    }
    if (digitalRead(10) == LOW) {
      ausw--;
    }
    if (ausw < 1) {
      ausw = 4;
    }
    if (ausw > 4) {
      ausw = 1;
    }

    display.setTextSize(1);
    display.setCursor(55, 10 * ausw + 2);
    display.println ("<==");

    if (digitalRead(13) == LOW) {

      if (ausw == 1) {
        if (m11) {
          m11 = false;
        } else {
          m11 = true;
        }
      }

      if (ausw == 2) {
        if (m13) {
          m13 = false;
        } else {
          m13 = true;
        }
      }

      if (ausw == 3) {
        if (m16) {
          m16 = false;
        } else {
          m16 = true;
        }
      }

      if (ausw == 4) {
        if (m18) {
          m18 = false;
        } else {
          m18 = true;
        }
      }

    }
    display.display();
    watchdog();  // Sicherheit Watchdog reset !!!!!!!
    delay(500); // 0,4 Sekunden

  }  while (digitalRead(12) == HIGH && mtime > millis() ); // Ausstieg

  // -- alle Pumpen aus
  digitalWrite(2, HIGH);
  digitalWrite(3, HIGH);
  digitalWrite(4, HIGH);
  digitalWrite(5, HIGH);
  digitalWrite(6, HIGH);
}


// mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
void  messwert()  {
  mtime=millis()+120000; // 2 Minute bis Rücksprung
  // min Max Heizung / Rücklauf/ WW / Störungen / Laufzeit-Pumpen /Störungen /Eingefrierschutz
  watchdog();  // Sicherheit Watchdog reset !!!!!!!
  delay(500);
  do {
    // ----- Tempreferenz einlesen
    Thz = analogRead(A2) / korf; // T Heizen
    Tww = analogRead(A3) / korf; // T WarmWasser
    Tout = analogRead(A0); // Test Einstellbereich 0 bis yy Celsius (5V an Poti gegen GND)  Ausentemperatur
    Tsole = analogRead(A1);

    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.setTextSize(1);

    display.print("Out: ");
    display.print(Tout );
    display.println("C ");
    display.print("Min: ");
    display.print(Toutmin);
    display.print("C ");
    display.print("Max: ");
    display.print(Toutmax);
    display.println("C");

    display.print("Sole: ");
    display.print(Tsole );
    display.println("C ");
    display.print("Min: ");
    display.print(Tsolemin);
    display.print("C ");
    display.print("Max: ");
    display.print(Tsolemax);
    display.println("C");

    display.print("Heizung: ");
    display.print(Thz );
    display.println("C ");
    display.print("Min: ");
    display.print(Thzmin);
    display.print("C ");
    display.print("Max: ");
    display.print(Thzmax);
    display.println("C");

    display.print("Wasser: ");
    display.print(Tww );
    display.println("C ");
    display.print("Min: ");
    display.print(Twwmin);
    display.print("C ");
    display.print("Max: ");
    display.print(Twwmax);
    display.println("C");

    display.display();
    watchdog();  // Sicherheit Watchdog reset !!!!!!!
    delay(200); // 0,4 Sekunden
  }  while (digitalRead(12) == HIGH && mtime > millis() ); // Ausstieg

}

//mes mes mes mes mes mes mes mes mes mes mes
void  Tmessen()  {

  // ----- Tempreferenz einlesen
  Tout = analogRead(A0); // Test Einstellbereich 0 bis yy Celsius (5V an Poti gegen GND)  Ausentemperatur
  Tsole = analogRead(A1); // Soletemperatur
  Thz = analogRead(A2) / korf; // T Heizen
  Tww = analogRead(A3) / korf; // T WarmWasser

  //  max min speichern
  if (Tout > Toutmax) {
    Toutmax = Tout;
    
  EEPROM.update(18, highByte(Tout)); // in Adresse highbyte schreiben
  EEPROM.update(19, lowByte(Tout)); // in Adresse lowbyte schreiben   
  }
  if (Tout < Toutmin) {
    Toutmin = Tout;
    
  EEPROM.update(20, highByte(Tout)); // in Adresse highbyte schreiben
  EEPROM.update(21, lowByte(Tout)); // in Adresse lowbyte schreiben
  }

  if (Tsole > Tsolemax) {
    Tsolemax = Tsole; 
  EEPROM.update(22, highByte(Tsolemax)); // in Adresse highbyte schreiben
  EEPROM.update(23, lowByte(Tsolemax)); // in Adresse lowbyte schreiben
    
  }
  if (Tsole < Tsolemin) {
    Tsolemin = Tsole;  
  EEPROM.update(24, highByte(Tsolemin)); // in Adresse highbyte schreiben
  EEPROM.update(25, lowByte(Tsolemin)); // in Adresse lowbyte schreiben
  }

  if (Thz > Thzmax) {
    Thzmax = Thz;  
  EEPROM.update(10, highByte(Thzmax)); // in Adresse highbyte schreiben
  EEPROM.update(11, lowByte(Thzmax)); // in Adresse lowbyte schreiben
    
  }
  if (Thz < Thzmin) {
    Thzmin = Thz;
  EEPROM.update(12, highByte(Thzmin)); // in Adresse highbyte schreiben
  EEPROM.update(13, lowByte(Thzmin)); // in Adresse lowbyte schreiben
    
  }

  if (Tww > Twwmax) {
    Twwmax = Tww; 
  EEPROM.update(14, highByte(Twwmax)); // in Adresse highbyte schreiben
  EEPROM.update(15, lowByte(Twwmax)); // in Adresse lowbyte schreiben
  
  }
  if (Tww < Twwmin) {
    Twwmin = Tww;  
  EEPROM.update(16, highByte(Twwmin)); // in Adresse highbyte schreiben
  EEPROM.update(17, lowByte(Twwmin)); // in Adresse lowbyte schreiben
    
  }
   
}


// !!!!!! watchdog !!!!!!! watchdog !!!!!!! watchdog !!!!!!! watchdog !!!!!!!
// !!!!!! Pin 30 Reset watchdog High nach Low
void  watchdog()  {

  digitalWrite(30, LOW);
  delay(1); // Test
  digitalWrite(30, HIGH);
}
