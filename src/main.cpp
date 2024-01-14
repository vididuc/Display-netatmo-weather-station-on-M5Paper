/***************************************************
  Marcos 19.11.2023
  Das ist mein Versuch die Daten aus der Netatmo Weather Station auf dem M5Paper zu bringen.
  Auf der Synology laeuft ein Python Script, das die Daten periodisch von der Netatmo-API holt und auf der Synology in einem JSON-File ablegt.
  Auf der Synology laeuft ein Webserver, welches das JSON-FIle im Heim WLAN zur Verfuegung stellt.
  Mit dem M5Paer soll das JSON-File abgeholt und die benoetigten Werte heraus gelesen und dargestellt werden.  

  Einige Code-Zeilen kommen aus unterschiedliche Versuche.
  netatmo_versuch_from_scratch_display mit einem ESP8266 Arduino FEather HUZZAH Board 
 ****************************************************/

// #include <Arduino.h>



#include <M5EPD.h>
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson.git
#include <NTPClient.h>           //https://github.com/taranais/NTPClient
#include "Orbitron_Medium_20.h"
#include "Orbitron44.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <StreamUtils.h>
#include <TimeLib.h>

// 26.11.2023 Icons aus diesem Projekt genommen https://github.com/tuenhidiy/m5paper-weatherstation/blob/master/src/THPIcons.c
// Beschreibung ist hier https://www.instructables.com/M5Paper-Weather-Station/
/*
#define LGFX_M5PAPER
#include <LovyanGFX.hpp>
#include <THPIcons.c> // 26.11.2023 Marcos beim kompilieren wird deises File nicht gefunden, weiss nicht warum
// Icon Sprites
LGFX gfx;
LGFX_Sprite THPIcons(&gfx);
*/

// 19.11.2023 Marcos hier kann man prototyping machen bzw. den Code testen https://wokwi.com/esp32


// 17.11.2023 Marcos global deklariert, weil so die konstanten pointer temp_indoor und temp_outdoor wenn einmal zugewiesen auch immer den wert erhalten
// bin nicht sicher ob das so ok ist aber es scheint zu funktionieren 
//const size_t capacity = JSON_OBJECT_SIZE(3) + JSON_ARRAY_SIZE(2) + 60;
//DynamicJsonDocument doc(capacity);

// 19.11.2023 Marcos den Assintent benutzen um den Code richtig zu machen https://arduinojson.org/v6/assistant 
StaticJsonDocument<1024> doc;

const char* temp_indoor;
const char* temp_outdoor;
const char* humidity_indoor;
time_t last_measured_indoor;
const char* humidity_outdoor;
time_t last_measured_outdoor;
const char* ssid     = "MARCOSHOMENET";       ///EDIIIT
const char* password = "ducatiA1974"; //EDI8IT

WiFiClient client;

HTTPClient http;

// 25.11.2023 Marcos aus https://github.com/arduino-libraries/NTPClient/tree/master
WiFiUDP ntpUDP;
// By default 'pool.ntp.org' is used with 60 seconds update interval and
// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionally you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);

// 25.11.2023 Marcos
rtc_time_t current_time;
rtc_time_t old_time;
rtc_date_t current_date;
time_t epochTime;
struct tm *ptm; // https://cplusplus.com/reference/ctime/tm/
struct tm *ptm_lastMeasuredIndoor;
struct tm *ptm_lastMeasuredOutdoor;
char timeStrbuff[64];
char dateStrbuff[64];
String weekDays[7] = { "Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag" }; // 25.11.2023 Marcos https://randomnerdtutorials.com/esp8266-nodemcu-date-time-ntp-client-server-arduino/

// 25.11.2023 Marcos
M5EPD_Canvas canvas_top(&M5.EPD);
M5EPD_Canvas canvas_left(&M5.EPD);
M5EPD_Canvas canvas_right(&M5.EPD);

