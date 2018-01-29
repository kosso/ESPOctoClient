/*
  ESPOctoClient v0.1   - @kosso : Jan 28, 2018
  --------------------------------------------

  A basic Wifi client to connect to a local OctoPrint system running on a Raspberry Pi 
  and make requests to the OctoPrint REST API.

  See API docs: http://docs.octoprint.org/en/master/api/

  Some test request exmaple methods are down at the bottom of the sketch.
 
 */


#include <ArduinoJson.h>
// Support ESP32 and ESP8266 
#ifdef ARDUINO_ARCH_ESP8266
#include "ESP8266WiFi.h"
#else
#include <WiFi.h>
#endif

// ##### SETTINGS

// Wifi Settings.
const char* ssid = "****WIFI-SSID****";
const char* password = "****WIFI-PASSWORD****";

// Octoprint Settings
String    OCTO_API_KEY = "**********************************";  // Via: OctoPrint > Settings > API
int       OCTO_PORT = 5000;
String    OCTO_HOST = "192.168.0.44";

// Printer status update interval
long interval = 6000; // Every 6 seconds

// Test button for sending g-code commands etc. 
const int buttonPin = 0; // Boot button for now (ESP32 Dev Board (& Wemos Lolin)). Boards without an extra button will need a breadboard etc. 

// ##### END SETTINGS

//////////////////////////////////////////////////////////////////////////////// 
WiFiClient client;
String ipStr = "";
// Button press detection
int buttonState = 0;
int lastButtonState = HIGH;   // the previous reading from the input pin
// Debounce
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers
// For status request timer.
long previousMillis = 0;    

/////////////////////////////////////////////////////////////////////////////////

void setup() {

  // initialize the pushbutton pin as an input:
  pinMode(buttonPin, INPUT);
  
  Serial.begin(115200);
  Serial.println();
  Serial.println("....");
 
  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // Print the IP address
  IPAddress ip = WiFi.localIP();
  
  ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
  
  Serial.println("Ready...");
  Serial.println("IP: " + ipStr);  
  
}
////////////////////////////////////////////////////////////////////////// end setup()

//////////////////////////////////////////////////////////////////////////////////////
void loop() {

  // Listen for a button push
  int reading = digitalRead(buttonPin);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) {
        Serial.println("BUTTON PRESSED! GO HOME XY");
        // getPrinterState();
        // getTemperatures();
        goHomeXY();
        // goHomeXYZ();
        // goForLevel();
        
      }
    }
  }

  lastButtonState = reading;

  // Calls /api/printer?exclude=sd every 6 seconds. [interval(ms)]
  unsigned long currentMillis = millis();
  if(currentMillis - previousMillis > interval) {
    previousMillis = currentMillis;   
    // Do something... 
    getPrinterState();
  }
}
//////////////////////////////////////////////////////////////////////////////// end loop()

// Methods 

boolean isOnline(){
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  } else {
    return true;
  }
}

void parsePrinterStateJSON(String jsonStr){
  DynamicJsonBuffer  jsonBuffer( jsonStr.length() );
  char *json_c_str = &jsonStr[0u];
  JsonObject& root = jsonBuffer.parseObject(json_c_str);
  if (!root.success()) {
    Serial.println("parseJSON() failed");
    return;
  }

  const char* printer_state_text = root["state"]["text"];
  double printer_temperature_bed = root["temperature"]["bed"]["actual"];
  double printer_temperature_bed_target = root["temperature"]["bed"]["target"];
  double printer_temperature_tool0 = root["temperature"]["tool0"]["actual"];
  double printer_temperature_tool0_target = root["temperature"]["tool0"]["target"];

  boolean is_printing = root["state"]["flags"]["printing"];
  boolean is_ready = root["state"]["flags"]["ready"];
  boolean is_paused = root["state"]["flags"]["paused"];
  boolean is_operational = root["state"]["flags"]["operational"];
  boolean has_error = root["state"]["flags"]["error"];
  
  Serial.print("printer_state_text: ");
  Serial.println(printer_state_text);
  
  Serial.print("printer_temperature_bed: ");
  Serial.println(printer_temperature_bed);
  Serial.print("printer_temperature_bed_target: ");
  Serial.println(printer_temperature_bed_target);
  Serial.print("printer_temperature_tool0: ");
  Serial.println(printer_temperature_tool0);
  Serial.print("printer_temperature_tool0_target: ");
  Serial.println(printer_temperature_tool0_target);

  String status_text = "";
  if(is_ready){
    status_text = "PRINTER READY";
  }
  if(is_printing){
    status_text = "PRINTING...";
  }
  if(is_paused){
    status_text = "PAUSED";
  }
  if(has_error){
    status_text = "ERROR!!!!!!";
    // probably do something like stop...
     
  }
  Serial.println("status_text: " + status_text);

  if(is_printing){
     getCurrentJobStatus();
  }

  // set some UI... 
  
}

