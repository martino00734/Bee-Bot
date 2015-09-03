/*==============================================
 * BBot 
 * 29 Aug 2015
 * 
 *==============================================
*/

// I2Cdev and MPU6050 must be installed as libraries, or else the .cpp/.h files
// for both classes must be in the include path of your project
#include "I2Cdev.h"

#include "MPU6050_6Axis_MotionApps20.h"

// Arduino Wire library is required if I2Cdev I2CDEV_ARDUINO_WIRE implementation
// is used in I2Cdev.h
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    #include "Wire.h"
#endif

MPU6050 mpu;


/* =========================================================================
   NOTE: In addition to connection 3.3v, GND, SDA, and SCL, this sketch
   depends on the MPU-6050's INT pin being connected to the Arduino's
   external interrupt #0 pin. On the Arduino Uno and Mega 2560, this is
   digital I/O pin 2.
 * ========================================================================= */

#define LED_PIN 13 // (Arduino is 13, Teensy is 11, Teensy++ is 6)
bool blinkState = false;

// MPU control/status vars
bool dmpReady = false;  // set true if DMP init was successful
uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount;     // count of all bytes currently in FIFO
uint8_t fifoBuffer[64]; // FIFO storage buffer

// orientation/motion vars
Quaternion q;           // [w, x, y, z]         quaternion container
VectorInt16 aa;         // [x, y, z]            accel sensor measurements
VectorInt16 aaReal;     // [x, y, z]            gravity-free accel sensor measurements
VectorInt16 aaWorld;    // [x, y, z]            world-frame accel sensor measurements
VectorFloat gravity;    // [x, y, z]            gravity vector
float euler[3];         // [psi, theta, phi]    Euler angle container
float ypr[3];           // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector

// packet structure for InvenSense teapot demo
uint8_t teapotPacket[14] = { '$', 0x02, 0,0, 0,0, 0,0, 0,0, 0x00, 0x00, '\r', '\n' };


// ================================================================
// ===                    CMDS DETECTION ROUTINE                ===
// ================================================================
//BTNS
#define NONE 0
#define IMU_WARM 1
#define CANCEL 2

#define UP 5
#define DN 6
#define LEFT 7
#define RIGHT 8
#define GO 9

//SYSTEM



// ================================================================
// ===               INTERRUPT DETECTION ROUTINE                ===
// ================================================================

volatile bool mpuInterrupt = false;     // indicates whether MPU interrupt pin has gone high
void dmpDataReady() {
    mpuInterrupt = true;
}



// ================================================================
// ===                      INITIAL SETUP                       ===
// ================================================================

void setup() {
    // join I2C bus (I2Cdev library doesn't do this automatically)
    #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
        Wire.begin();
        TWBR = 24; // 400kHz I2C clock (200kHz if CPU is 8MHz)
    #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
        Fastwire::setup(400, true);
    #endif

    // initialize serial communication
    // (115200 chosen because it is required for Teapot Demo output, but it's
    // really up to you depending on your project)
    Serial.begin(115200);
   
    // initialize device
    Serial.println(F("Initializing I2C devices...f="));
    mpu.initialize();

    // verify connection
    Serial.println(F("Testing device connections..."));
    Serial.println(mpu.testConnection() ? F("MPU6050 connection successful") : F("MPU6050 connection failed"));

    // wait for ready
    Serial.println(F("\nSend any character to begin DMP programming and demo: "));
    /*while (Serial.available() && Serial.read()); // empty buffer
    while (!Serial.available());                 // wait for data
    while (Serial.available() && Serial.read()); // empty buffer again*/

    // load and configure the DMP
    Serial.println(F("Initializing DMP..."));
    devStatus = mpu.dmpInitialize();

    // supply your own gyro offsets here, scaled for min sensitivity
    mpu.setXGyroOffset(220);
    mpu.setYGyroOffset(76);
    mpu.setZGyroOffset(-85);
    mpu.setZAccelOffset(1788); // 1688 factory default for my test chip

    // make sure it worked (returns 0 if so)
    if (devStatus == 0) {
        // turn on the DMP, now that it's ready
        Serial.println(F("Enabling DMP..."));
        mpu.setDMPEnabled(true);

        // enable Arduino interrupt detection
        Serial.println(F("Enabling interrupt detection (Arduino external interrupt 0)..."));
        attachInterrupt(0, dmpDataReady, RISING);
        mpuIntStatus = mpu.getIntStatus();

        // set our DMP Ready flag so the main loop() function knows it's okay to use it
        Serial.println(F("DMP ready! Waiting for first interrupt..."));
        dmpReady = true;

        // get expected DMP packet size for later comparison
        packetSize = mpu.dmpGetFIFOPacketSize();
    } else {
        // ERROR!
        // 1 = initial memory load failed
        // 2 = DMP configuration updates failed
        // (if it's going to break, usually the code will be 1)
        Serial.print(F("DMP Initialization failed (code "));
        Serial.print(devStatus);
        Serial.println(F(")"));
    }

    // configure LED for output
    pinMode(LED_PIN, OUTPUT); //LED
    pinMode(12,OUTPUT); //spkr
    pinMode(10,OUTPUT); //PWM Ctrl Dir
    pinMode(9,OUTPUT); //PWM Ctrl Dir
    digitalWrite(9, HIGH); //turn drives off
    digitalWrite(10, HIGH);
    digitalWrite(3, HIGH);
    digitalWrite(11, HIGH);
}//END Setup



