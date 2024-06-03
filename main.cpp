/*--------------------------------------------------------------------
 Copyright Douglas Haliburton, February/25/2023 13:09:01

 Battery Life Test : Brown Out Detector is triggered when voltage is 2.54V
                      Installed fresh batteries May 31,2024, voltage was 3.1V
----------------------------------------------------------------------*/
#include "settings.h"
#include <WiFi.h>
#include <AsyncTCP.h>
#include <WiFiClientSecure.h>
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <TimeLib.h>
#include <time.h>
#include <string.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <AsyncMqttClient.h>
#include <string.h>
#include <RTClib.h>

#define BOOT_REASON_MESSAGE_SIZE 150
char bootReasonMessage [BOOT_REASON_MESSAGE_SIZE];

String PROGRAM_NAME = "TideClock_with_RTC";

bool PWR_RST_FLAG = false;
bool FLASH_ERR_FLAG = false;
bool RTC_ERR = false;
bool GET_TIDE_DATA_FLAG = false;

RTC_DS3231 rtc;
bool century = false;
bool h12Flag;
bool pmFlag;
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 0;
DateTime RTCnow;

const int API_TIMEOUT = 15000;  //keep it long if you want to receive headers from client
const int httpsPort = 443;
const long utcOffsetInSeconds = -8 * 60 * 60;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
int hour0 = 0;
int hour1 = 23;
char FromString[21];// Formatted DateTimeStrings to send for tide info
char ToString[21];
unsigned long t, previousDay, tomorrow, DateTo;
time_t Last_Fetched_Date =0; // updated each time a new month of data is downloaded unix timestamp
time_t Last_Fetched_Date_Saved =0;


int NumWeeks = 4; // change this value to 4 for production version

//const char del[] = "[]{},";//Delimiters
//const char del2[] = "T,:,Z"; //delimiters for time
//char *token;
int i , j;
//char *array1[1100]; // token results from 1st split
//char *array2[1100]; // tokens from split of tokens in 1st array
//int loc1, loc2;
String eventDate, value;
//need to store 5 weeks of tide data in the following arrays plus some wiggle room.
//Say 4 tide heights per day * 7 days per week * 5 weeks = 140, round up to 150.
String dateTimeHigh[150];
float tideHeight[150];
unsigned long timeHigh[150];
unsigned long epochMinutes[150];
int index1, index2, index3, index4, numberOfTokens, numberOfDataPoints;
int timeOfDay[3]; // store hours, minutes and seconds
int hourNow, minuteNow, secondNow, minuteTime;
String dateTime1, value1;
unsigned long ClockFacePosition;
#define STATION_DEFAULT  "08074" // default to Campbell River
String STATION = STATION_DEFAULT; 
String STATION_SAVED, STATION_TIDE_DATA, STATION_TIDE_DATA_SAVED;
String SSID = "", SSID_SAVED;
String PASSWORD = "", PASSWORD_SAVED;

int ZERO_OFFSET, ZERO_OFFSET_SAVED ; // When Zero position is detected move the hand a few more steps to true zero, (sensor is not exactly at zero)
#define ZERO_OFFSET_MAX 5

//DFO web site API
//const char* resource = "/api/v1/stations/5cebf1de3d0f4a073c4bb996/data?time-series-code=wlp-hilo&from=2022-01-19T00%3A00%3A00Z&to=2022-01-20T12%3A00%3A00Z";
//const char* oldDFOserver = "api-iwls.dfo-mpo.gc.ca";
const char* DFOserver = "api.iwls-sine.azure.cloud-nuage.dfo-mpo.gc.ca";
String StationID = "5cebf1de3d0f4a073c4bb996"; // Campbell River
const String Link1 = "/api/v1/stations/";
const String Link2 = "/data?time-series-code=wlp-hilo&from=";
const String DateFromTest = "2022-01-19T00:00:00Z";
const String DateToTest = "2022-01-20T00:00:00Z";

bool AP_Flag;

// Variables to save date and time
unsigned long DateFromWeek[5], DateToWeek[5];
// DateFromTo[5][44];
String DateFromTo[5];
String formattedDate;
String dayStamp;
String timeStamp;
unsigned long minuteOfDay;
String testString;
char GetResponse = '0';
String eventDate1;
String data;
bool deepSleepWake = false;
#define SIX_MINUTES 60*6 
#define TEN_SECONDS 10
#define ONE_SECOND 1 
unsigned int sleepSecs;
// information for softAP configuration
IPAddress local_IP(4,3,2,1);
IPAddress gateway(4,3,2,1);
IPAddress subnet(255,255,255,0);
IPAddress wifiLocalIP;
//Soft Access Point 
const char* ssid     = "Tide_Clock_AP(http://4.3.2.1/)";
const char* password = "";
const char* PARAM_INPUT_1 = "input1";
const char* PARAM_INPUT_2 = "input2";
const char* PARAM_INPUT_3 = "input3";
const char* PARAM_INPUT_4 = "input4";
String inputMessage1;
String inputParam1;
String inputMessage2;
String inputParam2;
String inputParam3;
String inputMessage3;
String inputParam4;
String inputMessage4;

String wifiStatus = "waiting for WiFI to Connect!";
int refreshCount = 0;
String myString;
bool wait = false, connect_now_flag = false;

#define CAMPBELL_RIVER "8074"
String idArray[2];
String nameArray[2];
const char* ntpServer = "pool.ntp.org";



#define LOOP_SECS  120 // 300 secs causes AP server to wait 5 minutes for client to connect
String wifiSSIDplaceHolder = "                                "; //reserve 32 character spaces for each SSID
// setup String array for SSID's
String wifiSSIDs[10]
      = { wifiSSIDplaceHolder, wifiSSIDplaceHolder, wifiSSIDplaceHolder, wifiSSIDplaceHolder, wifiSSIDplaceHolder, wifiSSIDplaceHolder, wifiSSIDplaceHolder, wifiSSIDplaceHolder, wifiSSIDplaceHolder, wifiSSIDplaceHolder };
String Digits[10] = {"1","2","3","4","5","6","7","8","9","10"};
int numNetworks; // number of wifi networks found during scan
// Configure Analog Input to read internal VCC
float volt;

char buffer[80];// for sprintf message buffer
File SSID_File, PASSWORD_File, ZERO_OFFSET_File, STATION_File, TIDE_DATA_File, Last_Fetched_Date_File;
File STATION_TIDE_DATA_File; // Station that Tide_Data goes with
byte tickPin = 4;              // The pin to pulse to tick the clock forward one second
byte tockPin = 17;             // The pin to pulse to tock the clock forward one second
boolean tick;             // Step should "tick" the clock if true, "tock" it if not
#define CLOCK_ZERO (32)
#define CLOCK_POS_PWR (27)  //used on PCB version
#define I2C_SDA 21
#define I2C_SCL 22
RTC_DATA_ATTR unsigned int CLOCK_POSITION_RTC_MEM;


// Create AsyncWebServer object on port 80
    AsyncWebServer APserver(80);
    // Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP); // default uses pool.ntp and no utcOffset may be what is needed for DFO time
//----------------------------------------------------
// Create a client
WiFiClientSecure client;

