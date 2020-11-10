#include <ESP8266WiFi.h>  //https://github.com/esp8266/Arduino
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include "WiFiManager.h"  //https://github.com/tzapu/WiFiManager
#include <ArduinoOTA.h>
#include <ArduinoJson.h> 


char hostString[20] = {0};
char endpoint[40];

bool shouldSaveConfig = false;

IPAddress serverIP = IPAddress(0);

MDNSResponder::hMDNSService hMDNSService = 0; // The handle of our mDNS Service

void configModeCallback (WiFiManager *myWiFiManager) 
{
  Serial.print("Entered config mode IP: ");
  Serial.print(WiFi.softAPIP());
  Serial.print(" accespoint name: ");
  // If you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void saveConfigCallback () 
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setup() 
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(0, INPUT_PULLUP);
  pinMode(BUILTIN_LED, OUTPUT); // Primary led
  pinMode(2, OUTPUT); // Secondary led
  
  // BTurn led on so we see something is going on. 
  digitalWrite(LED_BUILTIN, LOW); // Turn the led on
  Serial.println(""); // Ensure a newline
  Serial.println("Starting setup");
  
  // Clear the data (used for debugging)
  // SPIFFS.format(); // This shouldn't be done in live production code since it will reset all data that it has. 
  if (SPIFFS.begin()) 
  {
    Serial.println("Mounted file system");
    if (SPIFFS.exists("/config.json")) 
    {
      //file exists, reading and loading
      Serial.println("Reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) 
      {
        Serial.println("Opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, buf.get());
        // Print it on the serial
        serializeJson(doc, Serial);
        
        if (!error) 
        {
          Serial.println("\nparsed json");
          strcpy(endpoint, doc["endpoint"]);
        } else 
        {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else 
  {
    Serial.println("Failed to mount FS");
  }

  digitalWrite(LED_BUILTIN, HIGH); // Turn the led off
  Serial.println("Data loaded, starting wifi manager");
  WiFiManagerParameter custom_endpoint("endpoint", "Custom endpoint", endpoint, 40);

  WiFiManager wifiManager;

  // Set the callback for the custom extra fields
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  
  wifiManager.addParameter(&custom_endpoint);
  
  //Set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  // Create the name of this board by using the chip ID. 
  sprintf(hostString, "Base-Control-%06X", ESP.getChipId());
  Serial.print("Hostname: ");
  Serial.println(hostString);
  
  // Try to connect with stored settings. If that fails, start access point.
  if(!wifiManager.autoConnect(hostString)) 
  {
    Serial.println("failed to connect and hit timeout");
    // Sooo even the autoconnect failed...
    // Do a long blink to notify!
    digitalWrite(LED_BUILTIN, LOW); // Turn the led on
    delay(4000);
    digitalWrite(LED_BUILTIN, HIGH); // Turn the led off
    ESP.reset();
    delay(1000);
  } 

  digitalWrite(LED_BUILTIN, LOW); // Turn the led on
  strcpy(endpoint, custom_endpoint.getValue());

  if(shouldSaveConfig)
  {
    // The custom config has been changed, so we have to store something.
    Serial.println("Saving the updated config");
    DynamicJsonDocument doc(1024);
    doc["endpoint"] = endpoint;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) 
    {
      Serial.println("failed to open config file for writing");
    }

    serializeJson(doc, Serial);
    serializeJson(doc, configFile);
    configFile.close();
    Serial.println("");
  }

  // If you get here you have connected to the WiFi
  Serial.println("Connection scuceeded!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  ArduinoOTA.setHostname(hostString);
  
  ArduinoOTA.onStart([]() 
  {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
    {
      type = "sketch";
    }
    else { // U_SPIFFS
      type = "filesystem";
    }
    Serial.println("Start updating " + type);
  });
  
  ArduinoOTA.onEnd([]() 
  {
    Serial.println("\nEnd");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) 
  {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) 
  {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
  Serial.println("Arduino OTA has booted.");
  digitalWrite(LED_BUILTIN, HIGH); // Turn the led off
}

void resolveServerIPWithWait()
{
  serverIP = getServerIP();
  if(serverIP != IPAddress(0))
  {
    return;
  }

  digitalWrite(2, LOW);
  delay(500);
  digitalWrite(2, HIGH);
  delay(500);
}

IPAddress getServerIP()
{
  MDNS.setHostProbeResultCallback(hostProbeResult);
  int n = MDNS.queryService("ScifiBase", "tcp");
  for (int i = 0; i < n; ++i) {
    if(MDNS.hostname(i) == "Base-Control-Server._ScifiBase._tcp.local")
    {
      return MDNS.IP(i);
    }
  }
  return IPAddress(0);
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

  notifyServer();
  delay(2000);
}


void notifyServer()
{
  if(serverIP == IPAddress(0))
  {
    resolveServerIPWithWait();
    if(serverIP == IPAddress(0))
    {
      Serial.println("Unable to find the Server");
      return; // Still nothing.
    }
  }
  
  HTTPClient http;
  http.addHeader("Content-Type", "application/json");
  
  // Connect with the control server at port 5000
  String server_address = serverIP.toString();
  server_address = "http://" + server_address + ":5000/controller/" + String(hostString) + "/";
  http.begin(server_address);  // Maybe we shouldn't start a server every update loop, but eh...

  DynamicJsonDocument doc(1024);
  doc["sensor_value"] = analogRead(A0);  // Use .set(value) to know if it succeedded (returns true if it worked)

  char output[128];
  serializeJson(doc, output);
  
  // Do the actual request.
  int httpCode = http.PUT(output);
  if(httpCode > 0)
  { 
    Serial.print("HTTP CODE RETURNED: ");
    Serial.println(httpCode);
    String payload = http.getString(); 
    // TODO: Actually do something with the response
    Serial.print("RESPONSE:");
    Serial.println(payload);
  } else 
  {
    Serial.print("Failed to find the server! Got status code");
    Serial.println(httpCode);
    // Couldn't find the server. Oh noes!
    resolveServerIPWithWait();
  }
}

void hostProbeResult(String p_pcDomainName, bool p_bProbeResult) 
{
  Serial.println("MDNSProbeResultCallback");
  //Serial.printf("MDNSProbeResultCallback: Host domain '%s.local' is %s\n", p_pcDomainName.c_str(), (p_bProbeResult ? "free" : "already USED!"));
  if (!hMDNSService) 
  {
    hMDNSService = MDNS.addService(0, "ScifiBase", "tcp", 80);
    if (hMDNSService) 
    {
      // Add a simple static MDNS service TXT item
      // MDNS.addServiceTxt(hMDNSService, "port#", 80);
      
      // Set the callback function for dynamic service TXTs
      MDNS.setDynamicServiceTxtCallback(MDNSDynamicServiceTxtCallback);
    }
  }
}

void MDNSDynamicServiceTxtCallback(const MDNSResponder::hMDNSService p_hService) 
{
  if (hMDNSService == p_hService) 
  {
    MDNS.addDynamicServiceTxt(p_hService, "Server", serverIP.toString().c_str());
  }
}