// ================================================================
// ===                    MAIN PROGRAM LOOP                     ===
// ================================================================

void loop() 
{
    static int    SubLoop, Demand;
    static byte   Moving, CMDBuff[256], Command, CurrentCMD;
    static float  Heading,HeadingTgt;
    float Reduction;
    int i;
    SubLoop++;

 
      
    // if programming failed, don't try to do anything
    if (!dmpReady) return;

    // *********************************************************************
    // ** 100Hz Fast Loop                                                 **
    // *********************************************************************
    
    // wait for MPU interrupt or extra packet(s) available
    while (!mpuInterrupt && fifoCount < packetSize) {
       
    }
    ButtonSounder(&Command);                                // @SM clear cmd msg here when done
    GetHeading(&Heading,&HeadingTgt, Moving);               // Hdg 1000 = FWD, 1180 = Back. Reset tgt & hdg when not moving
    if((CurrentCMD == LEFT) || (CurrentCMD == RIGHT) || (CurrentCMD == GO) )  // We seem to need more gain when the robot is moving? and less when stationary.
      Reduction = .3; // lower gain required
    else
      Reduction=1; 
      
    PID(Heading,HeadingTgt,&Demand,Reduction * 11,Reduction * .7,/*Reduction*/0,Moving);          // If not moving zero integral
  

    // *********************************************************************
    // ** 10Hz Loop                                                      **
    // *********************************************************************
    if((SubLoop % 2) == 0)
    {
    
        //all functions return immediately
        DecodeUserSwitch(&Command,Moving,&CurrentCMD);      // @SM read ADC allways & decode to an integer; if driving this is lost else it's compiled.
                                                            // Elsewhere it's turned to a sound.

        CheckIMU(&Command,Heading);                         // Look to see when the IMU has warmed up, issue a CMD when it has otherwise prevent start
                                                            
        CompileUserCommands(CMDBuff,Command,&Moving);       // if not moving then compile. set moving when asked append  as last CMD
                                                            // [#CMDS] [CMD1] [CMD2] ..... [GO]  (go allways at end)
                                                            //HDG is +/- 180
                   
        PullNextUserCommand(CMDBuff, Moving, &CurrentCMD);  // @SM if moving then execute next command 
        
     /*   Serial.print(Heading);
        Serial.print(" , "); 
        Serial.print(HeadingTgt);
        Serial.print(" , "); 
        Serial.println(Demand);*/
        
        ExecuteCommand(&CurrentCMD,&Moving,&HeadingTgt,Demand);// @SM takes several seconds to move the robot with a state machine
                                                            // read CMD , then execute CMD in background.  For turn this is a modification of the TGT then a wait
                                                            // when a CMD has executed set it to NEXT_CMD so above code pulls next CMD
                                                            // For FWD the Auto head demands are offset FWD or back.  
                                                            // The Last CMD is allways "END" which which turns off motors and clears moving 
                                                            // drivemotors allways does auto HDG when Moving!!       
    }//END 10HZ

    // blink LED to indicate activity
    if((SubLoop % 100) == 0)
    {   
        blinkState = !blinkState;
        digitalWrite(LED_PIN, blinkState);
    }
    
}//END Loop

