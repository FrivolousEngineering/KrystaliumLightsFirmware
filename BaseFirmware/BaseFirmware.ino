#include <ESP8266mDNS.h>

#include <Adafruit_NeoPixel.h>
#include "WiFiManager.h"  //https://github.com/tzapu/WiFiManager
#include <ArduinoOTA.h>
#include <ArduinoJson.h> 


#include "git-version.h" // https://github.com/FrivolousEngineering/git-describe-arduino


#define NUMPIXELS 12 // Number of pixels in the ledring
#define NUM_LED_GROUPS 12

#define LEDRINGPIN D2 // Datapin for the ledring


// Any unconnected pin, to try to generate a random seed
#define UNCONNECTED_PIN         2
 
// The LED can be in only one of these states at any given time
#define BRIGHT                  0
#define UP                      1
#define DOWN                    2
#define DIM                     3
#define BRIGHT_HOLD             4
#define DIM_HOLD                5
 
// Percent chance the LED will suddenly fall to minimum brightness
#define FLICKER_BOTTOM_PERCENT         10
// Absolute minimum of the flickering
#define FLICKER_ABSOLUTE_MIN_INTENSITY 64
// Minimum intensity during "normal" flickering (not a dramatic change)
#define FLICKER_MIN_INTENSITY          128
// Maximum intensity of the flickering
#define FLICKER_MAX_INTENSITY          255
 
// Decreasing brightness will take place over a number of milliseconds in this range
#define DOWN_MIN_MSECS          20
#define DOWN_MAX_MSECS          250
// Increasing brightness will take place over a number of milliseconds in this range
#define UP_MIN_MSECS            20
#define UP_MAX_MSECS            250
// Percent chance the color will hold unchanged after brightening
#define BRIGHT_HOLD_PERCENT     20
// When holding after brightening, hold for a number of milliseconds in this range
#define BRIGHT_HOLD_MIN_MSECS   0
#define BRIGHT_HOLD_MAX_MSECS   100
// Percent chance the color will hold unchanged after dimming
#define DIM_HOLD_PERCENT        5
// When holding after dimming, hold for a number of milliseconds in this range
#define DIM_HOLD_MIN_MSECS      0
#define DIM_HOLD_MAX_MSECS      50

// Mixes in a certain amount of red in every group.
#define MIN_IMPURITY            32
#define MAX_IMPURITY            96
 
#define MINVAL(A,B)             (((A) < (B)) ? (A) : (B))
#define MAXVAL(A,B)             (((A) > (B)) ? (A) : (B))


 
// END OF CANDLE MODE RELATED STUFF ////////////////////////////////////////////////////

byte state[NUM_LED_GROUPS];
unsigned long flicker_msecs[NUM_LED_GROUPS];
unsigned long flicker_start[NUM_LED_GROUPS];
byte index_start[NUM_LED_GROUPS];
byte index_end[NUM_LED_GROUPS];
byte impurity[NUM_LED_GROUPS];


WiFiManager wifiManager;

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, LEDRINGPIN, NEO_GRB + NEO_KHZ800);

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

void setFlickerIntensity(byte intensity, int group_index)
{
  int led_index = group_index * (NUMPIXELS / NUM_LED_GROUPS);
  int max_led_index = led_index + (NUMPIXELS / NUM_LED_GROUPS);
  int secondary_intensity;
  int impurity_base = impurity[group_index] / 2;
  int impurity_intensity = impurity_base + (impurity_base * ((float)intensity / (float)FLICKER_MAX_INTENSITY));
  // Clamp intensity between max and absolute min.
  Serial.println(((float)intensity / (float)FLICKER_MAX_INTENSITY));
  intensity = MAXVAL(MINVAL(intensity, FLICKER_MAX_INTENSITY), FLICKER_ABSOLUTE_MIN_INTENSITY);

  if (intensity >= FLICKER_MIN_INTENSITY)
  {
    secondary_intensity = intensity * 3 / 8; 
  } else {
    secondary_intensity = intensity * 3.25 / 8;
  }
  
  for(; led_index < max_led_index; led_index++)
  {
    strip.setPixelColor(led_index, impurity_intensity, secondary_intensity, intensity); 
  }
    
  strip.show();
  return;
}

void setupNeoPixel()
{
  // There is no good source of entropy to seed the random number generator,
  // so we'll just read the analog value of an unconnected pin.  This won't be
  // very random either, but there's really nothing else we can do.
  //
  // True randomness isn't strictlyfor(int group_index necessary, we just don't want a whole
  // string of these things to do exactly the same thing at the same time if
  // they're all powered on simultaneously.
  randomSeed(analogRead(UNCONNECTED_PIN));
  Serial.println("SETTING UP NEOPIXEL");
  for(int group_index = 0; group_index < NUM_LED_GROUPS; group_index++)
  {
    Serial.println(group_index);
    setFlickerIntensity(255, group_index);
    index_start[group_index] = 255;
    index_end[group_index] = 255;
    setFlickerState(BRIGHT, group_index);
    impurity[group_index] = int (random(MIN_IMPURITY, MAX_IMPURITY) + 0.5);
  }
  strip.begin();
}