// 26.11.2023 Marcos
#include "FS.h"
#include "SPIFFS.h"

// 02.12.2023 Marcos https://forum.arduino.cc/t/using-millis-for-timing-a-beginners-guide/483573
unsigned long startMillis;  //some global variables available anywhere in the program
unsigned long currentMillis;
const unsigned long period = 240000;  //the value is a number of milliseconds
boolean bRefreshed = false;


void refreshTop() {
    Serial.println("Entering refreshTop...");
    Serial.println("Get NTP time and date...");

    M5.RTC.getTime(&current_time);
    M5.RTC.getDate(&current_date);
    String weekDay = weekDays[timeClient.getDay()];
    canvas_top.clear();
    canvas_top.setTextSize(88);
    
    // 02.12.2023 Marcos center the String on canvas top
    char timeDateStrbuff[64];
    sprintf(timeDateStrbuff, "%02d:%02d %s %02d.%02d.%04d", current_time.hour, current_time.min, weekDay, current_date.day, current_date.mon, current_date.year);
    //int stringWidth = canvas_top.textWidth(timeDateStrbuff);
    // 02.12.2023 Marcos so funktionierts, mit textWidht bekomme ich als Wert 0, ist aber nicht elegant
    //int stringWidth = canvas_top.drawString(timeDateStrbuff, x_startPoint,10);
    // Serial.println("StringWidth: ");
    //Serial.println(stringWidth);
    //x_startPoint = (960 - stringWidth)/2;
    //canvas_top.clear();
    //canvas_top.drawString(timeDateStrbuff, x_startPoint,10);
    
    // 02.12.2023 Marcos drawCentreString funktioniert auch https://github.com/m5stack/m5-docs/blob/master/docs/en/api/lcd.md
    // fuer font habe ich einfach 0 eingegeben und funktioniert, keine Ahnung warum... 
    canvas_top.drawCentreString(timeDateStrbuff,480,0,0);   
    canvas_top.pushCanvas(0,0,UPDATE_MODE_DU);  //Update the screen.

    Serial.println("Exiting refreshTop...");

}

void refreshLeft(){
  Serial.println("Entering refreshLeft (Indoor)...");
  canvas_left.clear();
  canvas_left.setTextSize(32);
  canvas_left.drawCentreString("Indoor",240,0,0);  // 03.12.2023 Marcos
  canvas_left.drawCentreString("Letze Messung:",240,350,0);  // 03.12.2023 Marcos
  
  char timeDateStrbuff[64];
  sprintf(timeDateStrbuff, "%02d:%02d %02d.%02d.%04d", hour(last_measured_indoor)+1, minute(last_measured_indoor), day(last_measured_indoor), month(last_measured_indoor), year(last_measured_indoor));
  canvas_left.drawCentreString(timeDateStrbuff,240,385,0);  // 03.12.2023 Marcos


  canvas_left.drawPngFile(SPIFFS,"/celsius_130_130.png", 80, 60);
  canvas_left.drawPngFile(SPIFFS,"/humidity_130_130.png", 80, 220);
  canvas_left.setTextSize(120);
  canvas_left.drawString(temp_indoor, 230, 50);
  canvas_left.drawString(humidity_indoor, 230,205);
  canvas_left.pushCanvas(0,100,UPDATE_MODE_DU);  //Update the screen.
  Serial.println("Leaving refreshLeft (Indoor)...");
  
}