void ExecuteCommand(byte *CurrentCMD,byte *Moving,float *HeadingTgt,float Demand)
{
  static byte state;
  static int ForeDmd,Time;
  const int ExecuteDelay=80;
  
  if(*Moving)
  {
    // @@@@@@@@@@ CMD Init @@@@@@@@@@@@@@@@@@@@@@
    if(state==0) 
    {
     
      switch(*CurrentCMD)
      {
        case UP:    ForeDmd = 400;
                    Time = ExecuteDelay *2;
                    state++;
                    break;
        case DN:    ForeDmd = -400;
                    Time = ExecuteDelay *2 ;
                    state++;
                    break;
        case LEFT:  *HeadingTgt -= 90;
                    Time = ExecuteDelay;
                    state++;
                    if(*HeadingTgt < 0) *HeadingTgt += 360;
                    break;
        case RIGHT: *HeadingTgt += 90;
                    Time = ExecuteDelay;
                    state++;
                    if(*HeadingTgt > 360) *HeadingTgt -= 360;
                    break;
        case GO:    state++;
                    Time = ExecuteDelay;

                    break;
        case NONE:
                    break;  
      }//END switch
              
    }//END IF  
    // @@@@@@@@@@ CMD Execute @@@@@@@@@@@@@@@@@@@@
    else 
    {
        // count down them move back to CMD execution state
        Time--;
        if(Time==0)
        {
          //we've reached the end of the program!
          if(*CurrentCMD == GO)
          {
            Serial.println("Go found");
            *Moving = 0;
          } 
          state=0;
          *CurrentCMD = NONE;
          ForeDmd = 0;
        }
    } 
    // @@@@@@@@@@ ----------- @@@@@@@@@@@@@@@@@@@@
    
  }
  else
  {
     //idle waiting for go cmd
     state = 0;
    //NOT Moving
    ForeDmd = 0;
  }

  DriveMotors( (Demand * -1) + ForeDmd,  Demand + ForeDmd, *Moving);
}//END ExecuteCommand

     
void PullNextUserCommand(byte CMDBuff[],byte Moving, byte *CurrentCMD)  // @SM if moving then execute next command 
{
  static byte ptr;

  if(Moving) 
  {
    if(*CurrentCMD==NONE)
    { 
       /* Serial.print(CMDBuff[0]); Serial.print(" , ");
      Serial.print(CMDBuff[1]); Serial.print(" , ");
      Serial.print(CMDBuff[2]); Serial.print(" , ");
      Serial.print(CMDBuff[3]); Serial.print(" , ");
       Serial.print(CMDBuff[4]); Serial.print(" , ");
        Serial.print(CMDBuff[5]); Serial.print(" , ");
         Serial.print(CMDBuff[6]); Serial.print(" , ");
          Serial.print(CMDBuff[7]); Serial.println("");*/
      
       
        //need a new CMD...
        if(ptr <= CMDBuff[0])
        {
         /* Serial.print(CMDBuff[ptr]);
          Serial.println(" -Fetching New Command...");*/
           //CMDs still in there
           *CurrentCMD = CMDBuff[ptr];  
           CMDBuff[ptr] = 0; // clear to make debugging easier
           ptr++;
        }
        else
          CMDBuff[0] = 0; //Empty buffer
    }
  }//END IF Moving
  else
    ptr=1; //set ptr to 1st cmd
    
} //END PullNextUserCommand

void CheckIMU(byte *Command, float  Heading)
{
  static int Init=1,Count;
  static float oHeading;
  
  if(Init)
  {
      //If IMU not stable don't allow the robot to start navigating.
      if(*Command == GO)
        *Command = NONE;
      
      Count++;
      if(Count==100)
      {
        Count = 0; 
        if(abs(Heading - oHeading) < 1)
        {
          Init = 0;    
           *Command = IMU_WARM;
        }
        else
          oHeading = Heading;
      }  
  }
}//END Check IMU

/*
 ===================================================================
 CSU:    getPID()
 ===================================================================
 Desc:   A PID implementation; control an error with 3 constants and
 return a rate +/- 1000.
 I've lowered this to 350 as a result of the motor tests.
 
 ===================================================================
 */