void setupOTA()
{
  // Setup the arduino OTA (over the air update) handling
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

  Serial.println("Firmware version:");
  Serial.println(GIT_VERSION);

  setupNeoPixel();
  
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


  // Set the callback for the custom extra fields
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  
  wifiManager.addParameter(&custom_endpoint);
  
  // Set wifimanager to be not blocking. We don't want the setup of the wifi to prevent the loop from running!
  wifiManager.setConfigPortalBlocking(false); 
  
  //Set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  // Create the name of this board by using the chip ID. 
  sprintf(hostString, "Krystalium-Light-%06X", ESP.getChipId());
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
    delay(1000);
  } 

  digitalWrite(LED_BUILTIN, LOW); // Turn the led start
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

  MDNS.setHostProbeResultCallback(hostProbeResult);

  setupOTA();
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
  for (int i = 0; i < n; ++i) 
  {
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
  MDNS.update();
  wifiManager.process();

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

  neopixelLoop();
}

void neopixelLoop()
{
  unsigned long current_time; 
  current_time = millis();
  //Serial.println("loop!");
  for(int group_index = 0; group_index < NUM_LED_GROUPS; group_index++)
  {
    switch (state[group_index])
    {
      case BRIGHT:
      {   
        //Serial.println("Bright"); 
        flicker_msecs[group_index] = random(DOWN_MAX_MSECS - DOWN_MIN_MSECS) + DOWN_MIN_MSECS;
        flicker_start[group_index] = current_time;
        index_start[group_index] = index_end[group_index];
        if (index_start[group_index] > FLICKER_ABSOLUTE_MIN_INTENSITY && random(100) < FLICKER_BOTTOM_PERCENT)
        {
          index_end[group_index] = random(index_start[group_index] - FLICKER_ABSOLUTE_MIN_INTENSITY) + FLICKER_ABSOLUTE_MIN_INTENSITY;
        } else {
          index_end[group_index] = random(index_start[group_index]- FLICKER_MIN_INTENSITY) + FLICKER_MIN_INTENSITY;
        }
   
        setFlickerState(DOWN, group_index);
        break;  
      }  
      case DIM:
      {
        //Serial.println("Dim");
        flicker_msecs[group_index] = random(UP_MAX_MSECS - UP_MIN_MSECS) + UP_MIN_MSECS;
        flicker_start[group_index] = current_time;
        index_start[group_index] = index_end[group_index];
        index_end[group_index] = random(FLICKER_MAX_INTENSITY - index_start[group_index]) + FLICKER_MIN_INTENSITY;
        setFlickerState(UP, group_index);
        break;
      }
      case BRIGHT_HOLD:  
      case DIM_HOLD:
      {
        //Serial.println("DIM Hold");
        if (current_time >= (flicker_start[group_index] + flicker_msecs[group_index]))
        {
          setFlickerState(state[group_index] == BRIGHT_HOLD ? BRIGHT : DIM, group_index); 
        }
        break;
      }
      case UP:
      case DOWN:
      {
        //  Serial.println("Down");
        if (current_time < (flicker_start[group_index] + flicker_msecs[group_index])) {
          setFlickerIntensity(index_start[group_index] + ((index_end [group_index]- index_start[group_index]) * (((current_time - flicker_start[group_index]) * 1.0) / flicker_msecs[group_index])), group_index);
        } else {
          setFlickerIntensity(index_end[group_index], group_index);
   
          if (state[group_index] == DOWN)
          {
            if (random(100) < DIM_HOLD_PERCENT)
            {
              flicker_start[group_index] = current_time;
              flicker_msecs[group_index] = random(DIM_HOLD_MAX_MSECS - DIM_HOLD_MIN_MSECS) + DIM_HOLD_MIN_MSECS;
              setFlickerState(DIM_HOLD, group_index);
            } else {
              setFlickerState(DIM, group_index);
            } 
          } else {
            if (random(100) < BRIGHT_HOLD_PERCENT)
            {
              flicker_start[group_index] = current_time;
              flicker_msecs[group_index] = random(BRIGHT_HOLD_MAX_MSECS - BRIGHT_HOLD_MIN_MSECS) + BRIGHT_HOLD_MIN_MSECS;
              setFlickerState(BRIGHT_HOLD, group_index);
            } else {
              setFlickerState(BRIGHT,group_index);
            }
          }
        }
        break;
      }
    }
  }
}

void setFlickerState(byte new_state, int group_index)
{
  state[group_index] = new_state;
  // Select which pixels should be changed! 
}

void hostProbeResult(String p_pcDomainName, bool p_bProbeResult) 
{
  Serial.println("MDNSProbeResultCallback");
  //Serial.printf("MDNSProbeResultCallback: Host domain '%s.local' is %s\n", p_pcDomainName.c_str(), (p_bProbeResult ? "free" : "already USED!"));
  if (!hMDNSService) 
  {
    hMDNSService = MDNS.addService(0, "krystalium", "tcp", 80);
    if (hMDNSService) 
    {
      // Add a simple static MDNS service TXT item
      // MDNS.addServiceTxt(hMDNSService, "port#", 80);
      MDNS.addServiceTxt(hMDNSService, "Version", GIT_VERSION);
      // Set the callback function for dynamic service TXTs
      MDNS.setDynamicServiceTxtCallback(MDNSDynamicServiceTxtCallback);
    }
  }
}

void MDNSDynamicServiceTxtCallback(const MDNSResponder::hMDNSService p_hService) 
{
  if (hMDNSService == p_hService) 
  {
    //MDNS.addDynamicServiceTxt(p_hService, "Server", serverIP.toString().c_str());
  }
}
