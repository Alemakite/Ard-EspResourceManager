
////////////////////////////////////////////////////////////////
// Task 1
// Arduino Nano Resource Manager
// Name / Date
// Piotr Kosek / 17.02.2022 
////////////////////////////////////////////////////////////////

#include <Wire.h>

//ACCELEROMETER MODULE
#define PWR_MGMT_1 0x6B //power menegment register on the accelerometer
#define MPU_6050 0x68   //accelerometer's proccessor I2C
#define ACCEL_XOUT_HI 0x3B  //X axis register
#define ACCEL_YOUT_HI 0x3D  //Y axis register
#define ACCEL_ZOUT_HI 0x3F  //Z axis register

// Safe way of pulling the GRANTED line high and low.
// For high, the pin is set as an input
// For low, the pin is set as an output, pulled to logic 0.
#define GRANTED A2
#define GRANTED_HI pinMode(GRANTED,INPUT);
#define GRANTED_LO digitalWrite(GRANTED, LOW); \
                   pinMode(GRANTED, OUTPUT);

// Defining the RESOURCE MANAGER FSM states 
#define noTriggerNoDemand 0 // idle state
#define triggerMaster 1     // trigger detected, there was no demand 
#define noTriggerSlave 2    // there was a demand, it is granted 

// Global variable for storing the orientation
static char orientation;

//RESOURCE MANAGER variables
static bool ard_trigger;

//TRIGGER PULLER variables
static int trig_pull_state;

//Other module variables 
static int ard_state;

// Module clock initializers
bool init_trig_pull_clock;
bool acc_power_up;

//==============================================================
// demand()
//
// This function measures the voltage on analogue pin A6
// If this exceeds 1.25V the line is considered to be a
// logic 1, there is no demand, and the function returns false.
// Otherwise the line is considered a logic '0', there is
// demand and the function returns true.
//==============================================================
bool demand()
{
 return (!(analogRead(A6) >> 8));
}

void setup() {
 GRANTED_HI;
 
 ard_trigger = false;
 ard_state = noTriggerNoDemand;
  
 orientation = ' ';

 Wire.begin(8); // join i2c bus with address 8   
 Wire.onRequest(requestEvent); // register request event

 acc_power_up = true;
 init_trig_pull_clock = true;
 
 Serial.begin(9600);
}
void loop() 
{
 //==============================================================
 // RESOURCE MANAGER MODULE
 //
 // This untimed module resolves I2c connection between
 // arduino nano and esp8266.
 //==============================================================
 //
 //RESOURCE MANAGER MODULE
    {
      switch(ard_state)
      {
         case noTriggerNoDemand:
          GRANTED_HI;
          if(ard_trigger)
          { 
            ard_state = triggerMaster;
          }
          else if(demand())
          {
            ard_state = noTriggerSlave;
          }
          else
          {
            ard_state = noTriggerNoDemand;
          }
          break;

         case triggerMaster:
         GRANTED_HI;
         if(ard_trigger)
          {
            ard_state = triggerMaster;
          }
         else if(demand())
          {
            ard_state = noTriggerSlave;
          }
         else
          {
            ard_state = noTriggerNoDemand;
          }
          break;

         case noTriggerSlave:
         GRANTED_LO;
         if(!demand())
          {
            ard_state = noTriggerNoDemand;
          }
         else
         {
            ard_state = noTriggerSlave;
         }
          break;
         default:
            ard_state = noTriggerNoDemand;
      }
  }

  //==============================================================
  //TRIGGER PULLER MODULE
  //
  // This timed module drives the ard_trigger 
  // variable(controlling the RESOURCE MANAGER), requests data 
  // from accelerometer and based on them calculates the 
  // breadboard orientation.
  //==============================================================
  //
  //TRIGGER PULLER MODULE
  {
    static unsigned long trig_pull_time, trig_pull_delay;
    static bool trig_pull_execute;
    
    if(init_trig_pull_clock)
    {
      trig_pull_time = millis();
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
          trig_pull_delay = 17;
          trig_pull_state = 1;
        break;

        case 1:
            ard_trigger = true;
            trig_pull_state = 2;
            trig_pull_delay = 1;
        break;

        case 2:
          if(ard_state == triggerMaster)  //wait here until arduino nano reaches triggerMaster state
          {
            trig_pull_state = 3;
          }
        break;

        case 3:         
          static int16_t accX,accY,accZ; // accelerometer axes variables
          
          if(acc_power_up) // accelerometer one time setup and wake-up
          {
            Wire.beginTransmission(MPU_6050); 
            Wire.write(PWR_MGMT_1);
            Wire.write(0); //awaken the MPU
            Wire.endTransmission(true);
            acc_power_up = false;
          }
  
          // request accelerometer data
          Wire.beginTransmission(MPU_6050);
          Wire.write(ACCEL_XOUT_HI);
          Wire.endTransmission(false);
          Wire.requestFrom(MPU_6050,6,true);  //request acces to 6 registers, 2 for each axis
                                              
          accX = Wire.read()<<8|Wire.read();
          accY = Wire.read()<<8|Wire.read();
          accZ = Wire.read()<<8|Wire.read();
  
          Wire.endTransmission(MPU_6050);

          
          ard_trigger = false;
          
          if(accZ > 10000 )  //if the board is lying flat (checking the Z axis value)
            orientation = 'F';
  
          if(accZ < -10000) //if the board  is lying upside_down(checking the Z axis)
            orientation = 'b';  
  
          if(accX > 10000) //if the board is held in landscape (checking the X axis)
            orientation = 'L';  
            
          if(accX < -10000) //if the board is held upside-down in landspace (checking the X axis)
            orientation = 'U';  
  
          if(accY < -10000) //if the board is held in portrait(on the left side)(checking the Y axis)
            orientation = 'l';  
      
          if(accY > 10000) //if the board is held in portrait(on the right side)(checking the Y axis)
            orientation = 'r'; 
    
          trig_pull_state = 0;
          break;
         default:
          trig_pull_state = 0;
      }
    }
  }

}

//==============================================================
// requestEvent()
//
// This function executes whenever data 
// is requested by master( esp ). Upon request, sends back the 
// last acquired breadboard orientation.
//==============================================================
void requestEvent()
{
  Wire.write(orientation);
}