void PID(float Hdg,float HdgTgt,int *Demand, float kP,float kI,float kD, byte Moving)                                                 
{
  static unsigned long lastTime; 
  static float Output; 
  static float errSum, lastErr,error ; 

  // IF not moving then 
  if(!Moving)
  {
        errSum = 0;
        lastErr = 0;
        return;
  }

  //error correction for angular overlap
  error = Hdg-HdgTgt;
  if(error<180)
    error += 360;
  if(error>180)
    error -= 360;
  
      
  //http://brettbeauregard.com/blog/2011/04/improving-the-beginners-pid-introduction/

  /*How long since we last calculated*/
  unsigned long now = millis();    
  float timeChange = (float)(now - lastTime);       
  /*Compute all the working error variables*/
  //float error = Setpoint - Input;    
  errSum += (error * timeChange);   

  //integral windup guard
  LimitFloat(&errSum, -300, 300);

  float dErr = (error - lastErr) / timeChange;       

  /*Compute PID Output*/
  *Demand = kP * error + kI * errSum + kD * dErr;       
  /*Remember some variables for next time*/
  lastErr = error;    
  lastTime = now; 

  //limit demand 
  LimitInt(Demand, -400, 400);

}//END getPID

void CompileUserCommands(byte CMDBuff[],byte Command,byte *Moving)
{
    if(*Moving == 0)
    {
      // When static build up an array of commands.  !st byte is # commands
      // GO btn executes or can CANCEL commands.  When we get a go then start moving
      if(Command >= CANCEL )
      {
        CMDBuff[0]++;
        CMDBuff[ CMDBuff[0] ] = Command;

        if(Command == GO)
        {
           *Moving = 1;  
        }

        if(Command == CANCEL)
        {
           CMDBuff[0] = 0;
        }
      }//END IF
    }//END IF
}//END CompileUserCommands

void ButtonSounder(byte *Command)
{
    static int state;

    // was a CMD issued?
    if(*Command > NONE)
    {
      if(*Command==CANCEL)
        state+=300;
      else if (*Command==IMU_WARM)
        state+=200;
      else
        state+=10;
      
      *Command = NONE;
      //Serial.println(*Command);
    }

    // toggle DO to make noise
    if(state>0)
    {
      state--;
      if(state % 2)
       digitalWrite(12, 1);
      else
       digitalWrite(12, 0); 
    }
}//END ButtonSounder

void  GetHeading(float *Heading,float *HeadingTgt, byte Moving)               
{

  {
      //calc heading from IMU
      // reset interrupt flag and get INT_STATUS byte
      mpuInterrupt = false;
      mpuIntStatus = mpu.getIntStatus();

      // get current FIFO count
      fifoCount = mpu.getFIFOCount();

      // check for overflow (this should never happen unless our code is too inefficient)
      if ((mpuIntStatus & 0x10) || fifoCount == 1024) {
        // reset so we can continue cleanly
        mpu.resetFIFO();
        Serial.println(F("FIFO overflow!"));

        // otherwise, check for DMP data ready interrupt (this should happen frequently)
      } 
      else if (mpuIntStatus & 0x02) 
      {
          // wait for correct available data length, should be a VERY short wait
          while (fifoCount < packetSize) fifoCount = mpu.getFIFOCount();

          // read a packet from FIFO
          mpu.getFIFOBytes(fifoBuffer, packetSize);
        
          // track FIFO count here in case there is > 1 packet available
          // (this lets us immediately read more without waiting for an interrupt)
          fifoCount -= packetSize;
          
          // display Euler angles in degrees
          mpu.dmpGetQuaternion(&q, fifoBuffer);
          mpu.dmpGetGravity(&gravity, &q);
          mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
          //Serial.print("ypr\t");
          *Heading = (ypr[0] * 180/M_PI) + 180;
  
      }//done
  }
  
  if(!Moving)
  {     
      *HeadingTgt=*Heading;
  } 
}//END GetHeading