void refreshRight(){
  Serial.println("Entering refreshRight (Outdoor)...");
  canvas_right.clear(); 
  canvas_right.setTextSize(32);
  canvas_right.drawCentreString("Outdoor",240,0,0);  // 03.12.2023 Marcos 
  canvas_right.drawCentreString("Letze Messung:",240,350,0);  // 03.12.2023 Marcos

  char timeDateStrbuff[64];
  sprintf(timeDateStrbuff, "%02d:%02d %02d.%02d.%04d", hour(last_measured_outdoor)+1, minute(last_measured_outdoor), day(last_measured_outdoor), month(last_measured_outdoor), year(last_measured_outdoor));
  canvas_right.drawCentreString(timeDateStrbuff,240,385,0);  // 03.12.2023 Marcos

  canvas_right.drawPngFile(SPIFFS,"/celsius_130_130.png", 80, 60);
  canvas_right.drawPngFile(SPIFFS,"/humidity_130_130.png", 80, 220);
  String strTempOutdoor=temp_outdoor;
  canvas_right.setTextSize(120);
  canvas_right.drawString(strTempOutdoor, 230, 50);
  canvas_right.drawString(humidity_outdoor, 230,205);
  canvas_right.pushCanvas(480,100,UPDATE_MODE_DU);  //Update the screen.
  Serial.println("Leaving refreshRight (Outdoor)...");
}



// Marcos 19.11.2023 damit werden die Daten geholt.
void getData() {

  // Marcos 12.11.2023
/////////////////////////////////////////////////////////////////////////////////////////////////

  // wait for WiFi connection
  if ((WiFi.status() == WL_CONNECTED)) {



     ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // Marcos 12.11.2023
    // json abzuholen
    // https://arduinojson.org/v6/how-to/use-arduinojson-with-httpclient/
    // Dieser Teil muss innerhalb der IF Anweisung, welche checkt ob wifi vorhanden

    Serial.println("Entered getData()\n");
    Serial.println("Doing request to web server\n");

    // Send request
    //http.begin(client, "http://arduinojson.org/example.json");
    http.useHTTP10(true);
    http.begin(client,"http://192.168.1.100/netatmo_lastdata.json");
    int httpCode = http.GET();
    // 22.11.2023 Marcos man muss den Inhalt von getString in eine Variable ablegen weil sonst danach ist der Inhalt nicht mehr vorhanden, weiss nicht warum...
    // das war das ganze Problem!!!
    String strPayload = http.getString();


    // Print the response
    // Marcos 12.11.2023 das funktioniert, es wird das ausgegeben as im json file enthalten ist
    Serial.println("Printing strPayload");
    Serial.println(strPayload);
    Serial.println("httpCode:");
    Serial.println(httpCode);

    

    // Parse JSON object
    Serial.println("deserializing json\n");
   // 21.11.2023 Marcos Check this one https://arduinojson.org/v6/api/json/deserializejson/

    DeserializationError error = deserializeJson(doc, strPayload);
      if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      client.stop();
      return;
    }

    Serial.println("Content json docs\n");
    
    // Read values
    // Marcos 12.11.2023 funktioniert so nicht
    // check to see how to access the doc informatiob https://arduinojson.org/v6/assistant/#/step4
    temp_indoor = doc["indoor"]["temperature"];
    humidity_indoor = doc["indoor"]["humidity"];
    last_measured_indoor = doc["indoor"]["last_measured"];
    temp_outdoor = doc["outdoor"]["temperature"];
    humidity_outdoor = doc["outdoor"]["humidity"];
    last_measured_outdoor = doc["outdoor"]["last_measured"]; 
    Serial.println("Temperatur indoor: ");
    Serial.println(temp_indoor);
    Serial.println("Temperatur outdoor: ");
    Serial.println(temp_outdoor);
    Serial.println("Humidity indoor: ");
    Serial.println(humidity_indoor);
    Serial.println("Humidity outdoor: ");
    Serial.println(humidity_outdoor);
    Serial.println("Last measured indoor: ");
    Serial.println(last_measured_indoor);
    Serial.println("Last measured outdoor: ");
    Serial.println(last_measured_outdoor);
    // Disconnect
    Serial.println("Closing the http client...");
    http.end();
    for (uint8_t t = 4; t > 0; t--) {
      Serial.printf("[RETURNING GETDATA] WAIT %d...\n", t);
      Serial.flush();
      delay(1000);
    }
  }

}