/******************************************************************************************/
void setup() 
{   
  ConfigureIO();
  Wire.begin(I2C_SDA, I2C_SCL);// Start the I2C interface
  Serial.begin(115200);
  while (!Serial) {}
  delay(500);
  DEBUG_SERIAL.print(F("Reset Reason:"));
  DEBUG_SERIAL.println(esp_reset_reason());
  getBootReasonMessage(bootReasonMessage, BOOT_REASON_MESSAGE_SIZE);
  Serial.println(bootReasonMessage);
  Serial.println("Program Name, Version Date and Time = ");
  Serial.println(PROGRAM_NAME +(String)("  ") +  VERSION_DATE + (String)("  ") + VERSION_TIME + " local time");
  
  if (esp_reset_reason() != ESP_RST_DEEPSLEEP)
  {
    PWR_RST_FLAG = true;
    DEBUG_SERIAL.print("esp_reset_reason = ");
    DEBUG_SERIAL.println(esp_reset_reason());
  }
  if (!rtc.begin() || rtc.lostPower()) 
  {
    DEBUG_SERIAL.println("Couldn't find RTC");
    DEBUG_SERIAL.println("RTC lost power, you need to set the time!");
    RTC_ERR = true;
  }
  
  readFilesFromFlash();

  if(RTC_ERR || PWR_RST_FLAG)
  {
    handlePowerReset(); // this call does not return, goes to deepSleep
  }

  RTCnow  = rtc.now();
  setTime((uint32_t)RTCnow.unixtime()); // set system time
  DEBUG_SERIAL.print(("System time is "));
  DEBUG_SERIAL.print(RTCnow.year(), DEC);
  DEBUG_SERIAL.print('/');
  DEBUG_SERIAL.print(RTCnow.month(), DEC);
  DEBUG_SERIAL.print('/');
  DEBUG_SERIAL.print(RTCnow.day(), DEC);
  DEBUG_SERIAL.print(" ");
  DEBUG_SERIAL.print(RTCnow.hour(),DEC);
  DEBUG_SERIAL.print(':');
  DEBUG_SERIAL.print(RTCnow.minute(),DEC);
  DEBUG_SERIAL.print(':');
  DEBUG_SERIAL.print(RTCnow.second(),DEC);
  DEBUG_SERIAL.println(" UTC");
  DEBUG_SERIAL.print("now() is " + String(year()) + '/' + String(month()) + '/' + String(day()) + ' ' + String(hour()) + ':' + String(minute()) + ':' + String(second()) + '\n');
  /*For debugging, cause a tide data update hour
  if ((now() - Last_Fetched_Date ) > 3600/4)
  {
    GET_TIDE_DATA_FLAG = true;
    Serial.println("**********************************************");
    Serial.println("Expired! Going to get new Tide Data!");
    Serial.println("**********************************************");
  }
  */
  // Is today the 1st of the month? Already fetched new data today?
  if ((RTCnow.day() == 1) && !Check_if_Tide_Data_Already_Fetched())
  {
    GET_TIDE_DATA_FLAG = true;
  }
  // Check if Last Fetched Date is more than 4 weeks old
  if ((now() - Last_Fetched_Date) > (SECS_PER_WEEK * 4))
  {
    GET_TIDE_DATA_FLAG = true;
  }
  if(GET_TIDE_DATA_FLAG)
  {
    handlePowerReset(); // this call does not return, goes to deepSleep
  }
  // Continue to normal operation
  numberOfDataPoints = sizeof(tideHeight)/sizeof(tideHeight[0]);
  ExtractTideDataFromFile();// Extract tide data from File, put in variables
  minuteOfDay = now()/60UL; // turn epoch seconds into epoch minutes
  DEBUG_SERIAL.print(F("minuteOfDay = "));
  DEBUG_SERIAL.println(minuteOfDay);
  CalcHandPosition();
  DEBUG_SERIAL.print(F("savedClock Position from RTC_MEMORY = "));
  DEBUG_SERIAL.println(CLOCK_POSITION_RTC_MEM);
  
  // sanity check, if  ClockFacePosition > 961*1440 must be an error
  if (ClockFacePosition > 961*1440)
  {
    Serial.println("Clock Position calculation error, moving hand to Zero");
    ClockFacePosition = 0; // move and to zero position to indicate error state
    // and to save battery from having to move hand on next error.
  }
  MoveHandToPosition();
  goToDeepSleepNow(SIX_MINUTES); // sleep 10s=10e6, 100s=100e6, 1s=1e6, 60s=60e6, 1h=3600e6
} 

//***************************************************************************************

void loop() 
{  //if deep sleep is working, this code will never run.
  DEBUG_SERIAL.println(F("This shouldn't get printed"));
  delay(1000);
}

//---------------------------------------------------
// get date and time of day from ntp server
// return false if error occurs
bool getDateTimefromNTPserver() 
{
    timeClient.begin();
    bool err = timeClient.update();
    hour0 = timeClient.getHours();
    //2022-03-16T05:41:00Z
    char buf2[] = "YYYY-MM-DDThh:mm:ssZ";
    DEBUG_SERIAL.print("NTP Time is ");
    DEBUG_SERIAL.println(RTCnow.toString(buf2));
    char buf3[] = "YYYY-MM-DD";
    formattedDate = RTCnow.toString(buf2);
    DEBUG_SERIAL.println(formattedDate);
    return err;
}

String processor(const String& var){
  String temp = "";
  if (numNetworks){
    if (numNetworks > 10) {
          numNetworks= 10; // only have space in array for 10 SSID's
        }
      
    for (int i = 0; i < numNetworks; ++i) {
      if(var == ("Option" + Digits[i])){
        temp = WiFi.SSID(i);
      }
    }
  }  
 return temp;
}

String processorConnectingToWiFi(const String& var){
  String temp = "";
  if (var == "inputMessage1")
  {
    temp = inputMessage1;
  }
  if (var == "inputMessage2")
  {
    temp = inputMessage2;
  }
  if (var == "inputMessage3")
  {
    temp = inputMessage3;
  }
  if (var == "inputMessage4")
  {
    temp = inputMessage4;
  }
  if (var == "myString")
  {
    temp = myString;
  }
  if (var == "wifiStatus")
  {
    temp = wifiStatus;
  }  
 return temp;
}


String processorSetUpComplete(const String& var){
  String temp = "";
  if (var == "wifiLocalIP.toString()")
  {
    temp = wifiLocalIP.toString();
  }
 return temp;
}

void Start_AP()
{
    // Start AP server
  WiFi.disconnect();
  DEBUG_SERIAL.print("Setting AP (Access Point)…");
  DEBUG_SERIAL.println("Setting up AP + STA Mode");
  DEBUG_SERIAL.print("Setting soft-AP configuration ... ");
  DEBUG_SERIAL.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ssid);
  //  scan for wifi networks
  DEBUG_SERIAL.println("wifi scan start");

  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  numNetworks = n;
  DEBUG_SERIAL.println("scan done");
  if (n == 0)
  {
    DEBUG_SERIAL.println("no networks found");
  }
  else
  {
    DEBUG_SERIAL.print(n);
    DEBUG_SERIAL.println(" networks found");
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      DEBUG_SERIAL.print(i + 1);
      DEBUG_SERIAL.print(": ");
      DEBUG_SERIAL.print(WiFi.SSID(i));
      DEBUG_SERIAL.print(" (");
      DEBUG_SERIAL.print(WiFi.RSSI(i));
      DEBUG_SERIAL.print(")");
    }
    DEBUG_SERIAL.println(" ");
    IPAddress IP = WiFi.softAPIP();
    DEBUG_SERIAL.print("AP IP address: ");
    DEBUG_SERIAL.println(IP);
  }
  APsetupServer();
  APserver.begin(); 
}

//********************************************************************
void APsetupServer()
{
  APserver.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    if (WiFi.status() != WL_CONNECTED)
    { 
      refreshCount = 0;
      request->send(LittleFS, "/index.html", String(), false, processor);
      DEBUG_SERIAL.println("Client Connected");
    }
    else
    {   
      request->send(200, "text/html", "WiFi is already connected with IP address " + wifiLocalIP.toString() + "<br>Tide Clock setup is complete!");
    }
  });

  // Route to load style.css file
  APserver.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send(LittleFS, "/styles.css", "text/css");
  });

  APserver.onNotFound([](AsyncWebServerRequest *request)
  {
    DEBUG_SERIAL.println("Got to Not Found");
    request->send(404, "text/plain", "The content you are looking for was not found.");
  });
    
  APserver.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) 
  {
    String inputMessage;
    String inputParam;
    refreshCount++;
    myString = String(refreshCount);

    int paramsNr = request->params();
    AsyncWebParameter* p = request->getParam(0);
    inputMessage1 = p->value();
    p = request->getParam(1);
    inputMessage2 = p->value();
    p = request->getParam(2);
    inputMessage3 = p->value();
    p = request->getParam(3);
    inputMessage4 = p->value();
    
    //DEBUG_SERIAL.println("input message1 = " + inputMessage1);
    //DEBUG_SERIAL.println("input message2 = " + inputMessage2);
    //DEBUG_SERIAL.println("input message3 = " + inputMessage3);
    //DEBUG_SERIAL.println("input message4 = " + inputMessage4);

    if ((refreshCount < 4) && (WiFi.status() != WL_CONNECTED))
    {
      connect_now_flag = true;
      request->send(LittleFS, "/connectingToWiFi.htm", String(), false, processorConnectingToWiFi);                                
    }
    else if ((WiFi.status() != WL_CONNECTED) && (refreshCount >= 4))
    {
        request->send(LittleFS, "/timedOut.htm", "text/html" );
    }
    else
    {
      wait = true;
      request->send(LittleFS, "/setUpComplete.htm", String(), false, processorSetUpComplete);
      //request->redirect(wifiLocalIP.toString()); // redirect to connection on local WiFi
    }
  });
}