void DecodeUserSwitch(byte *Command,byte Moving,byte *CurrentCMD)
{
    static byte state = 0;
    static int relcount = 0;
    int Anal;

    if(Moving)
      return;
      
    Anal = analogRead(0);
    
/*  144 up
    327 dn
    0 L
    500 R
    740 Go
    */
    *Command = NONE;
    if(state == 0)
    {
      relcount=0;
      //detect btn press by user
      if (Anal < 100)
        *Command = RIGHT;
      else if (Anal < 200)
        *Command = DN;
      else if (Anal < 400)
        *Command = UP;
      else if (Anal < 600)
        *Command = LEFT;
      else if (Anal < 800)
        state = 1; //go
//Serial.println(*Command);
      //normal command entry
      if(*Command > NONE)
      {                         //Serial.println("got CMD");
        state = 2;
      }
    }
    else if(state == 1)
    {
      // go/cancel detect, hold btn to cancel else it's a go cmd
      relcount++; //used as timer
      //Serial.println(relcount);
      if(relcount > 200)
      {
        *Command=CANCEL;     
        state=2;
      }

      if(Anal>800)
      {
        *Command=GO;
        state = 2;
        *CurrentCMD=NONE; //tells pull to start pulling
      }
    }
    else if(state == 2)
    {
      //wait release
      if(Anal > 800)
        relcount++;
      else
        relcount=0;

      //firm rel
      if(relcount>10)
        state=0;
    }

}//END DecodeUSerSwitch

/*
 ===================================================================
 CSU:    LimitInt()
 ===================================================================
 Clamp an int between a min and max.  
        
 ===================================================================
 */
void LimitInt(int *x,int Min, int Max)
{
  if(*x > Max)
    *x = Max;
  if(*x < Min)
    *x = Min;

}//END LimitInt

/*
 ===================================================================
 CSU:    LimitFloat()
 ===================================================================
 Clamp a float between a min and max.  Note doubles are the same 
 as floats on this platform.
        
 ===================================================================
 */
void LimitFloat(float *x,float Min, float Max)
{
  if(*x > Max)
    *x = Max;
  if(*x < Min)
    *x = Min;

}//END LimitInt

/*
 ===================================================================
 CSU:    DriveMotors()
 ===================================================================
 Desc:   Drive port / stbd motors fwd or backwards using PWM.
 A breakout calc is needed to linearise the response since
 torque is proportional to voltage on a DC mottor the wheels
 don't move on the lower 25% range.
 
 A L9110 IC controls the motors and 2 PWMs are used.  2 DOs control 
 direction.
 
 IA(DO) IB(PWM) Motor State 
 L      L       Off 
 H      L       Forward 
 L      H       Reverse 
 H      H       Off 

 Inputs
 -----
 DriveVal    +/-1000  

 Note
 ----
 Demand>     0  100 200 300 400 500 600 700 800 900 1000
 Distance>  33  70  93  110 128 140 151 164* 168 168 168

 Motor demands are linear ish up to 700 with AAA hybrio batteries.
 For PID Distance = 400 and PID gets 300
 ===================================================================
 */
void DriveMotors(int PDrive,int SDrive,byte Moving)
{
  int Mag;

  if(!Moving)
  {
    PDrive = 0;
    SDrive = 0;
  }
  
  LimitInt(&PDrive, -1000,1000);
  LimitInt(&SDrive, -1000,1000);

  // ========= Port drive =========
  Mag = abs(PDrive) * .205 + 45;
 if(PDrive == 0) Mag = 0;
  
  if(PDrive < 0)
  {
    // fwds
    digitalWrite(9, HIGH);
    analogWrite(3, 255 - Mag);
  }
  else
  {
    // backwards
    digitalWrite(9, LOW);
    analogWrite(3, Mag);
  } 

  // ========= Stbd drive =========
  Mag = abs(SDrive) * .205 + 45;
  if(SDrive == 0) Mag = 0;
  
  if(SDrive < 0)
  {
    // 
    digitalWrite(10, HIGH);
    analogWrite(11, 255 - Mag); //you haave to do this; look at the truth table
  }
  else
  {
    // fwd
    digitalWrite(10, LOW);
    analogWrite(11, Mag);
  }  
}//END DriveMotors


















/*
 *    delay(10000);
    for(i=0;i<1000;i++)
    {
      delay(100);
       DriveMotors( i*-1, i*-1);
       Serial.println(i);
    }
    Serial.print(Heading);
     Serial.print(" , ");
        Serial.print(HeadingTgt);
     Serial.print(" , ");
      Serial.println(Demand);   
      *
 */