void setup() {
  // put your setup code here, to run once:
  // 25.11.2023 Marcos mit M5.begin wird die serielle Schnittstelle auf 115200 baud gesetzt siehe Dokumentation http://docs.m5stack.com/en/quick_start/m5paper/arduino
  M5.begin();
  //Serial.begin(115200);
  
  // Marcos 05.11.2023 Warten damit Zeit bleibt um den Serial Monitor einzuschalten 
  delay(5000);

  // Marcos 12.11.2023 Wifi connect
  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }
  
  // 25.11.2023 Marcos
  // RTC initialisieren  
  M5.RTC.begin();
  M5.RTC.getTime(&current_time);


  // 25.11.2023 Marcos
  // Display initialisieren
  Serial.println("Clearing display...");

  M5.EPD.Clear(true);
  M5.EPD.SetRotation(0); // Waagerechte Ausrichtig


  // 25.11.2023 Marcos
  Serial.println("Connecting to wifi...");

  WiFi.begin(ssid, password);

  
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
   
  }

  //Marcos 12.11.2023 end wifi connection
  Serial.println("WIFI Setup done!");
  
  // 25.11.2023 Marcos im Beispiel wird der Client initialisiert nach dem wifi connected wurde
  Serial.println("Starting NTP client...");
  timeClient.begin();
  // 25.11.2023 Marcos vom Anfang die richtige Zeit zu bekommen
  // Prinzip -> Zeit synchronisieren -> RTC mit der synchronisierte Zeit einstellen -> dann kann die Zeit vom RTC abgefragt werden
  // NTP regelmässig updaten und dann RTC updaten
  // man muss epoctime holen um an das datum zu kommen
  Serial.println("Get NTP time and date...");
  timeClient.update();
  epochTime = timeClient.getEpochTime();
  current_time.hour= timeClient.getHours();
  current_time.min=timeClient.getMinutes();
  current_time.sec=timeClient.getSeconds();
  old_time.hour= timeClient.getHours();
  old_time.min=timeClient.getMinutes();
  old_time.sec=timeClient.getSeconds();

  // https://randomnerdtutorials.com/esp8266-nodemcu-date-time-ntp-client-server-arduino/

  ptm = gmtime ((time_t *)&epochTime);
  current_date.day=ptm->tm_mday;
  current_date.mon=ptm->tm_mon+1;
  current_date.year=ptm->tm_year+1900;

  Serial.println("Set current time an date on loacal RTC...");
  M5.RTC.setTime(&current_time);
  // 02.12.2023 Marcos als der M5Paper keine Batterie mehr hatte und neu gestertat wurde dann war das Datum 00.00.2000
  // d.h. das Datum wurde nicht aus NTP uebernommen
  // diese Zeile behebt das Problem
  M5.RTC.setDate(&current_date);


  //25.11.2023 Marcos erst nach dem wir die richtige Zeit haben dann im canvas schreiben
  canvas_top.createCanvas(960, 100);  //Create a canvas. Zeichnenflaeche erstellen 960x540 pixels 
  canvas_top.setTextColor(15); // 25.11.2023 Marcos hier gesehen https://github.com/m5stack/m5-docs/blob/master/docs/en/api/lcd.md
  canvas_left.createCanvas(480, 440);
  canvas_right.createCanvas(480, 440);
  
  //canvas.useFreetypeFont(false); // 26.11.2023 Marcos wenn man vom spiffs font holt dann braucht es diese Zeile nicht
  //canvas.setFreeFont(&Orbitron_Medium_25);
  //canvas.setFreeFont(&Orbitron_Bold_44); 
  //canvas.setTextSize(3); //Set the text size

  // 26.11.2023 Marcos
  // hier schauen um file aus spiffs hochzuladen https://randomnerdtutorials.com/esp32-vs-code-platformio-spiffs/
  if (!SPIFFS.begin(true))
  {
    log_e("SPIFFS Mount Failed");
    Serial.println("SPIFFS Mount Failed");
    while(1);
  }

  
  // esp_err_t  error_load_font = canvas_top.loadFont("/liquid-crystal.ttf", SPIFFS); // Load font files internal flash
  // 03.12.2023 Marcos Font from https://www.dafont.com/digital-7.font?text=-3.4+Sonntag+Indoor&back=theme
  // liquid crystal font hat kein minus zeichen, ich habe etwas versucht mit forgefont aber verstehe das ganze nicht
  esp_err_t  error_load_font = canvas_top.loadFont("/digital-7.ttf", SPIFFS); // Load font files internal flash

  Serial.println("Error loadFont():");
  Serial.println(error_load_font);
  canvas_top.createRender(140);
  canvas_top.createRender(130);
  canvas_top.createRender(120);
  canvas_top.createRender(88);
  canvas_top.createRender(66);
  canvas_top.createRender(48);
  canvas_top.createRender(32);
  
  //////////////////////////////////////////////////////////////////////////////////////////////


  refreshTop();

  /* 26.11.2023 beim kompilieren wird thpicons.c nicht gefunden, weiss nicht warum
  // Draw temperature icon
  THPIcons.createSprite(112, 128);
  THPIcons.setSwapBytes(true);
  THPIcons.fillSprite(WHITE);  
  THPIcons.pushImage(0, 0, 112, 128, (uint16_t *)Temperature112x128);
  THPIcons.pushSprite(100, 10);
  */

  // 26.11.2023 Marcos Versuch ein Icon aus dem WebServer zu holen und dann auf dem Display zu rendern -> dauert zu lange
  // wx_i01_35.png
  /* bool drawPngUrl(const char *url, uint16_t x = 0, uint16_t y = 0,
            uint16_t maxWidth = 0, uint16_t maxHeight = 0,
            uint16_t offX = 0, uint16_t offY = 0,
            double scale = 1.0, uint8_t alphaThreshold = 127);
  */
  //canvas.drawPngUrl("http://192.168.1.100/wx_i01_35.png", 10, 300); // URL of temp icon
  //canvas.drawJpgUrl("https://m5stack.oss-cn-shenzhen.aliyuncs.com/image/example_pic/flower.jpg");
  //canvas1n.pushCanvas(50,0,UPDATE_MODE_GC16); // Position to show icon.
  //canvas.pushCanvas(0,0,UPDATE_MODE_DU);

  getData();

  refreshLeft();
  refreshRight();

  /*
  Serial.println("Draw left canvas...");
  canvas_left.clear();
  boolean drawPngReturn = canvas_left.drawPngFile(SPIFFS,"/celsius_130_130.png", 80, 10);
  drawPngReturn = canvas_left.drawPngFile(SPIFFS,"/humidity_130_130.png", 80, 160);
  canvas_left.setTextSize(120);
  canvas_left.drawString(temp_indoor, 230, 0);
  canvas_left.drawString(humidity_indoor, 230,155);
  canvas_left.pushCanvas(0,140,UPDATE_MODE_DU);  //Update the screen.


 
  Serial.println("Draw right canvas...");
  canvas_right.clear();
  drawPngReturn = canvas_right.drawPngFile(SPIFFS,"/celsius_130_130.png", 80, 10);
  drawPngReturn = canvas_right.drawPngFile(SPIFFS,"/humidity_130_130.png", 80, 160);
  canvas_right.setTextSize(120);
  canvas_right.drawString(temp_outdoor, 230, 0);
  canvas_right.drawString(humidity_outdoor, 230,155);
  canvas_right.pushCanvas(480,140,UPDATE_MODE_DU);
  */
}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.println("Entering loop...");
  
  // 25.11.2023 Marcos ich weiss nicht ob es nötig ist den NTP client so haeufig upzudaten
  // timeClient.update();

  // 25.11.2023 Marcos Zeit auf dem Display aktualisieren nur wenn sich etwas geaendert hat
  M5.RTC.getTime(&current_time);
    if (current_time.min!=old_time.min) {
    refreshTop(); 
    old_time.min=current_time.min;
  } 

  // 02.12.2023 Marcos Temperaturen nur zwischen 06.00 und 23:00 aktualisieren.
  // Synology holt aktuelle Daten nur zwischen 06:00 und 23:00, mehr brauche ich nicht weil in der Nacht nicht auf das Display schaue
  // Synology holt aktuelle Daten alle 10min startet um xx:00 dann xx:10
  // d.h. canvas_left und canvas_right koennen auch alle 10min aktualisiert werden starten xx:01
  /* Serial.println("current_time.hour");
  Serial.println(current_time.hour);  
  if (current_time.hour >= 5 && current_time.hour < 23) {
    Serial.println("Entered first if statement");
    Serial.println("bRefreshed");
    Serial.println(bRefreshed);
    Serial.println("current_time.min");
    Serial.println(current_time.min);  
  */  
  // 14.12.2023 Marcos Refreshen alle 5 min auch in der Nacht da die Synology so oder so laeuft
  // 02.12.2023 Marcos in eine Minute kann der Loop mehrmals durchlaufen werden, wie kann ich verhindern, dass in derselben Minute mehrmals getData u. refreshLeft u. refreshRight abgerufen werden? Refreshen und dann mindestens 5min warten
  if ((current_time.min % 5 == 0) && (bRefreshed==false)) {  
        Serial.println("Entered check minute if statement");
        getData();
        refreshLeft();
        refreshRight();
        bRefreshed=true;
        startMillis=millis();
        Serial.println("Leaving check minute if statement");
        Serial.println("bRefreshed");
        Serial.println(bRefreshed);
        Serial.println("startMillis");
        Serial.println(startMillis); 

        //14.12.2023 Marcos NTP synchronisieren
         Serial.println("Sync NTP time and date every 5min...");
        timeClient.update();
        epochTime = timeClient.getEpochTime();
        current_time.hour= timeClient.getHours();
        current_time.min=timeClient.getMinutes();
        current_time.sec=timeClient.getSeconds();
          // https://randomnerdtutorials.com/esp8266-nodemcu-date-time-ntp-client-server-arduino/

        ptm = gmtime ((time_t *)&epochTime);
        current_date.day=ptm->tm_mday;
        current_date.mon=ptm->tm_mon+1;
        current_date.year=ptm->tm_year+1900;

        Serial.println("Set synched current time an date on loacal RTC...");
        M5.RTC.setTime(&current_time);
        // 02.12.2023 Marcos als der M5Paper keine Batterie mehr hatte und neu gestertat wurde dann war das Datum 00.00.2000
        // d.h. das Datum wurde nicht aus NTP uebernommen
        // diese Zeile behebt das Problem
        M5.RTC.setDate(&current_date);

  }
  else{
      Serial.println("Entered Else of check minute if statement");
      Serial.println("bRefreshed");
      Serial.println(bRefreshed);
      currentMillis = millis();
      Serial.println("startMillis");
      Serial.println(startMillis);  
      Serial.println("currentMillis");
      Serial.println(currentMillis);
      Serial.println("period");
      Serial.println(period);
      Serial.println("Differenz currentMillis - startMillis");
      Serial.println(currentMillis-startMillis);       
      // 02.12.2023 Marcos damit wird geprueft ob seit dem letzten Refresh 4 minute vergangen sind
      // damit wird verhindert, dass in der gleichen Minute die canvas mehrmals refreshed werden
      if (currentMillis - startMillis >= period) {
        Serial.println("Entered check millis if statement");  
        bRefreshed=false;
        Serial.println("bRefreshed has to be false");
        Serial.println(bRefreshed);      
      }
      
    }
    
  //}
  Serial.println("Leaving loop...");
}








