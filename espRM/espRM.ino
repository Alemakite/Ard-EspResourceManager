////////////////////////////////////////////////////////////////
// Task 1
// NodeMCU (ESP8266) Resource Manager
// Name / Date 
// Piotr Kosek / 17.02.2022
////////////////////////////////////////////////////////////////

#include <Wire.h>
// Handshaking lines
#define DEMAND D1
#define GRANTED D2

// Safe way of pulling the DEMAND line high and low.
// For high, the pin is set as an input_pullup, pulled to logic 1.
// For low, the pin is set as an output, pulled to logic 0.
#define DEMAND_HI pinMode(D1,INPUT_PULLUP);
#define DEMAND_LO digitalWrite(D1, LOW); \
                  pinMode(D1, OUTPUT);

// Using the built-in LED on the ESP8266.                  
#define LED D4
#define LED_ON digitalWrite(LED, LOW)
#define LED_OFF digitalWrite(LED, HIGH)

// Defining the RESOURCE MANAGER FSM states 
#define noTriggerNotGranted 0 // idle state
#define triggerNotGranted 1   // trigger detected, waiting to be granted 
#define triggerMaster 2       // access granted, esp is master

//RESOURCE MANAGER variables
static int esp_state;

//TRIGGER PULLER variables
static int trig_pull_state;

//Other module variables 
static bool esp_trigger;

// Module clock initializers
bool init_esp_clock;
bool init_trig_pull_clock;


//==============================================================
// granted()
//
// This function checks the status of the GRANTED line.
// If this line is low, it means the resource has been 
// granted and the function returns true.
// Otherwise, the resource has not been granted and the
// function returns false.
//==============================================================
bool granted()
{
  return (!digitalRead(D2));   //if line is low then resource was granted, returns true
}                                   //if high, resource not granted, returns false

void setup() 
{

  DEMAND_HI;
  pinMode(GRANTED, INPUT_PULLUP);
  
  esp_state = noTriggerNotGranted;
  esp_trigger = false;

  pinMode(LED, OUTPUT);

  Serial.begin(9600);
  
  Wire.begin(D6, D7);

  init_trig_pull_clock = true;
}

void loop() 
{
  //==============================================================
  // RESOURCE MANAGER MODULE
  //
  // This untimed module resolves I2c connection between
  // esp8266 and arduino nano.
  //==============================================================
  //
  // RESOURCE MANAGER MODULE
   {
    switch(esp_state)
    {
      case noTriggerNotGranted:
        DEMAND_HI;
        LED_OFF;
        if(esp_trigger)
          esp_state = triggerNotGranted;
        else
          esp_state = noTriggerNotGranted;
       break;
  
      case triggerNotGranted:
        DEMAND_LO;
        LED_OFF;
        if(granted())
          esp_state = triggerMaster;
        else
          esp_state = triggerNotGranted;
       break;

      case triggerMaster:
        DEMAND_LO;
        LED_ON;
        if(!esp_trigger) 
          esp_state = noTriggerNotGranted;
        else
          esp_state = triggerMaster;  
       break;
      default: esp_state = noTriggerNotGranted;
    }
   }

  //==============================================================
  //TRIGGER PULLER MODULE
  //
  // This timed module drives the esp_trigger 
  // variable(controlling the RESOURCE MANAGER) and acquires
  // orientation from arduino nano to print it to the serial monitor.
  //==============================================================
  //
  //TRIGGER PULLER MODULE
  {
    static unsigned long trig_pull_time, trig_pull_delay;
    static bool trig_pull_execute;
    static char system_state;
    if(init_trig_pull_clock)
    {
      trig_pull_time = millis();
      //initial = trig_pull_time;
      trig_pull_delay = 1;
      trig_pull_execute = false;
      init_trig_pull_clock = false;
      trig_pull_state = 0;
    }

    else if ((long)(millis() - trig_pull_time) >= trig_pull_delay)
      {
        trig_pull_time = millis();
        trig_pull_execute = true;
      }
      else
      {
        trig_pull_execute = false;
      }
      
    if(trig_pull_execute)
    {
      switch(trig_pull_state)
      {
        case 0:
          trig_pull_delay = 19;
          trig_pull_state = 1;
         break;

        case 1:
            esp_trigger = true;
            trig_pull_delay = 1;
            trig_pull_state = 2;
        break;

        case 2:
          if(esp_state == triggerMaster)  //wait here until esp reaches triggerMaster state 
            trig_pull_state = 3;
        break;

        case 3:
          Wire.requestFrom(8, 1); // request & read data of size 1 from arduino nano
          if(Wire.available())
            {
              system_state = Wire.read();
            }
          esp_trigger = false;
          Serial.print("System state is : ");
          Serial.println(system_state);        
          trig_pull_state = 0;
         break;
        default: trig_pull_state = 0;
      }
    }
  }
}