void parseJobJSON(String jsonStr){
  DynamicJsonBuffer  jsonBuffer( jsonStr.length() );
  char *json_c_str = &jsonStr[0u];
  JsonObject& root = jsonBuffer.parseObject(json_c_str);
  if (!root.success()) {
    Serial.println("parseJSON() failed");
    return;
  }

  const char* printer_job_name = root["job"]["file"]["name"];
  double printer_job_progress = root["progress"]["completion"];

  Serial.print("printer_job_name: ");
  Serial.println(printer_job_name);
  Serial.print("printer_job_progress: ");
  Serial.println(printer_job_progress);
  
  // set some UI... 
}



String octoRequest(String uri, String method, String postData = ""){
  // HTTPClient request helper
  IPAddress addr;
  addr.fromString(OCTO_HOST);
  
  Serial.println("\nConnecting to OctoPrint server...");  
  if (!client.connect(addr, OCTO_PORT)){
    Serial.println("octoRequest Connection failed!");
    return "";
  } else {
    Serial.println("Connected to server!");
    
    client.println(method + " "+uri+" HTTP/1.1");
    client.println("Host: " + OCTO_HOST);
    client.println("Cache-Control: no-cache");
    client.println("X-Api-Key: " + OCTO_API_KEY);
    if(method=="POST"){
      Serial.println("postData JSON: "+postData);
      client.println("Content-Type: application/json");
      client.print("Content-Length: ");
      client.println(postData.length());
    }
    client.println("");
    if(method=="POST"){
      client.println(postData);
    }
    client.println("");
    while (client.connected()) {
      String line = client.readStringUntil('\n');
      // Serial.println("> "+line);   
      if (line == "\r") {
        // Serial.println("headers received");
        break;
      }
    }
    String response;
    while (client.available()) {
      response = client.readString();
    }
    client.stop();
    return response;    
  }
}

void getPrinterState(){
  Serial.println("getPrinterStatus");
  String json = octoRequest("/api/printer?exclude=sd", "GET");
  // Serial.print("STATUS: length: ");
  // Serial.println(json.length()); // test this to make sure we give the DynamicJsonBuffer enough
  // Serial.println(json);
  parsePrinterStateJSON(json);
  
}

void getCurrentJobStatus(){
  Serial.println("getCurrentJobStatus");
  String json = octoRequest("/api/job", "GET");
  // Serial.println(json);
  parseJobJSON(json);
}

void getTemperatures(){
  Serial.println("getTemperatures");
  String json = octoRequest("/api/printer?exclude=sd,state", "GET");
  // Serial.println("TEMPS: ");
  Serial.println(json);
  // parse the json and do something...
}


// Some basic G-Code commands.
void goHomeX(){
  String jsonCMD = "{\"commands\":[\"G91\",\"G28 X0\",\"G90\",\"M18\"]}";
  String json = octoRequest("/api/printer/command", "POST", jsonCMD);
}
void goHomeY(){
  String jsonCMD = "{\"commands\":[\"G91\",\"G28 Y0\",\"G90\",\"M18\"]}";
  String json = octoRequest("/api/printer/command", "POST", jsonCMD);
}
void goHomeZ(){
  String jsonCMD = "{\"commands\":[\"G91\",\"G28 Z0\",\"G90\",\"M18\"]}";
  String json = octoRequest("/api/printer/command", "POST", jsonCMD);
}

void goHomeXY(){
  Serial.println("goHomeXY");
  // using printhead method
  //String jsonCMD = "{\"command\":\"home\",\"axes\":[\"x\",\"y\"]}";
  //String json = octoRequest("/api/printer/printhead", "POST", jsonCMD);
  // Direct G-code : Home X and Y
  String jsonCMD = "{\"commands\":[\"G91\",\"G28 X0 Y0\",\"G90\",\"M18\"]}";
  String json = octoRequest("/api/printer/command", "POST", jsonCMD);
}

void goHomeXYZ(){
  Serial.println("goHomeXYZ");
  // Direct G-code : Home X, Y and Z
  String jsonCMD = "{\"commands\":[\"G91\",\"G28 X0 Y0\",\"G28 Z0\",\"G90\",\"M18\"]}";
  String json = octoRequest("/api/printer/command", "POST", jsonCMD);
}

void goForLevel(){
  Serial.println("goForLevel");
  // Homes X & Y, then travels to 60,60. Homes Z then disbales motors. 
  String jsonCMD = "{\"commands\":[\"G91\",\"G28 X0 Y0\",\"G90\",\"G1 X60.0 Y60.0 F3000\",\"G28 Z0\",\"M18\"]}";
  String json = octoRequest("/api/printer/command", "POST", jsonCMD);
}


// Done.
// That's it... Now go and add a screen or something! ;) 

// Cheers!  @kosso : 2018.