// ------------------Sub Routines ---------------------
void saveOtherItemsToFlash()
{
  if (ZERO_OFFSET_SAVED != ZERO_OFFSET)
  {
    ZERO_OFFSET_File = LittleFS.open("/ZERO_OFFSET.txt", "w");
    if (ZERO_OFFSET_File)
    {
      DEBUG_SERIAL.println(F("Write ZERO_OFFSET to file"));
      ZERO_OFFSET_File.print(String(ZERO_OFFSET));
      ZERO_OFFSET_File.close();
    }
    else
    {
      DEBUG_SERIAL.println(F("Problem creating ZERO_OFFSET.txt file!"));
    }
  }
  else
  {
    DEBUG_SERIAL.println("ZERO_OFFSET is " + String(ZERO_OFFSET));
    DEBUG_SERIAL.println(F("ZERO_OFFSET has not changed, not overwriting"));
  }

  if (STATION_SAVED != STATION)
    {
      DEBUG_SERIAL.print(F("Write STATION to file, Station number is "));
      DEBUG_SERIAL.println(STATION);
      STATION_File = LittleFS.open("/STATION.txt", "w");
      if (STATION_File)
      {
        STATION_File.print(STATION);
        STATION_File.close();
      }
      else
      {
        DEBUG_SERIAL.println(F("Problem creating STATION.txt file!"));
      }
    }
  else
  {
    DEBUG_SERIAL.println("STATION NUMBER is " + STATION);
    DEBUG_SERIAL.println(F("Station number has not changed, not overwriting"));
  }

  if (Last_Fetched_Date_Saved != Last_Fetched_Date)
  {
    Last_Fetched_Date_File = LittleFS.open("/Last_Fetched_Date.txt", "w");
    if (Last_Fetched_Date_File)
    {
      DEBUG_SERIAL.println(F("Write Last_Fetched_Date to file"));
      Last_Fetched_Date_File.print(Last_Fetched_Date);
      Last_Fetched_Date_File.close();
    }
    else
    {
      DEBUG_SERIAL.println(F("Problem creating Last_Fetched_Date.txt file!"));
    }
  }
  else
  {
    DEBUG_SERIAL.println(F("Last_Fetched_Date has not changed, not overwriting"));
  }  
  if (STATION_TIDE_DATA_SAVED != STATION)
  {
    STATION_TIDE_DATA_File = LittleFS.open("/STATION_TIDE_DATA.txt", "w");
    if (STATION_TIDE_DATA_File)
    {
      DEBUG_SERIAL.println(F("Write Station_Tide_Data to file"));
      STATION_TIDE_DATA_File.print(STATION_TIDE_DATA);
      STATION_TIDE_DATA_File.close();
    }
    else
    {
      DEBUG_SERIAL.println(F("Problem creating STATION_TIDE_DATA.txt file!"));
    }
  }
  else
  {
    DEBUG_SERIAL.println(F("STATION_TIDE_DATA has not changed, not overwriting"));
  }  
}

//***************************************************************************************
bool getStationID(void)
{ // Use Station number to get the Station ID from the DFO web site
  bool err = false;
  DEBUG_SERIAL.println("\nStarting connection to server...");
  client.setInsecure();
  if (!client.connect(DFOserver, 443))
  {
    DEBUG_SERIAL.println("Connection failed!");
  }
  else 
  {
    DEBUG_SERIAL.println("Connected to server!");

    // Make a HTTP request:
    DEBUG_SERIAL.print("Station number is: ");
    DEBUG_SERIAL.println(STATION);
    client.print(String("GET ") + "/api/v1/stations?code=" + STATION  +
               " HTTP/1.1\r\n" +
               "Host: " + DFOserver + "\r\n" +
               "Connection: close\r\n\r\n");

    int timeout = 5 * 10; // 5 seconds
    while (!client.available() && (timeout-- > 0)) {
      delay(100);
    }

    while (client.connected()) 
    {
      String line = client.readStringUntil('\n');
      if (line == "\r") 
      {
        DEBUG_SERIAL.println("headers received");
        break;
      }
    }
    String input;
    while (client.available()) {
      String line = client.readStringUntil('\n');
      //DEBUG_SERIAL.print("DFO line = ");
      //DEBUG_SERIAL.println(line);
      String firstChar = line.substring(0, 1);
      if (firstChar == "[") {
        //DEBUG_SERIAL.println(line);
        input = line;
      }
    }
    client.stop();

    DynamicJsonDocument stationDoc(1600);
    
    DeserializationError error = deserializeJson(stationDoc, input);
    //DEBUG_SERIAL.print("input = ");
    //DEBUG_SERIAL.println(input);
    if (error) {
      DEBUG_SERIAL.print(F("deserializeJson() failed: "));
      DEBUG_SERIAL.println(error.f_str());
    }
    for (JsonObject item : stationDoc.as<JsonArray>()) {
      const char* id = item["id"];// "5cebf1de3d0f4a073c4bb996",
      const char* code = item["code"]; //: "08074",
      const char* officialName = item["officialName"]; //: "Campbell River",
      const char* operating = item["operating"]; //: true,
      const char* latitude = item["latitude"]; //: 50.042,
      const char* longitude = item["longitude"]; //: -125.247,
      const char* type = item["type"]; //: "PERMANENT",
      const char* timeSeries = item["timeSeries"];
    }
    // Get a reference to the root array
    JsonArray arr = stationDoc.as<JsonArray>();
    // Get the number of elements in the array
    int countn = arr.size();
    numberOfDataPoints = countn;
    DEBUG_SERIAL.print("number of elements in JSON array: ");
    DEBUG_SERIAL.println(countn);

    
    for (int i = 0; i < countn; i++) {
      idArray[i] = stationDoc[i]["id"].as<const char*>();
      DEBUG_SERIAL.print(F("id["));
      DEBUG_SERIAL.print(i);
      DEBUG_SERIAL.print(F("]: "));
      DEBUG_SERIAL.println(stationDoc[i]["id"].as<const char*>());
      nameArray[i] = stationDoc[i]["officialName"].as<const char*>();
      DEBUG_SERIAL.print(F("official name = "));
      DEBUG_SERIAL.println(stationDoc[i]["officialName"].as<const char*>());
    }
    StationID = idArray[0]; // set Station ID for DFO request
    DEBUG_SERIAL.print(F("Station ID = "));
    DEBUG_SERIAL.println(StationID);
    err = true; 
  }  
return err;
}
//-----------------------------------------------------------------------------
/* Make an HTTP request to the Fisheries and Oceans Canada (DFO) web service for tide data
*   Make five separate requests for one week of data each. Combine the separate weeks into 
*   one String before deserializing with JSon. If an error occurs for one of weeks, retry once.
*
*/
bool makeDFORequest() 
{
  String DateFromToCopy;
  String inputsMerged ;
  String inputDFO[5];
  int OpeningBracketPosition;
  int ClosingBracketPosition;

  bool return_ok = true;
  DEBUG_SERIAL.print(F("Connecting to "));
  DEBUG_SERIAL.println(DFOserver);
  for (int i=0; i<5; i++)
  {
    client.setInsecure();
    int retries = 6;
    while (!client.connect(DFOserver, httpsPort) && (retries-- > 0)) 
    {
      DEBUG_SERIAL.print(".");
      delay(1000);
    }
    //DEBUG_SERIAL.println();
    if (!client.connected()) 
    {
      DEBUG_SERIAL.println(F("Failed to connect to DFO"));
      client.stop();
      return_ok = false;
    }
    client.setTimeout(API_TIMEOUT);
    
    DateFromToCopy =DateFromTo[i];
    client.print(String("GET ") + Link1 + StationID + Link2 + DateFromToCopy + 
                " HTTP/1.1\r\n" +
                "Host: " + DFOserver + "\r\n" +
                "Connection: close\r\n\r\n");
    int timeout = 5 * 10; // 5 seconds
    while (!client.available() && (timeout-- > 0)) 
    {
      delay(100);
      //Serial.println("DFO client not available");
    }
    while (client.available()) 
    {
      //Serial.println("DFO client is now available");
      String line = client.readStringUntil('\n');
      //Serial.print("line = ");
      //Serial.println(line);
      String firstChar = line.substring(0, 1);
      if (firstChar == "[") 
      {
        inputDFO[i] = line;
        //Serial.printf("inputDFO[%i]= ",i);
        //Serial.println(line);
      }
    }
    if (!client.available()) 
    {
      //DEBUG_SERIAL.println(F("No response from DFO"));
      client.stop();
    }
  }

  //DEBUG_SERIAL.println("\nclosing connection");
  delay(1000);
  client.stop();

  for( int i=0; i<5; i++) // Get rid of closing and open brackets on inputs
  {
    int OpeningBracketPosition = inputDFO[i].indexOf('[');
    inputDFO[i] = inputDFO[i].substring(OpeningBracketPosition+1);
    int ClosingBracketPosition = inputDFO[i].lastIndexOf(']');
    inputDFO[i] = inputDFO[i].substring(0,ClosingBracketPosition);
  }
  

  inputsMerged = "[" + (inputDFO[0]) + "," + inputDFO[1] + "," + inputDFO[2] + "," + inputDFO[3] + "," + inputDFO[4] + "]";
  

  DynamicJsonDocument doc(49152);

  DeserializationError error = deserializeJson(doc, inputsMerged);
  if (error) {
    DEBUG_SERIAL.print(F("deserializeJson() failed: "));
    DEBUG_SERIAL.println(error.f_str());
    return_ok = false;
  }
  else
  {

    for (JsonObject item : doc.as<JsonArray>()) {

      const char* eventDate = item["eventDate"]; // "2022-03-16T05:41:00Z", "2022-03-16T12:22:00Z", ...
      const char* qcFlagCode = item["qcFlagCode"]; // "2", "2", "2", "2", "2", "2", "2", "2", "2", "2", "2"
      float value = item["value"]; // 1.3, 3.9, 2.8, 3.6, 1.3, 3.9, 2.6, 3.7, 1.4, 4, 2.3
      const char* timeSeriesId = item["timeSeriesId"]; // "5d9dd7bb33a9f593161c3e77", ...

    }
    // Get a reference to the root array
    JsonArray arr = doc.as<JsonArray>();
    // Get the number of elements in the array
    int countn = arr.size();
    numberOfDataPoints = countn;
    DEBUG_SERIAL.print("number of elements in JSON array: ");
    DEBUG_SERIAL.println(countn);

    for (i = 0; i < countn; i++) {
      dateTimeHigh[i] = doc[i]["eventDate"].as<const char*>();
      //DEBUG_SERIAL.print(F("eventDate["));
      //DEBUG_SERIAL.print(i);
      //DEBUG_SERIAL.println(F("]: "));
      //DEBUG_SERIAL.println(doc[i]["eventDate"].as<const char*>());
      tideHeight[i] = doc[i]["value"].as<float>();
      //DEBUG_SERIAL.print(F("tide height = "));
      //DEBUG_SERIAL.println(doc[i]["value"].as<float>());
    }
  }  
  return return_ok; // Success!
}

