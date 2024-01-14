/***************************************************
  Marcos 19.11.2023
  This is my prototype to display data from ym wehater station on a M5Paper
  On my Synology I have a script running every 5 minutes which is fetching data from netatmo server using the netatmo api.
  After that the script writes a json file to web server running on the Synlogy.
  The M5Paper fetsches the data to be displayed from the web server running on the Synology. All inside my home lan.
  
  In the code there are multiple comments with url's
  This are my source of information and / or examples.

 ****************************************************/





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

// 26.11.2023 Icons from https://github.com/tuenhidiy/m5paper-weatherstation/blob/master/src/THPIcons.c
// Description https://www.instructables.com/M5Paper-Weather-Station/


// 19.11.2023 Marcos hier kann man prototyping machen bzw. den Code testen https://wokwi.com/esp32


// 17.11.2023 Marcos. Global declarations, this way the pointers temp_indoor and temp_outdoor will always contain a correct value.
// don't know if this is state of the art but for me it works


// 19.11.2023 Marcos. Use the Assinstant to get a code to start with https://arduinojson.org/v6/assistant 
StaticJsonDocument<1024> doc;

const char* temp_indoor;
const char* temp_outdoor;
const char* humidity_indoor;
time_t last_measured_indoor;
const char* humidity_outdoor;
time_t last_measured_outdoor;
const char* ssid     = "your SSID";       // you have to EDIT and use your SSID 
const char* password = "your password";   // you have to EDIT and use your password

WiFiClient client;

HTTPClient http;

// 25.11.2023 Marcos from https://github.com/arduino-libraries/NTPClient/tree/master
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
// create canvas
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

// refresh top canvas
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

    
    // 02.12.2023 Marcos. Using drawCentreString  https://github.com/m5stack/m5-docs/blob/master/docs/en/api/lcd.md
    // I used 0 for font. Is working but I do not know why... 
    canvas_top.drawCentreString(timeDateStrbuff,480,0,0);   
    canvas_top.pushCanvas(0,0,UPDATE_MODE_DU);  //Update the screen.

    Serial.println("Exiting refreshTop...");

}

// refrsh the left canvas
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

// refresh the right canvas
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



// Marcos 19.11.2023 get the data from my web server
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
    http.useHTTP10(true);
    http.begin(client,"http://192.168.1.100/netatmo_lastdata.json");
    int httpCode = http.GET();
    // 22.11.2023 Marcos. I put the content from getString into a variable, otherwise the content got lost, don't know why it has be done thsi way...
    String strPayload = http.getString();


    // Print the response
    // Marcos 12.11.2023 it works...
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
  // 25.11.2023 Marcos with M5.begin the serial interface is set to 115200 baud see Dokumentation http://docs.m5stack.com/en/quick_start/m5paper/arduino
  M5.begin();

  //Serial.begin(115200); // not needed due to M5.beginn
  
  // Marcos 05.11.2023 wait to have the time to switch to the serial monitor in visual studio code 
  delay(5000);

  // Marcos 12.11.2023 Wifi connect
  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }
  
  // 25.11.2023 Marcos
  // RTC initialize 
  M5.RTC.begin();
  M5.RTC.getTime(&current_time);


  // 25.11.2023 Marcos
  // Display initialize
  Serial.println("Clearing display...");

  M5.EPD.Clear(true);
  M5.EPD.SetRotation(0); // set to horizontal Ausrichtig


  // 25.11.2023 Marcos
  Serial.println("Connecting to wifi...");

  WiFi.begin(ssid, password);

  
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
   
  }

  //Marcos 12.11.2023 end wifi connection
  Serial.println("WIFI Setup done!");
  
  // 25.11.2023 Marcos in the example the client gets initilised after wifi is connected. I'm doing same
  Serial.println("Starting NTP client...");
  timeClient.begin();
  // 25.11.2023 Marcos get the corrent time from the beginning
  // Approach -> sync time -> set RTC to synched time -> afterwards you are able to get the correct time from the RTC
  // sync NTP from time to time and update RTC 
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

  Serial.println("Set current time an date on local RTC...");
  M5.RTC.setTime(&current_time);
  // 02.12.2023 Marcos if the M5Paper has no power source and it gets started the date will be 00.00.2000
  // it means the was no sync with NTP
  // this line solves the problem
  M5.RTC.setDate(&current_date);


  //25.11.2023 Marcos before you create the canvas and write to it you should have the data to be writen, this is why I first synched the time
  canvas_top.createCanvas(960, 100);  //Create a canvas. 960x540 pixels 
  canvas_top.setTextColor(15); // 25.11.2023 Marcos see https://github.com/m5stack/m5-docs/blob/master/docs/en/api/lcd.md
  canvas_left.createCanvas(480, 440);
  canvas_right.createCanvas(480, 440);


  // 26.11.2023 Marcos
  // see how to upload files to spiffs https://randomnerdtutorials.com/esp32-vs-code-platformio-spiffs/
  if (!SPIFFS.begin(true))
  {
    log_e("SPIFFS Mount Failed");
    Serial.println("SPIFFS Mount Failed");
    while(1);
  }

  
  // esp_err_t  error_load_font = canvas_top.loadFont("/liquid-crystal.ttf", SPIFFS); // Load font files internal flash
  // 03.12.2023 Marcos Font from https://www.dafont.com/digital-7.font?text=-3.4+Sonntag+Indoor&back=theme
  // 
  // liquid crystal font has no minus sign -> I switched to digital-7 font
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
  // get data and refresh every canvas

  refreshTop();

  getData();

  refreshLeft();

  refreshRight();

}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.println("Entering main loop...");
  
  // 25.11.2023 Marcos Zeit auf dem Display aktualisieren nur wenn sich etwas geaendert hat
  M5.RTC.getTime(&current_time);
    if (current_time.min!=old_time.min) {
    refreshTop(); 
    old_time.min=current_time.min;
  } 

  // 14.12.2023 Marcos Refresh every 5 min. Synology is always ruuning
  // 02.12.2023 Marcos. The main loop will be executed several times during the same minute. To avoid refreshing several times during the same minute I will al least wait 5 min to do the next refresh  
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

        //14.12.2023 Marcos NTP synch
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
        // 02.12.2023 Marcos if the M5Paper has no power source and it gets started the date will be 00.00.2000
        // it means the was no sync with NTP
        // this line solves the problem
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
      // 02.12.2023 Marcos check if 4 min passed since last refresh. To avoid refreshing several times during the same minute.
      if (currentMillis - startMillis >= period) {
        Serial.println("Entered check millis if statement");  
        bRefreshed=false;
        Serial.println("bRefreshed has to be false");
        Serial.println(bRefreshed);      
      }
      
    }
    
  Serial.println("Leaving loop...");
}








