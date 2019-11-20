
/*
Umgebung: 
Arduino 1.8.10 
Adafruit SSD1306  Version 1.1.2
MEGA2560
 * 
Steuerung für Wearmepumpe (Buderus WP90)   16.11.2019
arduino.4wonder.de
YouTube-Kanal: mytest4u

xxxxxxxx Test Version 1 xxxxxxxx
keine Temp nur Spannung
Datei  Waermepump8_2560
 

Testwerte 
WW     455 Ein   440 Aus
Heizen 640 Ein   620 Aus   

Arduino Nano in-/out-puts:
A0 Aussen Temperatur Einstellen Poti Bereich ( Masse - 5V ) Heizen oder WW
A2 Tempfühler Hz
A3 Tempfühler WW

Display
(A5) SLC 
(A4) SDA

Pumpen ( 1 HIGH  = Aus  / 0 LOW = Ein )
D2 Verdichter / Kompresssor
D3 M16 Ladepumpe Speicher
D4 M11 Sole / Außen Wärmekörbe im Boden
D5 M18 WW / Heizung umschalter
D6 M13 Heizung

Sicherheit
D10 Hochdruck Schalter
D11 Niederdruck Schalter

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

int HzsT=640;     // soll Heizungstemp 30  NTC Ein bei 640 und Aus bei 620 EIN
int WWsT=465;     // soll Warmwassertemp Ein
int hysthz=-10;  // ---------- Hysterese 1Kelvin  NTC -  PTC +  -20 Digits
int hystww=-10;  // ---------- Hysterese 1Kelvin  NTC -  PTC +  -20 Digits

//---Sensoren:
int TLade=25; // Temperatur Ladespeicher

int Thz; // Temperatur Heizung  Vorbelegung
int Thzmax=0;
int Thzmin=1024; 

int Tww; // Temperatur Warmwasser Vorbelegung
int Twwmax=0;
int Twwmin=1024; 

int korf=1; // Korekturfaktor Temperaturfühler 1-xx
int Tout; // Temperatur Draussen 
int Toutmax=0;
int Toutmin=1024; 

int Tsole; // Temperatur Sole Außen
int Tsolemax=0;
int Tsolemin=1024; 

int vdcount=0; // Laufzeit Verdichter Schleife
int maxcount=1000; // max Laufzeit Verdichter wenn aktiv Sicherheitsabschaltung
int PTime =1500; // Pausezeit gegen neuer Anlauf aktiv Sicherheitsabschaltung
int auszeit=1501;

int akt=0; // Aktiv Zähler
 int pas=0; // Pasiv Zähler 

int ausw=1; // Auswahlsteuerung im Menü

int Sthd=0;  // Störung Hochdruck
int Stnd=0;  // Störung Niederdruck

unsigned long zeitm; // Zeit Minuten
unsigned long zeith; // Zeit Stunden
unsigned long vdn=0; // Verdichter Start Anzahl

boolean stoerung = false; // Störungsspeicher merker
boolean sommer =false; // Sommer Heizung aus / Winter Heizung ein

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

  pinMode(8, INPUT);   //  Hochdruck Öffner
  pinMode(9, INPUT);   //  Niederdruck Öffner

// ---- Tasten
  pinMode(12, INPUT_PULLUP);  // T0
  pinMode(13, INPUT_PULLUP);  // T1
  pinMode(10, INPUT_PULLUP);  // T2
  pinMode(11, INPUT_PULLUP);  // T3
  
// -- alle Pumpen aus
 digitalWrite(2, HIGH);   
 digitalWrite(3, HIGH); 
 digitalWrite(4, HIGH);   
 digitalWrite(5, HIGH); 
 digitalWrite(6, HIGH);   
  
//Displaying a simple splash screen 
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,10);
  display.print("WP WPS90");
  display.setTextSize(1); 
   display.println("");
  display.setCursor(0,30); 
  display.println("Stand: 20.11.2019"); 
  display.println("Youtube => MyTest4u"); 
  display.print("Version 1.112 ");    
  display.display();

  delay(5000); // 2 Sekunden

//--- Testausgabe EEPROM erste 36 Byte
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("EEPROM Test 0-35");
  
  for (int address=0; address <= 35 ; address++){
  value = EEPROM.read(address);
  if (address==0 || address==6 || address==12 || address==18 || address==24 || address==30  ){
    
  if (address==0 || address==6 ){display.println("");display.print(" ");}else{display.println("");}
  display.print(address);
  display.print(":");
  }
  display.print(" "); 
  display.print(value, HEX); // HEX / DEC
        
  display.display();
  delay(10); // 2 Sekunden
   }
 delay(5000); // 2 Sekunden
}


void loop() {   
  
// ----- Tempreferenz einlesen

 delay(700); //  Verzögerung ca. 1 Sekunde (Test 10)
 
 Tmessen();
  
//========= Auf Störung prüfen ========================

if (stoerung){StoerAnzeige();} // Anzeige der Störung 
             else {
  Stnd = digitalRead(8); // Störung Niederdruck ?
  Sthd = digitalRead(9); // Störung Hochdruck ?   (soll anderer Eingang sein geht aber nicht !!!!    

  Lzeit(); // zeit stunden, minuten holen
   
if ((Sthd==1) || (Stnd==1)) { // Ist Störung in aktiver Phase aufgetretten?
   stoerung = true; 
}

if (stoerung){StoerAnzeige();} // Anzeige der Störung 
         else
             {
// ===========
// -------------- Steuerung 1 

if (auszeit>PTime) { //  Schaltsperre für Verdichter

  if (Tww>WWsT) {  //WWsT(-hyst?)  Test poti oben Vergleich umkehren  ">" für NTC ... "<" für PTC
   akt=akt+1;  
 //  einaus=true;   
   WWStart(); 
   }
   else
   {
 if (Thz>HzsT) {  //  Test poti mitte Vergleich umkehren  ">" für NTC ... "<" für PTC
     akt=akt+1;    
 //    einaus=true;  
     HzStart();
    } 
 }
}

// ---- Normal keine Aktion

  if (digitalRead(12)==LOW){  Menue();   } // Menue aufrufen

// pas=pas+1;  
// einaus=false; 
 
//  Heizen ein aus in Abhängigkeit Sommer 
  if (sommer){
  digitalWrite(6, HIGH);   //  Heizung aus M13 // nur im Winter Sommer Abfrage
  }else{
  digitalWrite(6, LOW);   //  Heizung ein M13 // nur im Winter Sommer Abfrage
  }
  
  Anzeige();
  AnzeigeDaten();
 // delay(700); //  Verzögerung ca. 1 Sekunde (Test 10)

if (auszeit<PTime+1) {// nur von 0 auf max zeit zählen sonst überlauf
  auszeit=auszeit+1; // Auszeit hochzählen
  } 

 } // Ende keine Störung
}
}

//------------------------------------- UP ----------------------------------
//WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW
// ========== WP starten warmes Wasser ===========
void WWStart() {
 auszeit=0; // Auszeit auf 0
 vdcount=0; // Laufzeit Verdichter Schleife
 vdn=vdn+1;
 AnzeigeWW();
//---------- WP start
// -- alle Pumpen aus
 digitalWrite(2, HIGH);   
 digitalWrite(3, HIGH); 
 digitalWrite(4, HIGH);   
 digitalWrite(5, HIGH); 
 digitalWrite(6, HIGH);   

 digitalWrite(5, LOW);   // Umschalten auf WW laden

//.....Wait 10 sec
   for (int i=0; i <= 50; i++){
    Tmessen();
    AnzeigeWW();
    delay(100); 
   }  
// delay(5000); // 
// -- Startsequenz
 digitalWrite(4, LOW); // M11 Sole

//.....Wait 10 sec
   for (int i=0; i <= 300; i++){
    Tmessen();
    AnzeigeWW();
    delay(100); 
   }   
// delay(30000); // warte ca. 30 s

 digitalWrite(3, LOW); //  M16 Lade

//.....Wait 10 sec
   for (int i=0; i <= 50; i++){
    Tmessen();
    AnzeigeWW();
    delay(100); 
   }  
// delay(5000); 
 
 digitalWrite(2, LOW); //  Verdichter WP

//-- -- Wasser aufheizen -- --
do {
//========= Auf Störung prüfen =======================
  Stnd = digitalRead(8); // Störung Niederdruck ?
  Sthd = digitalRead(9); // Störung Hochdruck ?   (soll anderer Eingang sein geht aber nicht !!!!
  if ((Sthd==1) || (Stnd==1)) {stoerung = true;}
//=====================================================
 
  vdcount=vdcount+1;
// ----- Tempreferenz einlesen
 Tmessen();

 AnzeigeWW();
 delay(500); 
  
} while ((Tww>WWsT+hystww)&&(vdcount<maxcount)&&(stoerung==false)); // Ende bei Temp erreicht oder WP Zeitüberschreitung .... Vergleich umkehren  ">" für NTC ... "<" für PTC

//---------- WP Aussequenz
 digitalWrite(2,HIGH); //  Verdichter WP
//.....Wait 10 sec
   for (int i=0; i <= 100; i++){
    Tmessen();
    AnzeigeWW();
    delay(100); 
   } 
 //delay(10000); // Optional Zeitschleife mit Temp??
 
 digitalWrite(3,HIGH); //  M16 Lade
 digitalWrite(5,HIGH);   // Umschalten auf WW laden

 //.....Wait 10 sec
   for (int i=0; i <= 100; i++){
    Tmessen();
    AnzeigeWW();
    delay(100); 
   } 
 //delay(20000); // Optional Zeitschleife mit Temp?? unterprogramm mit zeitübergabe
 
 digitalWrite(4,HIGH); // M11 Sole
}


//HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH
// ========== WP starten heizen ===========
void HzStart() {
//---------- WP start
// -- alle Pumpen aus
  auszeit=0; // Auszeit auf 0
  vdcount=0; // Laufzeit Verdichter Schleife
  vdn=vdn+1;
  AnzeigeHz();
  
 digitalWrite(2, HIGH);   
 digitalWrite(3, HIGH); 
 digitalWrite(4, HIGH);   
 digitalWrite(5, HIGH); 
 digitalWrite(6, HIGH);   

// --Start
 digitalWrite(4,LOW); // M11 Sole
// delay(30000);
//.....Wait 10 sec
   for (int i=0; i <= 300; i++){
    Tmessen();
    AnzeigeHz();
    delay(100); 
   }  
 
 digitalWrite(3,LOW); //  M16 Lade
// delay(5000); 
//.....Wait 10 sec
   for (int i=0; i <= 50; i++){
    Tmessen();
    AnzeigeHz();
    delay(100); 
   }  

 
 digitalWrite(2,LOW); //  Verdichter WP
//-- bis ist --

do {
//========= Auf Störung prüfen ========================
  Stnd = digitalRead(8); // Störung Niederdruck ?
  Sthd = digitalRead(9); // Störung Hochdruck ?   (soll anderer Eingang sein geht aber nicht !!!!
if ((Sthd==1) || (Stnd==1)) {stoerung = true;}
//=====================================================

// ----- Tempreferenz einlesen
 Tmessen();
  
 vdcount=vdcount+1;
 AnzeigeHz();
 delay(500); 
  
} while ((Thz>HzsT+hysthz)&&(vdcount<maxcount)&&(stoerung==false)); // Ende bei Temp erreicht oder WP Zeitüberschreitung .....Vergleich umkehren  ">" für NTC ... "<" für PTC

//---------- WP aus
 digitalWrite(2,HIGH); //  Verdichter WP
 delay(5000);
//.....Wait 10 sec
   for (int i=0; i <= 50; i++){
    Tmessen();
    AnzeigeHz();
    delay(100); 
   }  
  
 digitalWrite(3,HIGH); //  M16 Lade
 delay(20000); 
 //.....Wait 10 sec
   for (int i=0; i <= 200; i++){
    Tmessen();
    AnzeigeHz();
    delay(100); 
   }  
 
 digitalWrite(4,HIGH); // M11 Sole
}

//AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
// -------------------------- Anzeige Betrieb ------------------------------
void Anzeige() {
  display.clearDisplay();
  display.setCursor(10,0);
  display.println("");  
//  display.print("---------------------");  
  display.setTextSize(1);
  display.setCursor(0,10);
  display.print("T Sole: ");  
  display.print(Tsole);  
  display.print("C");
    if (sommer){
    display.print("  Sommer");  //  Heizung aus M13 
     }else{
    display.print("  Winter");  //  Heizung ein M13 
     }
  display.setCursor(0,0);
  display.print("Aussentemp:  ");  
  display.print(Tout);  // Solltemperatur
  display.print(" C ");       
}

// -------- Anzeige Heizen
void AnzeigeHz() {
  display.clearDisplay();
  display.setTextColor(WHITE); 
  display.setCursor(10,18);  
  display.setTextSize(2);  
  display.print(Tsole);  
  display.setTextSize(1); 
  display.print(" Celsius");
  display.setCursor(0,0);
  display.print("Temp soll: ");  
  display.print(HzsT);  // Solltemperatur
  display.println(" C "); 
  display.print("Temp ist : ");         
  display.print(Thz);  // ist Temp
  display.print(" C ");      
  display.setCursor(0,38);
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
  display.setCursor(10,18); 
  display.setTextSize(2);  
  display.print(Tsole);  
  display.setTextSize(1); 
  display.print(" Celsius");
  display.setCursor(0,0);
  display.print("Temp soll: ");  
  display.print(WWsT);  // Solltemperatur
  display.println(" C ");  
  display.print("Temp ist : ");        
  display.print(Tww);  // ist Temp
  display.print(" C ");      
  display.setCursor(0,38);
  display.println(vdcount);  
  display.setTextSize(1);
  display.print("Warm Wasser");  
//  display.setCursor(10,17);
  display.display();  
}

void AnzeigeDaten() {

//------- Zeit anzeigen -------------
  display.setCursor(0,22);
  display.print("BetrZeit: ");  
  Lzeit();
  display.print(zeith);  
  display.print(":");  
  
//...... Minuten richtig darstellen
  if (zeitm > 9) {    
     display.println(zeitm);  
  } else{
     display.print("0");       // führende 0 einfügen
     display.println(zeitm);  
  }
        
  display.print("WP Pause: ");  
  display.print(PTime-auszeit+1);  
  display.print("s ");  
  display.println(vdn);  // Anzahl der Anläufe VD
  display.setCursor(0,42);
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
  display.setCursor(10,0); 
  display.setTextSize(2);  
  display.print("Stoerung");  
  display.setTextSize(1); 
  // display.println("");
  display.setCursor(0,20);
  display.print(" H-Druck : ");  
  display.println(Sthd);  // Störung 
  display.print(" N-Druck : ");        
   display.println(Stnd);  // störung   
   display.setCursor(0,40);
   display.print("Laufzeit: ");   
   display.print(zeith);  
   display.print(":");       
   display.println(zeitm);  
   display.print("Anzahl VD: ");    
   display.print(vdn);  // Anzahl VD Starts
  display.display();
  if (digitalRead(13)== LOW){ // Störung zurücksetzen
     stoerung = false;
    } 
}

// -------------------- Zeit --------------------
void  Lzeit() { // Laufzeitberechnung
   zeitm=millis()/60000; // Minuten
   zeith=zeitm/60; // Stunden /3600000    
   zeitm = zeitm - 60*zeith;
}



//wwwwwwwwwwwwwwwwwww------------  Menue   --------------wwwwwwwwwwwwwwwww

void  Menue()  {
int ausw=1;
  delay(500); // 0,2 Sekunden
do {
  display.clearDisplay();
  display.setTextColor(WHITE);  
  display.setCursor(0,0);
  display.setTextSize(1);  
  display.print("---- Haupt-Menue ---");
  display.setCursor(0,20);
  display.print("Einstellungen");
  display.setCursor(0,30);
  display.print("  Pumpen-Test");
  display.setCursor(0,40);
  display.print("    Messwerte"); 
  display.setCursor(0,50);
  display.print("       Sonder"); 
  
  display.display();
// auf ab Positionszähler
  if (digitalRead(11)== LOW){ ausw++;   }
  if (digitalRead(10)== LOW){ ausw--;   }
  if (ausw<1){ ausw=4;   }
  if (ausw>4){ ausw=1;   }
    
  display.setTextSize(1);  
  display.setCursor(90,10*ausw+10);
  display.println ("<==");

  display.display();
   
if (digitalRead(13)== LOW) {
 if (ausw==1) { Meinstell();   }
 if (ausw==2) { pumpen();   }
 if (ausw==3) { messwert();   }
 if (ausw==4) { sonder();   }
}

  delay(200); // 0,2 Sekunden
  
}  while (digitalRead(12)==HIGH); // Ausstieg
}

// +++++++ Einstellen von T heizen + - / WW + - / Sommer Winter / Hystererse
void  Meinstell()  {
int ausw=1;
  delay(500); // 0,2 Sekunden
do {
  display.clearDisplay();
  display.setTextColor(WHITE);  
  display.setCursor(0,0);
  display.setTextSize(1);  
  display.print("-- Einstell-Menue --");
  display.setCursor(0,12);
  display.print("        Heizung");
  display.setCursor(0,22);
  display.print(" Hz Hysterresse");
  display.setCursor(0,32);
  display.print("    Warmwassere"); 
  display.setCursor(0,42);
  display.print("  WW Hysterrese"); 
  display.setCursor(0,52);
  display.print("  Sommer/Winter"); 
  
  display.display();
// auf ab Positionszähler
  if (digitalRead(11)== LOW){ ausw++;   }
  if (digitalRead(10)== LOW){ ausw--;   }
  if (ausw<1){ ausw=5;   }
  if (ausw>5){ ausw=1;   }

    display.setTextSize(1);  
  display.setCursor(93,10*ausw+2);
  display.println ("<==");

  display.display();
   
if (digitalRead(13)== LOW) {
 if (ausw==1) { Mheizen();   }
 if (ausw==2) { Mhyhz();   }
 if (ausw==3) { Mww();   }
 if (ausw==4) { Mhyww();   }
 if (ausw==5) { somzeit();   }
}

  delay(200); // 0,2 Sekunden
  
}  while (digitalRead(12)==HIGH); // Ausstieg
}

//hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh
void  Mheizen()  {
  delay(500); // 0,2 Sekunden
do {
  display.clearDisplay();
  display.setTextColor(WHITE);  
  display.setCursor(0,0);
  display.setTextSize(1);  
  display.println("Einstellen Heiztemp");
  display.display();

  if (digitalRead(10)== LOW){  HzsT++;   }
  if (digitalRead(11)== LOW){  HzsT--;   }
  display.setTextSize(1);  
  display.setCursor(0,15);
  display.println ("Heizung +/- :");
 
  display.setCursor(0,30);
  display.print("Soll: ");
  display.setTextSize(2);       
  display.print(HzsT);  
  display.setTextSize(1);   
  display.println("C ");
  
  display.setCursor(0,52);
  display.print("Ist-Temperatur: ");  
  display.print(Thz ); 
  display.print("C "); 
  display.display(); 

  delay(200); // 0,4 Sekunden
}  while (digitalRead(12)==HIGH  ); // Ausstieg

}

//wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww
void  Mww()  {
  delay(500); // 0,2 Sekunden
do {
  display.clearDisplay();
  display.setTextColor(WHITE);  
  display.setCursor(0,0);
  display.setTextSize(1);  
  display.println("Einstellen Wassertemp");
  display.display();

  if (digitalRead(10)== LOW){  WWsT++;   }
  if (digitalRead(11)== LOW){  WWsT--;   }
  display.setTextSize(1);  
  display.setCursor(0,15);
  display.println ("Warmwasser +/- :");
 
  display.setCursor(0,30);
  display.print("Soll: ");
  display.setTextSize(2);       
  display.print(WWsT);  
  display.setTextSize(1);   
  display.println("C ");
  
  display.setCursor(0,52);
  display.print("Ist-Temperatur: ");  
  display.print(Tww ); 
  display.print("C "); 
  display.display(); 

  delay(200); // 0,4 Sekunden
}  while (digitalRead(12)==HIGH ); // Ausstieg

}

//hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh
void  Mhyww()  {
  delay(500); // 0,2 Sekunden
do {
  display.clearDisplay();
  display.setTextColor(WHITE);  
  display.setCursor(0,0);
  display.setTextSize(1);  
  display.println("Einstellen Wasserhyst");
  display.display();

  if (digitalRead(10)== LOW){  hystww++;   }
  if (digitalRead(11)== LOW){  hystww--;   }
  display.setTextSize(1);  
  display.setCursor(0,15);
  display.println ("Warmwasser Hyst +/- :");
 
  display.setCursor(0,30);
  display.print("Soll: ");
  display.setTextSize(2);       
  display.print(hystww);  
  display.setTextSize(1);   
  display.println("C delta");
  
  display.setCursor(0,52);
  display.print("Ist-Temperatur: ");  
  display.print(Tww ); 
  display.print("C "); 
  display.display(); 

  delay(200); // 0,4 Sekunden
}  while (digitalRead(12)==HIGH  ); // Ausstieg

}

//hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh
void  Mhyhz()  {
  delay(500); // 0,2 Sekunden
do {
  display.clearDisplay();
  display.setTextColor(WHITE);  
  display.setCursor(0,0);
  display.setTextSize(1);  
  display.println("Einstellen Heizhyst");
  display.display();

  if (digitalRead(10)== LOW){  hysthz++;   }
  if (digitalRead(11)== LOW){  hysthz--;   }
  display.setTextSize(1);  
  display.setCursor(0,15);
  display.println ("Heizen Hyst +/- :");
 
  display.setCursor(0,30);
  display.print("Soll: ");
  display.setTextSize(2);       
  display.print(hysthz);  
  display.setTextSize(1);   
  display.println("C delta");
  
  display.setCursor(0,52);
  display.print("Ist-Temperatur: ");  
  display.print(Thz ); 
  display.print("C "); 
  display.display(); 

  delay(200); // 0,4 Sekunden
}  while (digitalRead(12)==HIGH  ); // Ausstieg

}

//ssssssssssssssssssssssssssssssssssssssssssssssssssssss
void  somzeit()  {
  delay(500); // 0,2 Sekunden
do {
  display.clearDisplay();
  display.setTextColor(WHITE);  
  display.setCursor(0,5);
  display.setTextSize(1);  
  display.println("Einstellen Sommer");
   
  if (digitalRead(10)== LOW){  sommer=true;   }
  if (digitalRead(11)== LOW){  sommer=false;   }
  display.setTextSize(1);  
 
  display.setCursor(0,20);
  display.print("Auswahl ist:  ");
  
  display.setCursor(10,35); 
  display.setTextSize(2);       
   if (sommer){
    display.print("Sommer"); 
    display.setTextSize(1);
    display.setCursor(10,52);     
    display.print("Heizung aus!"); //  Heizung aus M13 
     }else{
    display.print("Winter");  //  Heizung ein M13
    display.setTextSize(1);
    display.setCursor(10,52);     
    display.print("Heizung ein!");
     } 
     
  display.display();
  delay(200); // 0,4 Sekunden
}  while (digitalRead(12)==HIGH  ); // Ausstieg

}



//wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww
void  sonder()  {
  delay(500); // 0,2 Sekunden
int miin=1024;
int maax=0;
boolean m16=false;

  digitalWrite(4, LOW);  //M11 
 
do {
  display.clearDisplay();
  display.setTextColor(WHITE);  
  display.setCursor(0,0);
  display.setTextSize(1);  
  display.println("Sonder Soletemp ");
   
  Tsole=analogRead(A1); // Soletemperatur
if (Tsole>maax){maax=Tsole;}
if (Tsole<miin){miin=Tsole;}
  display.setCursor(0,10); 
  display.setTextSize(2);    
  
  display.print("ist: ");
 
  display.println(Tsole);
  
//  display.setCursor(0,40);   
  display.print("min: ");
  display.println(miin);
  display.print("max: ");
  display.println(maax);
     
  display.display();
  delay(200); // 0,4 Sekunden
}  while (digitalRead(12)==HIGH  ); // Ausstieg


  digitalWrite(4, HIGH);   
}



//wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww
void  werte()  {
  delay(500); // 0,2 Sekunden
do {
  display.clearDisplay();
  display.setTextColor(WHITE);  
  display.setCursor(0,0);
  display.setTextSize(1);  
  display.println("Werte");
   
   display.setCursor(0,20);
   display.print("Laufzeit: ");   
   display.print(zeith);  
   display.print(":");       
   display.println(zeitm);  
   display.print("Anzahl VD: ");    
   display.print(vdn);  // Anzahl VD Starts
     
  display.display();
  delay(200); // 0,4 Sekunden
}  while (digitalRead(12)==HIGH  ); // Ausstieg

}




//PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
void  pumpen()  {
  boolean m11=  false;
  boolean m13 = false;
  boolean m16=  false;
  boolean m18 = false;

 // -- alle Pumpen aus
 digitalWrite(2, HIGH);   
 digitalWrite(3, HIGH); 
 digitalWrite(4, HIGH);   
 digitalWrite(5, HIGH); 
 digitalWrite(6, HIGH);  
   
  delay(500); // 0,2 Sekunden
do {
  display.clearDisplay();
  display.setTextColor(WHITE);  
  display.setCursor(0,0);
  display.setTextSize(1);  

  display.print("-- Pumpentest --");
  display.setCursor(0,12);
  display.print(" M11");
    if (m11){
        digitalWrite(4, LOW); // M11
        display.print(" ein ");  
        }
    else
    {
        digitalWrite(4, HIGH); // M11
        display.print(" aus ");     
    }
    
  display.setCursor(0,22);
  display.print(" M13");
    if (m13){
        digitalWrite(6, LOW); // M13
        display.print(" ein ");  
        }
    else
    {
        digitalWrite(6, HIGH); // M13
        display.print(" aus ");     
    }
  
  display.setCursor(0,32);
  display.print(" M16"); 
    if (m16){
       digitalWrite(3, LOW); // M16    
        display.print(" ein ");  
        }
    else
    {
       digitalWrite(3, HIGH); // M16     
        display.print(" aus ");     
    }
  
  display.setCursor(0,42);
  display.print(" M18"); 
    if (m18){
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
  if (digitalRead(11)== LOW){ ausw++;   }
  if (digitalRead(10)== LOW){ ausw--;   }
  if (ausw<1){ ausw=4;   }
  if (ausw>4){ ausw=1;   }
  
  display.setTextSize(1);  
  display.setCursor(55,10*ausw+2);
  display.println ("<==");
 
if (digitalRead(13)== LOW) {
  
 if (ausw==1) { 
if (m11) {m11=false;}else{m11=true;}
  }
    
 if (ausw==2) { 
if (m13) {m13=false;}else{m13=true;}
  }
   
 if (ausw==3) {
if (m16) {m16=false;}else{m16=true;}
  }
  
 if (ausw==4) { 
if (m18) {m18=false;}else{m18=true;}
  }
 
}
  display.display();
  delay(200); // 0,4 Sekunden
  
}  while (digitalRead(12)==HIGH  ); // Ausstieg

 // -- alle Pumpen aus
 digitalWrite(2, HIGH);   
 digitalWrite(3, HIGH); 
 digitalWrite(4, HIGH);   
 digitalWrite(5, HIGH); 
 digitalWrite(6, HIGH);  
}


// mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
void  messwert()  {

  // min Max Heizung / Rücklauf/ WW / Störungen / Laufzeit-Pumpen /Störungen /Eingefrierschutz
  delay(500); 
do {
// ----- Tempreferenz einlesen
  Thz=analogRead(A2)/korf; // T Heizen
  Tww=analogRead(A3)/korf; // T WarmWasser
  Tout=analogRead(A0); // Test Einstellbereich 0 bis yy Celsius (5V an Poti gegen GND)  Ausentemperatur
  Tsole=analogRead(A1);
  
  display.clearDisplay();
  display.setTextColor(WHITE);  
  display.setCursor(0,0);
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


  delay(200); // 0,4 Sekunden
}  while (digitalRead(12)==HIGH  ); // Ausstieg

}

//mes mes mes mes mes mes mes mes mes mes mes
void  Tmessen()  {
// ----- Tempreferenz einlesen
  Tout=analogRead(A0); // Test Einstellbereich 0 bis yy Celsius (5V an Poti gegen GND)  Ausentemperatur 
  Tsole=analogRead(A1); // Soletemperatur
  Thz=analogRead(A2)/korf; // T Heizen
  Tww=analogRead(A3)/korf; // T WarmWasser
  
//  max min speichern  
if (Tout>Toutmax){Toutmax=Tout;}
if (Tout<Toutmin){Toutmin=Tout;}

if (Tsole>Tsolemax){Tsolemax=Tsole;}
if (Tsole<Tsolemin){Tsolemin=Tsole;}

if (Thz>Thzmax){Thzmax=Thz;}
if (Thz<Thzmin){Thzmin=Thz;}

if (Tww>Twwmax){Twwmax=Tww;}
if (Tww<Twwmin){Twwmin=Tww;}
}
  
