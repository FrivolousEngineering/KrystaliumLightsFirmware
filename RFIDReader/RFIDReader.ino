
// Include Libraries
#include "Arduino.h"
#include "RFID.h"


// Pin Definitions
#define RFID_PIN_RST	2
#define RFID_PIN_SDA	10

#define buzzerPin 3
#define GREEN_LED_PIN 5 

#define RED_LED_PIN 7

#define led_brightness 128

bool card_was_present_last_check = false;

// object initialization
RFID rfid(RFID_PIN_SDA,RFID_PIN_RST);

String detected_tag = "None";
bool waiting_for_confirmation = false;


#define MAX_BUFFER_SIZE 64
uint8_t command_buffer_pos = 0;
char command_buffer[MAX_BUFFER_SIZE];


void setup() 
{
  // Setup Serial which is useful for debugging
  // Use the Serial Monitor to view printed messages
  Serial.begin(9600);
  while (!Serial) ; // wait for serial port to connect. Needed for native USB
  Serial.println("start");
  
  // Initialize RFID module
  rfid.init();
  pinMode(RED_LED_PIN, OUTPUT);  
  pinMode(GREEN_LED_PIN, OUTPUT); 
  pinMode(buzzerPin, OUTPUT); //Set buzzerPin as output
  analogWrite(buzzerPin, 255);
}

void beep(int delayms, float frequency = 4000) 
{
  tone(buzzerPin, frequency, delayms);
  delay(delayms + 2);
  analogWrite(buzzerPin, 255);
}

void loop() 
{
  analogWrite(buzzerPin, 255);
  if(waiting_for_confirmation)
  {
    analogWrite(GREEN_LED_PIN, 0);
    analogWrite(RED_LED_PIN, led_brightness);
    delay(200);
    analogWrite(GREEN_LED_PIN, led_brightness);
    analogWrite(RED_LED_PIN, 0);
    delay(200);
    if(readFromSerial())
    {
      
      String result(command_buffer); 
      if(result == "ok")
      {
        waiting_for_confirmation = false;
        analogWrite(GREEN_LED_PIN, led_brightness);
        analogWrite(RED_LED_PIN, 0);
        beep(50, 4000);
        delay(50);
        beep(50, 5000);
        delay(50);
        beep(50, 6000);
      } else
      {
        waiting_for_confirmation = false;
        // DENIED
        analogWrite(GREEN_LED_PIN, 0);
        analogWrite(RED_LED_PIN, led_brightness);
        beep(50, 500);
        delay(50);
        beep(50, 500);
        delay(50);
        
        beep(50, 500);
      }
    }
  } else
  {
    // Normal read mode
    detected_tag = rfid.readTag();
    if(detected_tag == "None")
    {
      card_was_present_last_check = false; // Card has been removed
      waiting_for_confirmation = false;
    } else
    {
      //analogWrite(GREEN_LED_PIN, led_brightness);
      //analogWrite(RED_LED_PIN, 0);
      if(!card_was_present_last_check)
      {
        // Ask the other side if the card is okay!
        waiting_for_confirmation = true;
        Serial.println(detected_tag);
        beep(50);
        delay(50);
        card_was_present_last_check = true;
      }
    }
  }
}





bool readFromSerial()
{
    for(int c; (c = Serial.read()) != -1; )
    {
        if (c == '\n' || c == '\r')
        {
            if (command_buffer_pos > 0)
            {
                command_buffer[command_buffer_pos] = '\0';
                command_buffer_pos = 0;
                return true;
            }
        }else{
            command_buffer[command_buffer_pos++] = c;
            if (command_buffer_pos == MAX_BUFFER_SIZE - 1)
                command_buffer_pos --;
        }
    }
    return false;
}