//------------------------------------------------------------------------------------------
// use to convert dateTime of high tides to epoch minutes
time_t tmConvert_t(int YYYY, byte MM, byte DD, byte hh, byte mm, byte ss)
{
 tmElements_t tmSet; 
 tmSet.Year = uint8_t(YYYY - 1970);
 tmSet.Month = MM;
 tmSet.Day = DD;
 tmSet.Hour = hh;
 tmSet.Minute = mm;
 tmSet.Second = ss;
 return makeTime(tmSet);
}

//-----------------------------------------------------------------
int TimeStringToMinutes(String eventDate) {
  index1 = eventDate.indexOf("T"); // find index of "T"
  index2 = eventDate.indexOf("Z"); // find index of "Z"
  index3 = eventDate.indexOf(":"); // find index of 1st ":"
  index4 = eventDate.indexOf(":", index3 + 1); // find index of 2nd ":"

  hourNow = (eventDate.substring(index1 + 1, index1 + 3)).toInt();
  minuteNow = (eventDate.substring(index3 + 1, index3 + 3)).toInt();
  secondNow = (eventDate.substring(index4 + 1, index4 + 3)).toInt();
  minuteTime = hourNow * 60 + minuteNow;
  return minuteTime;
}
//------------------------------------------------------------------
// routine to find index of nearest element to x
int nearest(int x, int myArray[], int elements)
{
  int idx = 0; // by default near first element

  int distance = abs(myArray[idx] - x);
  for (int i = 1; i < elements; i++)
  {
    int d = abs(myArray[i] - x);
    if (d < distance)
    {
      idx = i;
      distance = d;
    }
  }
  return idx;
}

//---------------------------------------------------
//
void extractDateTime() //used to get new tide data, get 5 weeks to have a one week buffer
{

 // Add check for Last_Fetched_Date. If its older than NumWeeks weeks from today its expired.
  
  DEBUG_SERIAL.print("RTCnow is : ");
  uint32_t TTnow = RTCnow.unixtime();
  DEBUG_SERIAL.println(TTnow);
  uint32_t LFD = (uint32_t)Last_Fetched_Date;
  if ((TTnow - LFD) > SECS_PER_WEEK * NumWeeks) // get new tide data every NumWeeks
  {
    Last_Fetched_Date = RTCnow.unixtime();
  }
  
  for (int i=0 ;i < (NumWeeks+1) ; i++) // fetch an extra week for a buffer
  {
    DateFromWeek[i] = Last_Fetched_Date + SECS_PER_WEEK*i - SECS_PER_DAY; // make sure tide data starts from yesterday. 
    DateToWeek[i] = DateFromWeek[i] + SECS_PER_WEEK;
    sprintf(buffer, "%4d-%02d-%02dT00:00:00Z&to=%4d-%02d-%02dT00:00:00Z", year(DateFromWeek[i]), month(DateFromWeek[i]), day(DateFromWeek[i]), year(DateToWeek[i]) , month(DateToWeek[i]) , day(DateToWeek[i]));
    DateFromTo[i] = buffer;
    DEBUG_SERIAL.println(DateFromTo[i]);
  }  
}
//-------------------------------------------------------------------------------------
// find index of max value in an array
int findIndexOfMaxValue(float flArray[], int numberOfElements) 
{
  int index = 0;
  int i;
  
  float maxVal = flArray[0];
/*
  DEBUG_SERIAL.println(numberOfElements);
  for(i=0;i < numberOfElements; i++)
  {
    DEBUG_SERIAL.print("flArray[] = ");
    DEBUG_SERIAL.println(flArray[i]);
  }
*/

  for (i = 0; i < numberOfElements; i++) {
    if (flArray[i] > maxVal) 
    {
      maxVal = flArray[i];
      /*
      DEBUG_SERIAL.print("maxVal so far =");
      DEBUG_SERIAL.println(maxVal);
      DEBUG_SERIAL.print("index of maxval so far is ");
      DEBUG_SERIAL.println(i);
      */
      index = i;
    }
  }
  DEBUG_SERIAL.print("The maximum value of the array is: "); DEBUG_SERIAL.println(maxVal);
  DEBUG_SERIAL.print("The index of the maximum value of the array is: "); DEBUG_SERIAL.println(index);
  return index;
}


//------------------------------------------------------------------------------------------
// search timeHigh array and return index of element closest to minuteOfDay. number of elements in array is numberOfDataPoints
int closest(unsigned long minuteOfDay, unsigned long *localArray, int numberOfElements) 
{
  int index = 0;
  unsigned long diff1 = 0;
  signed long tmp = minuteOfDay - localArray[0]; // issue with abs() handling operations within brackets??
  unsigned long diff2 = abs(tmp); // seed diff2
  for (int n = 0; n < numberOfElements; n++) {
    tmp = minuteOfDay - localArray[n];
    diff1 = abs(tmp);
    if (diff1 <= diff2) {
      diff2 = diff1;
      index = n;
    }
  }
  return index;
}

