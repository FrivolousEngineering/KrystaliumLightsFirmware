#include <ESP8266WiFi.h>  //https://github.com/esp8266/Arduino

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include "WiFiManager.h"  //https://github.com/tzapu/WiFiManager
#include <ArduinoOTA.h>


const char* board_name = "test_board";

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.print("Entered config mode IP: ");
  Serial.print(WiFi.softAPIP());
  Serial.print(" accespoint name: ");
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(0, INPUT_PULLUP);
  pinMode(BUILTIN_LED, OUTPUT);

  WiFiManager wifiManager;
  
  // Blink a few times to indicate reboot. 
  digitalWrite(LED_BUILTIN, LOW); // Turn the led on
  delay(250);
  digitalWrite(LED_BUILTIN, HIGH); // Turn the led off
  delay(250);
  digitalWrite(LED_BUILTIN, LOW); // Turn the led on
  delay(250);
  digitalWrite(LED_BUILTIN, HIGH); // Turn the led off
  
  //Set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);


  // Try to connect with stored settings. If that fails, start access point.
  if(!wifiManager.autoConnect(board_name)) {
    Serial.println("failed to connect and hit timeout");
    // Sooo even the autoconnect failed...
    // Do a long blink to notify!
    digitalWrite(LED_BUILTIN, LOW); // Turn the led on
    delay(4000);
    digitalWrite(LED_BUILTIN, HIGH); // Turn the led off
    ESP.reset();
    delay(1000);
  } 

  //if you get here you have connected to the WiFi
  Serial.println("Connection scuceeded!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  ArduinoOTA.setHostname(board_name);
  
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH){
      type = "sketch";
    }
    else { // U_SPIFFS
      type = "filesystem";
    }
    Serial.println("Start updating " + type);
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
  Serial.println("Arduino OTA has booted.");
}

void loop() {
  // Do the over the air Maaaagic.
  ArduinoOTA.handle(); 

  // Handle resetting of the wifi credentials.
  if(digitalRead(0) == LOW)
  {
    Serial.printf("RESETTING CREDENTIALS!");
    digitalWrite(LED_BUILTIN, LOW); // Turn the led on
    delay(2000);
    digitalWrite(LED_BUILTIN, HIGH); // Turn the led off again
    
    WiFiManager wifiManager;
    wifiManager.resetSettings();
    ESP.restart();
  }
}