//-------------------------------------------------------
//
void CalcHandPosition() 
{
  unsigned long minutesFromHighTide;
  unsigned long minutesFromLowTide;
  unsigned long highTideMinutes[75];  // high tide times only
  unsigned long lowTideMinutes[75]; // low tide times only
  
  bool highTideClosest;
  bool lowTideClosest ;
  numberOfDataPoints = sizeof(tideHeight)/sizeof(tideHeight[0]);
  int indexOfMax = findIndexOfMaxValue(tideHeight, numberOfDataPoints );
  DEBUG_SERIAL.print("indexofMax = ");
  DEBUG_SERIAL.println(indexOfMax);
  // Fill one array with time of each high tides, another with low tides times
  if ((indexOfMax & 1) == 0) { // test if index is even
    for (int i = 0; i < 75; i++) {
      highTideMinutes[i] = timeHigh[i * 2];
      lowTideMinutes[i]  = timeHigh[i * 2 + 1];
    }
  }
  else {
    for (int i = 0; i < 75; i++) { // index is odd
      highTideMinutes[i] = timeHigh[i * 2 + 1];
      lowTideMinutes[i]  = timeHigh[i * 2];
    }
  }
  /*
  for (i = 0; i < 75; i++) 
  {
    DEBUG_SERIAL.print(F("highTideMinutes["));
    DEBUG_SERIAL.print(i);
    DEBUG_SERIAL.print(F("] = "));
    DEBUG_SERIAL.println(highTideMinutes[i]);
    DEBUG_SERIAL.print(F("lowTideMinutes["));
    DEBUG_SERIAL.print(i);
    DEBUG_SERIAL.print(F("] = "));
    DEBUG_SERIAL.println(lowTideMinutes[i]);
  }
  */
  int idx = closest(minuteOfDay, timeHigh, numberOfDataPoints);//timeHigh[] contains both high and low tide times
  DEBUG_SERIAL.print("index to nearest time is ");
  DEBUG_SERIAL.println(idx);
  // is minute closest to low or high tide?
  // indexOfMax points to a high tide time, is odd or even, match closest idx?
  if ((((indexOfMax & 1) == 0) && ((idx & 1) == 0)) || (((indexOfMax & 1) == 1) && ((idx & 1) == 1))) { //test for any high tide index matches idx
    // idx points to a high tide
    highTideClosest = true;
    lowTideClosest =  false;
  }
  else {
    // idx points to low tide
    highTideClosest  = false;
    lowTideClosest   = true;
  }
  //Is minuteOfDay before or after closest tide (low or high)?
  char pos = 'b';
  if (minuteOfDay >= timeHigh[idx]) {
    pos = 'a';
  }
  else {
    pos = 'b';
  }
  if (highTideClosest) {
    if (pos == 'b') { // high tide at 24 hour mark
      minutesFromHighTide = timeHigh[idx] - minuteOfDay;
      DEBUG_SERIAL.print(F("minutesFromHighTide6 = "));
      DEBUG_SERIAL.println(minutesFromHighTide);
      ClockFacePosition = (1440 - minutesFromHighTide) * 961; // convert to ClockFacePosition in partial ticks
    }
    else { //must be after
      minutesFromHighTide = minuteOfDay - timeHigh[idx];
      DEBUG_SERIAL.print(F("minutesFromHighTide7 = "));
      DEBUG_SERIAL.println(minutesFromHighTide);
      ClockFacePosition = minutesFromHighTide * 961;// convert to ClockFacePosition in partial ticks
    }
  }
  if (lowTideClosest) 
  {
    if (pos == 'b') 
    { // low tide at 12 hour mark
      minutesFromLowTide = timeHigh[idx] - minuteOfDay;
      DEBUG_SERIAL.print(F("minutesFromLowTide8 = "));
      DEBUG_SERIAL.println(minutesFromLowTide);
      //ClockFacePosition = 30 - minutesFromLowTide/MINUTES_PER_CLOCK_FACE_HOUR;// convert to ClockFacePosition
      ClockFacePosition = (720 - minutesFromLowTide) * 961; // convert to ClockFacePosition in partial ticks
    }
    else 
    { // must be after
      minutesFromLowTide = minuteOfDay - timeHigh[idx];
      DEBUG_SERIAL.print(F("minutesFromLowTide9 = "));
      DEBUG_SERIAL.println(minutesFromLowTide);
      //ClockFacePosition = 30 + minutesFromLowTide/MINUTES_PER_CLOCK_FACE_HOUR;// convert to ClockFacePosition
      ClockFacePosition = (minutesFromLowTide + 720) * 961; // convert to ClockFacePosition in partial ticks

    }
  }
  DEBUG_SERIAL.print(F("before or after (high or low) tide? :"));
  DEBUG_SERIAL.println(pos);
  DEBUG_SERIAL.print(F(" 0 = low, 1 = high, high or low ? = " ));
  DEBUG_SERIAL.println(highTideClosest);
}
//-----------------------------------------------
// Lavet Motor Driver
// Step clock forward one tick
void step() 
{
  if (tick) 
  {
    //gpio_set_direction(GPIO_NUM_4, GPIO_MODE_OUTPUT);
    //gpio_set_level(GPIO_NUM_4, HIGH);
    digitalWrite(tickPin, HIGH);            // Issue a tick pulse
    delay(LAVET_PULSE_DURATION);
    //gpio_set_level(GPIO_NUM_4, LOW);
    digitalWrite(tickPin, LOW);
  } 
  else 
  {
    //gpio_set_direction(GPIO_NUM_17, GPIO_MODE_OUTPUT);
    //gpio_set_level(GPIO_NUM_17, HIGH);
    digitalWrite(tockPin, HIGH);            // Issue a tock pulse
    delay(LAVET_PULSE_DURATION);
    //gpio_set_level(GPIO_NUM_17, LOW);
    digitalWrite(tockPin, LOW);
  }
  delay(LAVET_STEP_INTERVAL);             //   Give the Lavet motor time to settle first
  tick = !tick;                     // Switch from tick to tock or vice versa
}

//-------------------------------------------------------------
//
void MoveHandToZeroPosition() // When a Power On Reset is detected, Hand may in unknown position!
{
  int s =0,z,analogInput;
  DEBUG_SERIAL.println("Moving Hand 250 steps to indicate life");
  s = 250;
  while (s > 0)
  {
    step();
    s--;
  }
  digitalWrite(CLOCK_POS_PWR, LOW); // turn on detection circuit
  delay(100);
  DEBUG_SERIAL.println("Moving Hand to Zero Position");
  DEBUG_SERIAL.print("AnalogRead is ");
  analogInput = analogRead(CLOCK_ZERO);
  DEBUG_SERIAL.println(analogInput);
    while (analogInput > ANALOG_THRESHOLD)
    {
      /*
      DEBUG_SERIAL.println(z);
      DEBUG_SERIAL.print("digitalRead is ");
      DEBUG_SERIAL.println(digitalRead(CLOCK_ZERO));
      DEBUG_SERIAL.print("CLOCK_ZERO is ");
      DEBUG_SERIAL.println(CLOCK_ZERO);
      */
      step();
      DEBUG_SPRINTF(buffer,"s =  %i , analogInput = %i", s,analogInput);
      DEBUG_SERIAL.println(buffer);
      s++;
      analogInput = analogRead(CLOCK_ZERO);
    }
  digitalWrite(CLOCK_POS_PWR, HIGH); // turn off detection circuit
  // move hand ahead to zero position
  s = ZERO_OFFSET;
  while (s > 0)
  {
    step();
    s--;
  }
  CLOCK_POSITION_RTC_MEM = 0;
}
 //-------------------------------------------------------
  //
void MoveHandToPosition()
  {
    unsigned long numberOfTicks, numberOfTicksToZero;
    unsigned long n;
    unsigned long CFP = ClockFacePosition;
    DEBUG_SERIAL.print(F("CFP in partial ticks out of 961*1440 = "));
    DEBUG_SERIAL.println(CFP);
    signed long diff = CFP - CLOCK_POSITION_RTC_MEM;
    numberOfTicksToZero = 0;
    
    if (diff < 0) 
    {
        numberOfTicksToZero = MECHANISM_TICKS_PER_REVOLUTION - CLOCK_POSITION_RTC_MEM/1440;
        DEBUG_SERIAL.print(F("number of ticks to zero = "));
        DEBUG_SERIAL.println(numberOfTicksToZero);
        for (n = 1; n <= numberOfTicksToZero; n++) 
        {
            step();  // Step clock forward one tick
        }
        // Check if Hand at Zero position.
        // if not, move hand to zero
        
        MoveHandToZeroPosition();
    }
    numberOfTicks = (CFP-CLOCK_POSITION_RTC_MEM)/1440;
    DEBUG_SERIAL.print(F("diff = "));
    DEBUG_SERIAL.println(diff);

    DEBUG_SERIAL.print(F("number of ticks = "));
    DEBUG_SERIAL.println(numberOfTicks);
    for (n = 1; n <= numberOfTicks; n++) 
    {
      step();  // Step clock forward one tick
      
      if ((n % 40) == 0) 
      {
        DEBUG_SERIAL.println (F(" clock advanced one hour (24 hr clock face)"));
      }
      
    }
    
    CLOCK_POSITION_RTC_MEM = CLOCK_POSITION_RTC_MEM + (numberOfTicks * 1440);
    DEBUG_SERIAL.print(F("new CLOCK_POSITION_RTC_MEM to be SAVED = "));
    DEBUG_SERIAL.println(CLOCK_POSITION_RTC_MEM);
 }

 //-----------------------------------------------------------------------------------------
// Enter deepSleep for n seconds
void goToDeepSleepNow(int n) 
{
  if (!DeepSleepEnable)
  {
    n = 10; // if testing, only deepsleep for 10 secs
  }
  DEBUG_SERIAL.print(F("deepsleepnow for "));
  DEBUG_SERIAL.print(n);
  DEBUG_SERIAL.println(F(" seconds"));
  if (DeepSleepEnable)
  {
  ESP.deepSleep(n * 1e6); //sleep 10s=10e6, 100s=100e6, 1s=1e6, 60s=60e6, 1h=3600e6
  }
  else
  {
    delay(n *1000);
  }
}

//----------------------------------------------------
void printLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo))
  {
    DEBUG_SERIAL.println("Failed to obtain time");
    return;
  }
  DEBUG_SERIAL.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  DEBUG_SERIAL.print("Day of week: ");
  DEBUG_SERIAL.println(&timeinfo, "%A");
  DEBUG_SERIAL.print("Month: ");
  DEBUG_SERIAL.println(&timeinfo, "%B");
  DEBUG_SERIAL.print("Day of Month: ");
  DEBUG_SERIAL.println(&timeinfo, "%d");
  DEBUG_SERIAL.print("Year: ");
  DEBUG_SERIAL.println(&timeinfo, "%Y");
  DEBUG_SERIAL.print("Hour: ");
  DEBUG_SERIAL.println(&timeinfo, "%H");
  DEBUG_SERIAL.print("Hour (12 hour format): ");
  DEBUG_SERIAL.println(&timeinfo, "%I");
  DEBUG_SERIAL.print("Minute: ");
  DEBUG_SERIAL.println(&timeinfo, "%M");
  DEBUG_SERIAL.print("Second: ");
  DEBUG_SERIAL.println(&timeinfo, "%S");

  DEBUG_SERIAL.println("Time variables");
  char timeHour[3];
  strftime(timeHour,3, "%H", &timeinfo);
  DEBUG_SERIAL.println(timeHour);
  char timeWeekDay[10];
  strftime(timeWeekDay,10, "%A", &timeinfo);
  DEBUG_SERIAL.println(timeWeekDay);
  DEBUG_SERIAL.println();
}

/**********************************************************/
void saveWiFiCredentials() // save to Flash
{
  if (SSID_SAVED != SSID)
  {
    SSID_File = LittleFS.open("/SSID.txt", "w");
    if (SSID_File)
    {
      DEBUG_SERIAL.println(F("Write SSID to file"));
      SSID_File.print(inputMessage1);
      SSID_File.close();
    }
    else
    {
      DEBUG_SERIAL.println(F("Problem creating SSID.txt file!"));
    }
  }
  else
  {
    DEBUG_SERIAL.println(F("SSID is the same, not overwriting file!"));
  }
  if (PASSWORD_SAVED != PASSWORD)
  {
    PASSWORD_File = LittleFS.open("/PASSWORD.txt", "w");
    if (PASSWORD_File)
    {
      DEBUG_SERIAL.println(F("Write PASSWORD to file"));
      PASSWORD_File.print(inputMessage2);
      PASSWORD_File.close();  
    }
    else
    {
      DEBUG_SERIAL.println(F("Problem creating PASSWORD.txt file!"));
    }
  }
  else
  {
    DEBUG_SERIAL.println(F("PASWORD is the same, not overwriting file!"));
  }
  
}
//***********************************************************************
void readFilesFromFlash()
{
  DEBUG_SERIAL.println(F("Inizializing FS..."));
  if (LittleFS.begin())
  {
    DEBUG_SERIAL.println(F("Initializing FS OK"));
  } 
  else 
  {
    DEBUG_SERIAL.println(F("failed initializing FS"));
    FLASH_ERR_FLAG = true;
  }

  SSID_File = LittleFS.open("/SSID.txt", "r");
  if (SSID_File) 
  {
    DEBUG_SERIAL.println(F("Read SSID_file content!"));
    SSID = SSID_File.readString();
    SSID_SAVED = SSID;
    DEBUG_SERIAL.println("SSID : " + SSID);
    SSID_File.close();
  } 
  else 
  {
    DEBUG_SERIAL.println(F("Problem reading SSID file!"));
  }

  PASSWORD_File = LittleFS.open("/PASSWORD.txt", "r");
  if (PASSWORD_File) 
  {
    DEBUG_SERIAL.println(F("Read PASSWORD_file content!"));
    PASSWORD = PASSWORD_File.readString();
    PASSWORD_SAVED = PASSWORD;
    DEBUG_SERIAL.println("PASSWORD : " + PASSWORD);
    PASSWORD_File.close();
  } 
  else 
  {
    DEBUG_SERIAL.println(F("Problem reading Password file!"));
  }
  
  ZERO_OFFSET_File = LittleFS.open("/ZERO_OFFSET.txt","r");
  if (ZERO_OFFSET_File) 
  {
    DEBUG_SERIAL.println(F("Read ZERO_OFFSET_file content!"));
    String tempString = ZERO_OFFSET_File.readString();
    ZERO_OFFSET = tempString.toInt();
    if (ZERO_OFFSET > ZERO_OFFSET_MAX)
    {
      ZERO_OFFSET = ZERO_OFFSET_MAX; // sanity check
    }
    ZERO_OFFSET_SAVED = ZERO_OFFSET;
    DEBUG_SERIAL.println("ZERO_OFFSET : " + String(ZERO_OFFSET));
    ZERO_OFFSET_File.close();
  } 
  else 
  {
    DEBUG_SERIAL.println(F("Problem reading ZERO_OFFSET file!"));
    ZERO_OFFSET = 0; // default to 0
  }
  STATION_File = LittleFS.open("/STATION.txt","r");
  if (STATION_File) 
  {
    DEBUG_SERIAL.println(F("Read STATION_file content!"));
    String tempString = STATION_File.readString();
    STATION = tempString;
    if (STATION.length() < 4)
    {
      STATION = STATION_DEFAULT; // default to Campbell River
    }
    STATION_SAVED = STATION;
    DEBUG_SERIAL.println("STATION : " + STATION);
    STATION_File.close();
  } 
  else 
  {
    DEBUG_SERIAL.println(F("Problem reading STATION file!"));
    STATION = CAMPBELL_RIVER; // default 
  }
  TIDE_DATA_File = LittleFS.open("/TIDE_DATA.txt","r");
  if (TIDE_DATA_File)
  {
    String tempString = "";
    int i =0;
    int arrayLength = 150;
    while (TIDE_DATA_File.available() && (i <=arrayLength)) 
    {
      tempString = TIDE_DATA_File.readStringUntil('\n');
      dateTimeHigh[i] = tempString;
      tempString = TIDE_DATA_File.readStringUntil('\n');
      tideHeight[i] = tempString.toFloat();
      //DEBUG_SERIAL.print("tideHeight[] is : ");
      //DEBUG_SERIAL.println(tideHeight[i]);
      i++;
    }
    DEBUG_SERIAL.println(F("Read TIDE_DATA_file content!")); 
  }
  TIDE_DATA_File.close();

  STATION_TIDE_DATA_File = LittleFS.open("/STATION_TIDE_DATA.txt","r");
  if (STATION_TIDE_DATA_File)
  {
    DEBUG_SERIAL.println(F("Read STATION_TIDE_DATA content!"));
    String tempString = STATION_TIDE_DATA_File.readString();
    STATION_TIDE_DATA_SAVED = tempString;
    STATION_TIDE_DATA = STATION_TIDE_DATA_SAVED;
    DEBUG_SERIAL.println("STATION_TIDE_DATA : " + STATION_TIDE_DATA);
    STATION_TIDE_DATA_File.close();
  }  

  Last_Fetched_Date_File = LittleFS.open("/Last_Fetched_Date.txt","r");
  if (Last_Fetched_Date_File)
  {
    DEBUG_SERIAL.println(F("Read Last_Fetched_Date_File content!"));
    String tempString = Last_Fetched_Date_File.readString();
    Last_Fetched_Date_Saved = tempString.toInt();
    Last_Fetched_Date = Last_Fetched_Date_Saved;
    DEBUG_SERIAL.println("Last_Fetched_Date : " + String(Last_Fetched_Date));
    DEBUG_SPRINTF(buffer,"Last_Fetched_Date is %i/%i/%i", year(Last_Fetched_Date),month(Last_Fetched_Date),day(Last_Fetched_Date));
    DEBUG_SERIAL.println(buffer);
    DEBUG_SPRINTF(buffer,"Last_Fetched_Date-Time is %i:%i:%i UTC", hour(Last_Fetched_Date),minute(Last_Fetched_Date),second(Last_Fetched_Date));
    DEBUG_SERIAL.println(buffer);
    Last_Fetched_Date_File.close();
  }
}

//---------------------------------------------------
void setRTCTime()
{ 
  byte theYear, theMonth, theDate, theHour, theMinute, theSecond ;
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo))
  {
   DEBUG_SERIAL.println("Failed to obtain time");
    return;
  }
  
  // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  rtc.adjust(DateTime(2024, 1, 9, 14, 55, 0));
}
//*********************************************************
void saveTideDataToFile() // 
{
  int arrayLength = 150;
  String TideDataFileString = "";
  for (int i=0;i<arrayLength;i++)
  {
    TideDataFileString += dateTimeHigh[i] + "\n";
    TideDataFileString += String(tideHeight[i],2) + "\n";
  }
  //DEBUG_SERIAL.print("TideDataFileString is ");
  //DEBUG_SERIAL.println(TideDataFileString);
  // write Tide Data to littleFS
  TIDE_DATA_File = LittleFS.open("/TIDE_DATA.txt","w");
  if (TIDE_DATA_File)
  {
    DEBUG_SERIAL.println(F("Ready to write TIDE_DATA_file content!")); 
    TIDE_DATA_File.print(TideDataFileString);
    TIDE_DATA_File.close(); 
    Last_Fetched_Date = now();
    STATION_TIDE_DATA = STATION; // Station that Tide data is for.
    DEBUG_SERIAL.print("Last_Fetched_Date is ");
    DEBUG_SERIAL.println(Last_Fetched_Date);
  }
  TIDE_DATA_File.close();
}
//************************************************************
void ExtractTideDataFromFile()
{
  int YYYY;  byte MM; byte DD; byte hh; byte mm; byte ss;
  String tmp;
  DEBUG_SERIAL.print(F("number of data points = "));
  DEBUG_SERIAL.println(numberOfDataPoints);
  // dateTimeHigh[] contains dateTime string
  // timeHigh[] contains epoch minutes
  // tideHeight[] contains value, tide height as float
  for (i = 0; i < numberOfDataPoints; ++i) 
  {
    // convert dateTime to epoch minutes and store in array timeHigh[]
    tmp = dateTimeHigh[i].substring(0, 4);
    YYYY = tmp.toInt();
    tmp = dateTimeHigh[i].substring(5, 7);
    MM = tmp.toInt();
    tmp = dateTimeHigh[i].substring(8, 10);
    DD = tmp.toInt();
    tmp = dateTimeHigh[i].substring(11, 13);
    hh = tmp.toInt();
    tmp = dateTimeHigh[i].substring(14, 16);
    mm = tmp.toInt();
    tmp = dateTimeHigh[i].substring(17, 19);
    ss = tmp.toInt();
    // convert dateTime to epoch minutes
    unsigned long temp_seconds = tmConvert_t( YYYY,  MM,  DD,  hh,  mm,  ss);
    timeHigh[i] = (temp_seconds / 60UL); //convert seconds to minutes
    //DEBUG_SERIAL.print(F("timeHigh in epochminutes = "));
    //DEBUG_SERIAL.println(timeHigh[i]);
    //
  }
  // simple check for valid data from parsing
  if ((numberOfDataPoints == 0) || (numberOfDataPoints > 150)) 
  {
    DEBUG_SERIAL.print(F("number of DataPoints = "));
    DEBUG_SERIAL.println(numberOfDataPoints);
  }
}
//***********************************************************
void GetMonthOfTideData()
{
  bool errReturn = false;
  extractDateTime(); // need special formatted time for DFO request
  errReturn = getStationID();
  if (errReturn == false)
  {
    DEBUG_SERIAL.println(F("error in getStationID()"));
  }
  // if error in getting station ID don't bother making DFO request
  if (errReturn == true)
  {
    errReturn = makeDFORequest();
    if (errReturn == false)
    {
      DEBUG_SERIAL.println(F("error in makeDFORequest()"));
    }
    if (errReturn == true)
    {
    saveTideDataToFile(); // save monthly tide data to file
    }
  }
}
//***********************************************************************
void ConfigureIO()
{
  pinMode(CLOCK_ZERO, INPUT_PULLUP); // set gpioX for input with internal pullup resistor enabled
  pinMode(CLOCK_POS_PWR, OUTPUT); 
  digitalWrite(CLOCK_POS_PWR, LOW);
  digitalWrite(CLOCK_POS_PWR, HIGH); // turn off detection circuit

  // setup for Lavet Motor Driver ----------------------
  tick = true;                      // Start with a "tick"
  pinMode(tickPin, OUTPUT);               // Set the tick pin of the clock's Lavet motor to OUTPUT,
  digitalWrite(tickPin, LOW);               //   with a low-impedance path to groundmove
  pinMode(tockPin, OUTPUT);               // Set the tock pin of the clock's Lavet motor to OUTPUT,
  digitalWrite(tockPin, LOW);
}
/*****************************************************************/
bool Check_if_Tide_Data_Already_Fetched()
{
  bool fetched =false;
  if (year(Last_Fetched_Date) == RTCnow.year())
  {
    if (month(Last_Fetched_Date) == RTCnow.month())
    {
      if(day(Last_Fetched_Date) == RTCnow.day())
      {
        fetched = true;
      }
    }
  }
  return fetched;
}

void getBootReasonMessage(char *buffer, int bufferlength)
{
 #if defined(ARDUINO_ARCH_ESP32)

    esp_reset_reason_t reset_reason = esp_reset_reason();

    switch (reset_reason)
    {
    case ESP_RST_UNKNOWN:
        snprintf(buffer, bufferlength, "Reset reason can not be determined");
        break;
    case ESP_RST_POWERON:
        snprintf(buffer, bufferlength, "Reset due to power-on event");
        break;
    case ESP_RST_EXT:
        snprintf(buffer, bufferlength, "Reset by external pin (not applicable for ESP32)");
        break;
    case ESP_RST_SW:
        snprintf(buffer, bufferlength, "Software reset via esp_restart");
        break;
    case ESP_RST_PANIC:
        snprintf(buffer, bufferlength, "Software reset due to exception/panic");
        break;
    case ESP_RST_INT_WDT:
        snprintf(buffer, bufferlength, "Reset (software or hardware) due to interrupt watchdog");
        break;
    case ESP_RST_TASK_WDT:
        snprintf(buffer, bufferlength, "Reset due to task watchdog");
        break;
    case ESP_RST_WDT:
        snprintf(buffer, bufferlength, "Reset due to other watchdogs");
        break;
    case ESP_RST_DEEPSLEEP:
        snprintf(buffer, bufferlength, "Reset after exiting deep sleep mode");
        break;
    case ESP_RST_BROWNOUT:
        snprintf(buffer, bufferlength, "Brownout reset (software or hardware)");
        break;
    case ESP_RST_SDIO:
        snprintf(buffer, bufferlength, "Reset over SDIO");
        break;
    }

    if (reset_reason == ESP_RST_DEEPSLEEP)
    {
        esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

        switch (wakeup_reason)
        {
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            snprintf(buffer, bufferlength, "In case of deep sleep: reset was not caused by exit from deep sleep");
            break;
        case ESP_SLEEP_WAKEUP_ALL:
            snprintf(buffer, bufferlength, "Not a wakeup cause: used to disable all wakeup sources with esp_sleep_disable_wakeup_source");
            break;
        case ESP_SLEEP_WAKEUP_EXT0:
            snprintf(buffer, bufferlength, "Wakeup caused by external signal using RTC_IO");
            break;
        case ESP_SLEEP_WAKEUP_EXT1:
            snprintf(buffer, bufferlength, "Wakeup caused by external signal using RTC_CNTL");
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            snprintf(buffer, bufferlength, "Wakeup caused by timer");
            break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            snprintf(buffer, bufferlength, "Wakeup caused by touchpad");
            break;
        case ESP_SLEEP_WAKEUP_ULP:
            snprintf(buffer, bufferlength, "Wakeup caused by ULP program");
            break;
        case ESP_SLEEP_WAKEUP_GPIO:
            snprintf(buffer, bufferlength, "Wakeup caused by GPIO (light sleep only)");
            break;
        case ESP_SLEEP_WAKEUP_UART:
            snprintf(buffer, bufferlength, "Wakeup caused by UART (light sleep only)");
            break;
        }
    }
    else
    {
        snprintf(buffer, bufferlength, "Unknown reset reason %d", reset_reason);
    }
 #endif
}

/*********************************************************************************************
 * This call does not return, ends in DeepSleep. All WiFi functions are handled and kept separate from 
 * daily routine operation. Downloading tide data, checking NTP time and updating RTC andstarting Webserver
 * to obtain configuration data from User. Even if the battery power is good, this routine will run each
 * month because the Get_Tide_Data will get set.
 * Every time this routine runs it will update the RTC, get the latest tide data, and will run the webserver
 * in case the User wants to change the configuration.
 * First, the clock hand will go to zero.
 * Second, the Web Server will start to get the configuration.
 * Third, with the configuration data, the Time can be set through NTP
 * Fourth, with the correct Date-Time, the tide data can be downloaded.
 * Finally, the data can be saved in Flash, then the system will go to a short
 * DeepSleep. When the system wakens, the routine daily operation should begin normally
 * with no errors.
 * 
**********************************************************************************************/
void handlePowerReset()
{
  int loopCounter = LOOP_SECS; // about 1 sec per loop

  MoveHandToZeroPosition();
  Start_AP();
  // wait about 2 minutes for AP server to complete
  while ((loopCounter > 0) && !wait)
  {
    if (connect_now_flag)
    {
      connect_now_flag = false; // reset to allow user 2nd chance to make connection before timeout.
      // try to connect to wifi with new credentials
      Serial.print("STA SSID:= ");
      Serial.println(inputMessage1);
      Serial.print("STA Passwd = ");
      Serial.println(inputMessage2);
      if (inputMessage1 == "")
      {
        inputMessage1 = SSID; // if blank, try the saved SSID
      }
      if (inputMessage2 == "")
      {
        inputMessage2 = PASSWORD; // if blank, try saved Password
      }
      WiFi.begin(inputMessage1.c_str(), inputMessage2.c_str());
      if (WiFi.waitForConnectResult() != WL_CONNECTED) 
      {
        Serial.printf("\nConnection status: %d\n", WiFi.status());
        wifiStatus = "WiFI Not Connected!";
        Serial.println(F("WiFi Failed!")); 
      } 
      else
      {
        wifiStatus = "Wifi Connected with IP address " + WiFi.localIP().toString() +"<br>Tide Clock Setup is complete!!";                         
        Serial.print("Router Assigned IP Address: ");
        wifiLocalIP = WiFi.localIP();
        Serial.println(WiFi.localIP());
        delay(5000); // delay awhile to let web pages update with success or fail message
      }
    }
  delay(1000);
  loopCounter--; 
  }
  
  delay(3000); 
  if (wait) // wait is set in APserver routine
  {
    delay(2000); // delay here to make sure last message is sent to user over wifi.
    if (inputMessage1 != "")
    {
      SSID = inputMessage1;
    }
    if (inputMessage2 != "")
    {
      PASSWORD = inputMessage2;
    }
    if (inputMessage3 != "")
    {
      ZERO_OFFSET = atoi(inputMessage3.c_str());
      if (ZERO_OFFSET > ZERO_OFFSET_MAX)
      {
        ZERO_OFFSET = ZERO_OFFSET_MAX;
      }
    }
    if (inputMessage4 != "")
    {
      STATION = inputMessage4;
    }
  }
  if (FLASH_ERR_FLAG)
  {
    DEBUG_SERIAL.println("Flash Error");
  }

  // If AP server timed out without connecting to WiFi, try
  // using the saved SSIS and password
  if (WiFi.status() != WL_CONNECTED)
  {
   WiFi.disconnect();
   WiFi.mode(WIFI_STA); 
   WiFi.begin(SSID, PASSWORD);
   if (WiFi.waitForConnectResult() != WL_CONNECTED)
   {
    DEBUG_SERIAL.print("Could not connect to WiFi with SSID and PASSWORD ");
   } 
  }
  
  // Update RTC from NTP
  if (WiFi.status() == WL_CONNECTED)
  {
    if(getDateTimefromNTPserver()) // true if update successful
    {
      DEBUG_SERIAL.print("Formatted time after NTP update: ");
      DEBUG_SERIAL.println(timeClient.getFormattedTime());
      unsigned long EpochTime = timeClient.getEpochTime();
      DEBUG_SERIAL.print("epochTime = ");
      DEBUG_SERIAL.println(EpochTime);
      //print system time from timeLib
      setTime(timeClient.getEpochTime());
      DEBUG_SERIAL.print("System Time UTC = ");
      DEBUG_SERIAL.println(now());
      DEBUG_SPRINTF(buffer,"System Date is %i/%i/%i", year(),month(),day());
      DEBUG_SERIAL.println(buffer);
      DEBUG_SPRINTF(buffer,"System Time is %i:%i:%i UTC", hour(),minute(),second());
      DEBUG_SERIAL.println(buffer);
      //set the RTC
      rtc.adjust(DateTime(year(), month(), day(),hour(), minute(), second()));
      RTCnow  = rtc.now();
      DEBUG_SPRINTF(buffer,"RTC Date and Time is %i/%i/%i %i:%i:%i UTC" , RTCnow.year(),RTCnow.month(),RTCnow.day(),RTCnow.hour(),RTCnow.minute(),RTCnow.second());
      DEBUG_SERIAL.println(buffer);
      DEBUG_SERIAL.print(daysOfTheWeek[RTCnow.dayOfTheWeek()]);
      DEBUG_SERIAL.println(") ");
    }
    else
    {
      DEBUG_SERIAL.println("NTP Time Update failed");
    }
    

    GetMonthOfTideData(); // also saves it to flash
    saveWiFiCredentials(); // only save if WiFi connection was successful
    saveOtherItemsToFlash();// only save if WiFi connection was successful
  }
  WiFi.disconnect();
  goToDeepSleepNow(ONE_SECOND);
}
