
#if defined(ARDUINO) && ARDUINO >= 100
  #include "Arduino.h"
#else
  #include "WProgram.h"
  #include <pins_arduino.h>
#endif


#include "Zowi.h"
#include <Oscillator.h>
#include <US.h>


// Zowi::Zowi(int YL, int YR, int RL, int RR, int NoiseSensor, int Buzzer, int SecondButton, int ThirdButton, int USTrigger, int USEcho, bool load_calibration){
//   //Zowi::init(YL, YR, RL, RR, NoiseSensor, Buzzer, SecondButton, ThirdButton, USTrigger, USEcho, load_calibration);
// }

void Zowi::init(int YL, int YR, int RL, int RR, bool load_calibration, int NoiseSensor, int Buzzer, int USTrigger, int USEcho) {
  
  Zowi::servo_pins[0] = YL;
  Zowi::servo_pins[1] = YR;
  Zowi::servo_pins[2] = RL;
  Zowi::servo_pins[3] = RR;

  attachServos();

  if (load_calibration) {
    for (int i = 0; i < 4; i++) {
      int servo_trim = EEPROM.read(i);
      if (servo_trim > 128) servo_trim -= 256;
      servo[i].SetTrim(servo_trim);
    }
  }
  
  for (int i = 0; i < 4; i++) servo_position[i] = 90;

  //US sensor init with the pins:
  us.init(USTrigger, USEcho);

  //Buzzer & noise sensor pins: 
  Zowi::pinBuzzer = Buzzer;
  Zowi::pinNoiseSensor = NoiseSensor;

  pinMode(Buzzer,OUTPUT);
  pinMode(NoiseSensor,INPUT);

}

void Zowi::attachServos(){
    servo[0].attach(servo_pins[0]);
    servo[1].attach(servo_pins[1]);
    servo[2].attach(servo_pins[2]);
    servo[3].attach(servo_pins[3]);
}

void Zowi::detachServos(){
    servo[0].detach();
    servo[1].detach();
    servo[2].detach();
    servo[3].detach();
}

void Zowi::setTrims(int YL, int YR, int RL, int RR) {
  servo[0].SetTrim(YL);
  servo[1].SetTrim(YR);
  servo[2].SetTrim(RL);
  servo[3].SetTrim(RR);
}

void Zowi::saveTrimsOnEEPROM() {
  for (int i = 0; i < 4; i++) 
      EEPROM.write(i, servo[i].getTrim());
}

void Zowi::moveServos(int time, int  servo_target[]) {

  attachServos();

  if(time>10){
    for (int i = 0; i < 4; i++) increment[i] = ((servo_target[i]) - servo_position[i]) / (time / 10.0);
    final_time =  millis() + time;

    for (int iteration = 1; millis() < final_time; iteration++) {
      partial_time = millis() + 10;
      for (int i = 0; i < 4; i++) servo[i].SetPosition(servo_position[i] + (iteration * increment[i]));
      while (millis() < partial_time); //pause
    }
  }
  else{
    for (int i = 0; i < 4; i++) servo[i].SetPosition(servo_target[i]);
  }
  for (int i = 0; i < 4; i++) servo_position[i] = servo_target[i];


}

void Zowi::oscillateServos(int A[4], int O[4], int T, double phase_diff[4], float cycle=1){

  for (int i=0; i<4; i++) {
    servo[i].SetO(O[i]);
    servo[i].SetA(A[i]);
    servo[i].SetT(T);
    servo[i].SetPh(phase_diff[i]);
  }
  double ref=millis();
   for (double x=ref; x<=T*cycle+ref; x=millis()){
     for (int i=0; i<4; i++){
        servo[i].refresh();
     }
  }

}

void Zowi::_execute(int A[4], int O[4], int T, double phase_diff[4], float steps = 1.0){

  attachServos();

  int cycles=(int)steps;    

  //-- Execute complete cycles
  if (cycles >= 1) 
    for(int i = 0; i < cycles; i++) 
      oscillateServos(A,O, T, phase_diff);
      
  //-- Execute the final not complete cycle    
  oscillateServos(A,O, T, phase_diff,(float)steps-cycles);

}



//--------------------------------
//-- Zowi at rest position
//--------------------------------
void Zowi::home(){

  int homes[4]={90, 90, 90, 90}; //All the servos at rest position
  moveServos(500,homes);   //Move the servos in half a second

  detachServos();
  
}


//**************************************************
//** MOVEMENTS *************************************
//**************************************************

//------------------------------------------------
//-- Zowi gait: Walking  (forward or backward)    
//-- Parameters:
//--    * steps:  Number of steps
//--    * T : Period
//--    * Dir: Direction: FORWARD / BACKWARD
//------------------------------------------------
void Zowi::walk(float steps, int T, int dir){

  //-- Oscillator parameters for walking
  //-- Hip sevos are in phase
  //-- Feet servos are in phase
  //-- Hip and feet are 90 degrees out of phase
  //--      -90 : Walk forward
  //--       90 : Walk backward
  //-- Feet servos also have the same offset (for tiptoe a little bit)
  int A[4]= {30, 30, 20, 20};
  int O[4] = {0, 0, 4, -4};
  double phase_diff[4] = {0, 0, DEG2RAD(dir * -90), DEG2RAD(dir * -90)};

  //-- Let's oscillate the servos!
  _execute(A, O, T, phase_diff, steps);  
}

//----------------------------------------------------
//-- Zowi gait: Turning (left or right)
//-- Parameters:
//--   * Steps: Number of steps
//--   * T: Period
//--   * Dir: Direction: LEFT / RIGHT
//-----------------------------------------------------
void Zowi::turn(float steps, int T, int dir){

  //-- Same coordination than for walking (see Zowi::walk)
  //-- The Amplitudes of the hip's oscillators are not igual
  //-- When the right hip servo amplitude is higher, the steps taken by
  //--   the right leg are bigger than the left. So, the robot describes an 
  //--   left arc
  int A[4]= {30, 30, 20, 20};
  int O[4] = {0, 0, 4, -4};
  double phase_diff[4] = {0, 0, DEG2RAD(-90), DEG2RAD(-90)}; 
    
  if (dir == LEFT) {  
    A[0] = 30; //-- Left hip servo
    A[1] = 10; //-- Right hip servo
  }
  else {
    A[0] = 10;
    A[1] = 30;
  }
    
  //-- Let's oscillate the servos!
  _execute(A, O, T, phase_diff, steps); 
}


//------------------------------------------------------
//-- Zowi movement: up-down
//--   Zowi moves up and down
//--
//-- Parameters:
//--    * steps: Number of jumps
//--    * T: Period
//--    * h: Jump height: SMALL / MEDIUM / BIG 
//--              (or a number in degrees 0 - 90)
//----------------------------------------------------- 
void Zowi::updown(float steps, int T, int h){

  //-- Both feet are 180 degrees out of phase
  //-- Feet amplitude and offset are the same
  //-- Initial phase for the right foot is -90, so that it starts
  //--   in one extreme position (not in the middle)
  int A[4]= {0, 0, h, h};
  int O[4] = {0, 0, h, -h};
  double phase_diff[4] = {0, 0, DEG2RAD(-90), DEG2RAD(90)};
  
  //-- Let's oscillate the servos!
  _execute(A, O, T, phase_diff, steps); 
}

//-----------------------------------------------------------------
//-- Zowi gait: Moonwalker
//--   Zowi moves like Michael Jackson
//--
//--  Parameters:
//--    Steps: Number of steps
//--    T: Period
//--    h: Height. Typical valures between 15 and 40
//--    dir: Direction: LEFT / RIGHT
//------------------------------------------------------------------
void Zowi::moonwalker(float steps, int T, int h, int dir){

  //-- This motion is similar to that of the caterpillar robots: A travelling
  //-- wave moving from one side to another
  //-- The two Zowi's feet are equivalent to a minimal configuration. It is known
  //-- that 2 servos can move like a worm if they are 120 degrees out of phase
  //-- In the example of Zowi, the two feet are mirrored so that we have:
  //--    180 - 120 = 60 degrees. The actual phase difference given to the oscillators
  //--  is 60 degrees.
  //--  Both amplitudes are equal. The offset is half the amplitud plus a little bit of
  //-   offset so that the robot tiptoe lightly
 
  int A[4]= {0, 0, h, h};
  int O[4] = {0, 0, h/2+2, -h/2 -2};
  int phi = -dir * 90;
  double phase_diff[4] = {0, 0, DEG2RAD(phi), DEG2RAD(-60 * dir + phi)};
  
  //-- Let's oscillate the servos!
  _execute(A, O, T, phase_diff, steps); 
}

//---------------------------------------------------------
//-- Zowi movement: swinging side to side
//--
//--  Parameters:
//--     steps: Number of steps
//--     T : Period
//--     h : Amount of swing (from 0 to 50 aprox)
//-----------------------------------------------------------
void Zowi::swing(float steps, int T, int h){

  //-- Both feets are in phase. The offset is half the amplitude
  //-- It causes the robot to swing from side to side
  int A[4]= {0, 0, h, h};
  int O[4] = {0, 0, h/2, -h/2};
  double phase_diff[4] = {0, 0, DEG2RAD(0), DEG2RAD(0)};
  
  //-- Let's oscillate the servos!
  _execute(A, O, T, phase_diff, steps); 
}

//---------------------------------------------------------
//-- Zowi movement: swinging side to side without touching the floor with the heel
//--
//--  Parameters:
//--     steps: Number of steps
//--     T : Period
//--     h : Amount of swing (from 0 to 50 aprox)
//-----------------------------------------------------------
void Zowi::tiptoeSwing(float steps, int T, int h){

  //-- Both feets are in phase. The offset is not half the amplitude in order to tiptoe
  //-- It causes the robot to swing from side to side
  int A[4]= {0, 0, h, h};
  int O[4] = {0, 0, h, -h};
  double phase_diff[4] = {0, 0, 0, 0};
  
  //-- Let's oscillate the servos!
  _execute(A, O, T, phase_diff, steps); 
}

//-----------------------------------------------------------------
//-- Zowi gait: Crusaito: a mixture between moonwalker and walk
//--
//--   Parameters:
//--     steps: Number of steps
//--     T: Period
//--     h: height (Values between 20 - 50)
//--     dir:  Direction: LEFT / RIGHT
//------------------------------------------------------------------
void Zowi::crusaito(float steps, int T, int h, int dir){

  int A[4]= {25, 25, h, h};
  int O[4] = {0, 0, h/2+ 4, -h/2 - 4};
  double phase_diff[4] = {90, 90, DEG2RAD(0), DEG2RAD(-60 * dir)};
  
  //-- Let's oscillate the servos!
  _execute(A, O, T, phase_diff, steps); 
}


//-----------------------------------------------------
//-- Zowi movement: Jump
//--
//--  Parameters:
//--    steps: Number of steps
//--    T: Period
//------------------------------------------------------
void Zowi::jump(float steps, int T){

  int up[]={90,90,150,30};
  moveServos(T,up);
  int down[]={90,90,90,90};
  moveServos(T,down);
}

//-----------------------------------------------------
//-- Zowi gait: Flapping
//--
//--  Parameters:
//--    steps: Number of steps
//--    T: Period
//--    h: height (Values between 10 - 30)
//--    dir: direction: FOREWARD, BACKWARD
//------------------------------------------------------
void Zowi::flapping(float steps, int T, int h, int dir){

  int A[4]= {12, 12, h, h};
  int O[4] = {0, 0, h - 10, -h + 10};
  double phase_diff[4] = {DEG2RAD(0), DEG2RAD(180), DEG2RAD(-90 * dir), DEG2RAD(90 * dir)};
  
  //-- Let's oscillate the servos!
  _execute(A, O, T, phase_diff, steps); 
}

//-----------------------------------------------------
//-- Zowi gait: Shake a leg
//--
//--  Parameters:
//--    steps: Number of shakes
//--    T: Period of one shake
//--    dir: RIGHT=Right leg LEFT=Left leg
//------------------------------------------------------
void Zowi::shakeLeg (int steps,int T,int dir){

  //This variable change the amount of shakes
  int numberLegMoves=2;

  //Parameters of all the movements. Default: Right leg
  int shake_leg1[4]={90, 90, 58, 35};   
  int shake_leg2[4]={90, 90, 58, 120};
  int shake_leg3[4]={90, 90, 58, 60};
  int homes[4]={90, 90, 90, 90};

  //Changes in the parameters if left leg is chosen
  if(dir==1)      
  {
    shake_leg1[2]=180-35;
    shake_leg1[3]=180-58;
    shake_leg2[2]=180-120;
    shake_leg2[3]=180-58;
    shake_leg3[2]=180-60;
    shake_leg3[3]=180-58;
  }
  
  //Time of the bend movement. Fixed parameter to avoid falls
  int T2=1000;    
  //Time of one shake, constrained in order to avoid movements too fast.            
  T=T-T2;
  T=max(T,200*numberLegMoves);  

  for (int j=0; j<steps;j++)
  {
  //Bend movement
  moveServos(T2/2,shake_leg1);
  moveServos(T2/2,shake_leg2);
  
    //Shake movement
    for (int i=0;i<numberLegMoves;i++)
    {
    moveServos(T/(2*numberLegMoves),shake_leg3);
    moveServos(T/(2*numberLegMoves),shake_leg2);
    }
    moveServos(500,homes); //Return to home position
  }
  delay(T);
}

//-----------------------------------------------------
//-- Zowi gait: Lateral bend
//--
//--  Parameters:
//--    steps: Number of bends
//--    T: Period of one bend
//--    dir: RIGHT=Right bend LEFT=Left bend
//------------------------------------------------------

void Zowi::bend (int steps, int T, int dir){

  //Parameters of all the movements. Default: Left bend
  int bend1[4]={90, 90, 58, 35};
  int bend2[4]={90, 90, 58, 105};
  int homes[4]={90, 90, 90, 90};
  //Time of one bend, constrained in order to avoid movements too fast.
  T=max(T, 600);

  //Changes in the parameters if right direction is chosen 
  if(dir==-1)
  {
    bend1[2]=180-35;
    bend1[3]=180-58;
    bend2[2]=180-105;
    bend2[3]=180-58;
  }
  //Bend movement
  for (int i=0;i<steps;i++)
  {
  moveServos(T/4,bend1);
  moveServos(T/10,bend2);
  delay(3*T/4);
  moveServos(500,homes);
  }

}

//-----------------------------------------------------
//-- Zowi gait: Jitter 
//--
//--  Parameters:
//--    steps: Number of jitters
//--    T: Period of one jitter 
//--    h: height (Values between 5 - 25)   
//------------------------------------------------------

void Zowi::jitter(float steps, int T, int h){

  //-- Both feet are 180 degrees out of phase
  //-- Feet amplitude and offset are the same
  //-- Initial phase for the right foot is -90, so that it starts
  //--   in one extreme position (not in the middle)
  //-- h is constrained to avoid hit the feets
  h=min(25,h);
  int A[4]= {h, h, 0, 0};
  int O[4] = {0, 0, 0, 0};
  double phase_diff[4] = {DEG2RAD(-90), DEG2RAD(90), 0, 0};
  
  //-- Let's oscillate the servos!
  oscillateServos(A, O, T, phase_diff, steps); 
}


//-----------------------------------------------------
//-- Zowi gait: Jitter while up&down
//--
//--  Parameters:
//--    steps: Number of bends
//--    T: Period of one bend
//--    h: height (Values between 5 - 15) 
//------------------------------------------------------
void Zowi::ascendingTurn(float steps, int T, int h){

  //-- Both feet and legs are 180 degrees out of phase
  //-- Initial phase for the right foot is -90, so that it starts
  //--   in one extreme position (not in the middle)
  //-- h is constrained to avoid hit the feets
  h=min(15,h);
  int A[4]= {h, h, h, h};
  int O[4] = {0, 0, h+4, -h+4};
  double phase_diff[4] = {DEG2RAD(-90), DEG2RAD(90), DEG2RAD(-90), DEG2RAD(90)};
  
  //-- Let's oscillate the servos!
  _execute(A, O, T, phase_diff, steps); 
}



//**************************************************
//** SENSORS ***************************************
//**************************************************

//-----------------------------------------------------
//-- Zowi getDistance: return zowi's ultrasonic sensor measure
//------------------------------------------------------
float Zowi::getDistance(){
  return us.read();
}

//-----------------------------------------------------
//-- Zowi getNoise: return zowi's noise sensor measure
//------------------------------------------------------
int Zowi::getNoise(){

  int noiseLevel = 0;
  int noiseReadings = 0;
  int numReadings = 2;

    for(int i=0; i<numReadings; i++){
        noiseReadings += analogRead(pinNoiseSensor);
        delay(2); // delay in between reads for stability
    }

    noiseLevel = noiseReadings / numReadings;

    return noiseLevel;
}


//-----------------------------------------------------
//-- Zowi getBatteryLevel: return battery voltage percent
//------------------------------------------------------
double Zowi::getBatteryLevel(){

  //The first read of the batery is often a wrong reading, so we will discard this value. 
    double batteryLevel = battery.readBatPercent();
    double batteryReadings = 0;
    int numReadings = 10;

    for(int i=0; i<numReadings; i++){
        batteryReadings += battery.readBatPercent();
        delay(1); // delay in between reads for stability
    }

    batteryLevel = batteryReadings / numReadings;

    return batteryLevel;
}

double Zowi::getBatteryVoltage(){

  //The first read of the batery is often a wrong reading, so we will discard this value. 
    double batteryLevel = battery.readBatVoltage();
    double batteryReadings = 0;
    int numReadings = 10;

    for(int i=0; i<numReadings; i++){
        batteryReadings += battery.readBatVoltage();
        delay(1); // delay in between reads for stability
    }

    batteryLevel = batteryReadings / numReadings;

    return batteryLevel;
}


//**************************************************
//** MOUTHS & ANIMATIONS ***************************
//**************************************************

unsigned long int Zowi::getMouthShape(int number){
  unsigned long int types []={zero_code,one_code,two_code,three_code,four_code,five_code,six_code,seven_code,eight_code,
  nine_code,smile_code,happyOpen_code,happyClosed_code,heart_code,bigSurprise_code,smallSurprise_code,tongueOut_code,
  vamp1_code,vamp2_code,lineMouth_code,confused_code,diagonal_code,sad_code,sadOpen_code,sadClosed_code,
  okMouth_code, xMouth_code,interrogation_code,thunder_code,culito_code,angry_code};

  return types[number];
}


unsigned long int Zowi::getAnimShape(int anim, int index){

      unsigned long int littleUuh_code[]={
         0b00000000000000001100001100000000,
         0b00000000000000000110000110000000,
         0b00000000000000000011000011000000,
         0b00000000000000000110000110000000,
         0b00000000000000001100001100000000,
         0b00000000000000011000011000000000,
         0b00000000000000110000110000000000,
         0b00000000000000011000011000000000  
      };

      unsigned long int dreamMouth_code[]={
         0b00000000000000000000110000110000,
         0b00000000000000010000101000010000,  
         0b00000000011000100100100100011000,
         0b00000000000000010000101000010000           
      };

      unsigned long int adivinawi_code[]={
         0b00100001000000000000000000100001,
         0b00010010100001000000100001010010,
         0b00001100010010100001010010001100,
         0b00000000001100010010001100000000,
         0b00000000000000001100000000000000,
         0b00000000000000000000000000000000
      };

      unsigned long int wave_code[]={
         0b00001100010010100001000000000000,
         0b00000110001001010000100000000000,
         0b00000011000100001000010000100000,
         0b00000001000010000100001000110000,
         0b00000000000001000010100100011000,
         0b00000000000000100001010010001100,
         0b00000000100000010000001001000110,
         0b00100000010000001000000100000011,
         0b00110000001000000100000010000001,
         0b00011000100100000010000001000000    
      };

  switch  (anim){

    case littleUuh:
        return littleUuh_code[index];
        break;
    case dreamMouth:
        return dreamMouth_code[index];
        break;
    case adivinawi:
        return adivinawi_code[index];
        break;
    case wave:
        return wave_code[index];
        break;    
  }  

  
}

void Zowi::putAnimationMouth(unsigned long int aniMouth, int index){

      ledmatrix.writeFull(Zowi::getAnimShape(aniMouth,index));
}


void Zowi::putMouth(unsigned long int mouth, bool predefined){

  if (predefined){
    ledmatrix.writeFull(Zowi::getMouthShape(mouth));
  }
  else{
    ledmatrix.writeFull(mouth);
  }
}


void Zowi::clearMouth(){

  ledmatrix.clearMatrix();
}


//**************************************************
//** SOUNDS ****************************************
//**************************************************

void Zowi::_tone (float noteFrequency, long noteDuration, int silentDuration){

    // tone(10,261,500);
    // delay(500);

      tone(Zowi::pinBuzzer, noteFrequency, noteDuration);
      delay(noteDuration);       //milliseconds to microseconds
      //noTone(PIN_Buzzer);
      delay(silentDuration);     
}


void Zowi::bendTones (float initFrequency, float finalFrequency, float prop, long noteDuration, int silentDuration){

  //Examples:
  //  bendTones (880, 2093, 1.02, 18, 0);
  //  bendTones (note_A5, note_C7, 1.02, 18, 0);

  if(initFrequency < finalFrequency)
  {
      for (int i=initFrequency; i<finalFrequency; i=i*prop) {
          Zowi::_tone(i, noteDuration, silentDuration);
      }

  } else{

      for (int i=initFrequency; i>finalFrequency; i=i/prop) {
          Zowi::_tone(i, noteDuration, silentDuration);
      }
  }
}



void Zowi::sing(int songName){
  switch(songName){

    case S_connection:
      Zowi::_tone(note_E5,50,30);
      Zowi::_tone(note_E6,55,25);
      Zowi::_tone(note_A6,60,10);
    break;

    case S_disconnection:
      Zowi::_tone(note_E5,50,30);
      Zowi::_tone(note_A6,55,25);
      Zowi::_tone(note_E6,50,10);
    break;

    case S_buttonPushed:
      Zowi::bendTones (note_E6, note_G6, 1.03, 20, 2);
      delay(30);
      Zowi::bendTones (note_E6, note_D7, 1.04, 10, 2);
    break;

    case S_mode1:
      Zowi::bendTones (note_E6, note_A6, 1.02, 30, 10);  //1318.51 to 1760
    break;

    case S_mode2:
      Zowi::bendTones (note_G6, note_D7, 1.03, 30, 10);  //1567.98 to 2349.32
    break;

    case S_mode3:
      Zowi::_tone(note_E6,50,100); //D6
      Zowi::_tone(note_G6,50,80);  //E6
      Zowi::_tone(note_D7,300,0);  //G6
    break;

    case S_surprise:
      Zowi::bendTones(800, 2150, 1.02, 10, 1);
      Zowi::bendTones(2149, 800, 1.03, 7, 1);
    break;

    case S_OhOoh:
      Zowi::bendTones(880, 2000, 1.04, 8, 3); //A5 = 880
      delay(200);

      for (int i=880; i<2000; i=i*1.04) {
           Zowi::_tone(note_B5,5,10);
      }
    break;

    case S_OhOoh2:
      Zowi::bendTones(1880, 3000, 1.03, 8, 3);
      delay(200);

      for (int i=1880; i<3000; i=i*1.03) {
          Zowi::_tone(note_C6,10,10);
      }
    break;

    case S_cuddly:
      Zowi::bendTones(700, 900, 1.03, 16, 4);
      Zowi::bendTones(899, 650, 1.01, 18, 7);
    break;

    case S_sleeping:
      Zowi::bendTones(100, 500, 1.04, 10, 10);
      delay(500);
      Zowi::bendTones(400, 100, 1.04, 10, 1);
    break;

    case S_happy:
      Zowi::bendTones(1500, 2500, 1.05, 20, 8);
      Zowi::bendTones(2499, 1500, 1.05, 25, 8);
    break;

    case S_superHappy:
      Zowi::bendTones(2000, 6000, 1.05, 8, 3);
      delay(50);
      Zowi::bendTones(5999, 2000, 1.05, 13, 2);
    break;

    case S_happy_short:
      Zowi::bendTones(1500, 2000, 1.05, 15, 8);
      delay(100);
      Zowi::bendTones(1900, 2500, 1.05, 10, 8);
    break;

    case S_sad:
      Zowi::bendTones(880, 669, 1.02, 20, 200);
    break;

    case S_confused:
      Zowi::bendTones(1000, 1700, 1.03, 8, 2); 
      Zowi::bendTones(1699, 500, 1.04, 8, 3);
      Zowi::bendTones(1000, 1700, 1.05, 9, 10);
    break;

    case S_fart1:
      Zowi::bendTones(1600, 3000, 1.02, 2, 15);
    break;

    case S_fart2:
      Zowi::bendTones(2000, 6000, 1.02, 2, 20);
    break;

    case S_fart3:
      Zowi::bendTones(1600, 4000, 1.02, 2, 20);
      Zowi::bendTones(4000, 3000, 1.02, 2, 20);
    break;

  }

}



//**************************************************
//** GESTURES **************************************
//**************************************************

void Zowi::playGesture(int gesture){

  int sadPos[4]={110, 70, 20, 160};
  int bedPos[4]={100, 80, 40, 140};
  int fartPos_1[4]={90, 90, 145, 122}; //rightBend
  int fartPos_2[4]={90, 90, 80, 122};
  int fartPos_3[4]={90, 90, 145, 80};
  int confusedPos[4]={110, 70, 90, 90};
  int angryPos[4]={90, 90, 70, 110};
  int headLeft[4]={110, 110, 90, 90};
  int headRight[4]={70, 70, 90, 90};
  int fretfulPos[4]={90, 90, 90, 110};
  

  switch(gesture){

    case ZowiHappy: 
        Zowi::_tone(note_E5,50,30);
        Zowi::putMouth(smile);
        Zowi::sing(S_happy_short);
        Zowi::swing(1,800,20); 
        Zowi::sing(S_happy_short);

        Zowi::home();
        Zowi::putMouth(happyOpen);
    break;


    case ZowiSuperHappy:
        Zowi::putMouth(happyOpen);
        Zowi::sing(S_happy);
        Zowi::putMouth(happyClosed);
        Zowi::tiptoeSwing(1,500,20);
        Zowi::putMouth(happyOpen);
        Zowi::sing(S_superHappy);
        Zowi::putMouth(happyClosed);
        Zowi::tiptoeSwing(1,500,20); 

        Zowi::home();  
        Zowi::putMouth(happyOpen);
    break;


    case ZowiSad: 
        Zowi::putMouth(sad);
        Zowi::moveServos(700, sadPos);     
        Zowi::bendTones(880, 830, 1.02, 20, 200);
        Zowi::putMouth(sadClosed);
        Zowi::bendTones(830, 790, 1.02, 20, 200);  
        Zowi::putMouth(sadOpen);
        Zowi::bendTones(790, 740, 1.02, 20, 200);
        Zowi::putMouth(sadClosed);
        Zowi::bendTones(740, 700, 1.02, 20, 200);
        Zowi::putMouth(sadOpen);
        Zowi::bendTones(700, 669, 1.02, 20, 200);
        Zowi::putMouth(sad);
        delay(500);

        Zowi::home();
        delay(300);
        Zowi::putMouth(happyOpen);
    break;


    case ZowiSleeping:

        Zowi::moveServos(800, bedPos);     

        for(int i=0; i<4;i++){
          Zowi::putAnimationMouth(dreamMouth,0);
          Zowi::bendTones (100, 200, 1.04, 10, 10);
          Zowi::putAnimationMouth(dreamMouth,1);
          Zowi::bendTones (200, 300, 1.04, 10, 10);  
          Zowi::putAnimationMouth(dreamMouth,2);
          Zowi::bendTones (300, 500, 1.04, 10, 10);   
          delay(500);
          Zowi::putAnimationMouth(dreamMouth,1);
          Zowi::bendTones (400, 250, 1.04, 10, 1); 
          Zowi::putAnimationMouth(dreamMouth,0);
          Zowi::bendTones (250, 100, 1.04, 10, 1); 
          delay(500);
        } 

        Zowi::putMouth(lineMouth);
        Zowi::sing(S_cuddly);

        Zowi::home();  
        Zowi::putMouth(happyOpen);
    break;


    case ZowiFart:
        Zowi::moveServos(500,fartPos_1);
        delay(300);     
        Zowi::putMouth(lineMouth);
        Zowi::sing(S_fart1);  
        Zowi::putMouth(tongueOut);
        delay(250);
        Zowi::moveServos(500,fartPos_2);
        delay(300);
        Zowi::putMouth(lineMouth);
        Zowi::sing(S_fart2); 
        Zowi::putMouth(tongueOut);
        delay(250);
        Zowi::moveServos(500,fartPos_3);
        delay(300);
        Zowi::putMouth(lineMouth);
        Zowi::sing(S_fart3);
        Zowi::putMouth(tongueOut);    
        delay(300);

        Zowi::home(); 
        delay(500); 
        Zowi::putMouth(happyOpen);
    break;


    case ZowiConfused:
        Zowi::moveServos(300, confusedPos); 
        Zowi::putMouth(confused);
        Zowi::sing(S_confused);
        delay(500);

        Zowi::home();  
        Zowi::putMouth(happyOpen);
    break;


    case ZowiLove:
        Zowi::putMouth(heart);
        Zowi::sing(S_cuddly);
        Zowi::crusaito(2,1500,15,1);

        Zowi::home(); 
        Zowi::sing(S_happy_short);  
        Zowi::putMouth(happyOpen);
    break;


    case ZowiAngry: 
        Zowi::moveServos(300, angryPos); 
        Zowi::putMouth(angry);

        Zowi::_tone(note_A5,100,30);
        Zowi::bendTones(note_A5, note_D6, 1.02, 7, 4);
        Zowi::bendTones(note_D6, note_G6, 1.02, 10, 1);
        Zowi::bendTones(note_G6, note_A5, 1.02, 10, 1);
        delay(15);
        Zowi::bendTones(note_A5, note_E5, 1.02, 20, 4);
        delay(400);
        Zowi::moveServos(200, headLeft); 
        Zowi::bendTones(note_A5, note_D6, 1.02, 20, 4);
        Zowi::moveServos(200, headRight); 
        Zowi::bendTones(note_A5, note_E5, 1.02, 20, 4);

        Zowi::home();  
        Zowi::putMouth(happyOpen);
    break;


    case ZowiFretful: 
        Zowi::putMouth(angry);
        Zowi::bendTones(note_A5, note_D6, 1.02, 20, 4);
        Zowi::bendTones(note_A5, note_E5, 1.02, 20, 4);
        delay(300);
        Zowi::putMouth(lineMouth);

        for(int i=0; i<4; i++){
          Zowi::moveServos(100, fretfulPos);   
          Zowi::home();
        }

        Zowi::putMouth(angry);
        delay(500);

        Zowi::home();  
        Zowi::putMouth(happyOpen);
    break;


    case ZowiMagic:

        //Initial note frecuency = 400
        //Final note frecuency = 1000
        
        // Reproduce the animation four times
        for(int i = 0; i<4; i++){ 

          int noteM = 400; 

            for(int index = 0; index<6; index++){
              Zowi::putAnimationMouth(adivinawi,index);
              Zowi::bendTones(noteM, noteM+100, 1.04, 10, 10);    //400 -> 1000 
              noteM+=100;
            }

            Zowi::clearMouth();
            Zowi::bendTones(noteM-100, noteM+100, 1.04, 10, 10);  //900 -> 1100

            for(int index = 0; index<6; index++){
              Zowi::putAnimationMouth(adivinawi,index);
              Zowi::bendTones(noteM, noteM+100, 1.04, 10, 10);    //1000 -> 400 
              noteM-=100;
            }
        } 
 
        delay(300);
        Zowi::putMouth(happyOpen);
    break;


    case ZowiWave:
        
        // Reproduce the animation four times
        for(int i = 0; i<2; i++){ 

            int noteW = 500; 

            for(int index = 0; index<10; index++){
              Zowi::putAnimationMouth(wave,index);
              Zowi::bendTones(noteW, noteW+100, 1.02, 10, 10); 
              noteW+=101;
            }
            for(int index = 0; index<10; index++){
              Zowi::putAnimationMouth(wave,index);
              Zowi::bendTones(noteW, noteW+100, 1.02, 10, 10); 
              noteW+=101;
            }
            for(int index = 0; index<10; index++){
              Zowi::putAnimationMouth(wave,index);
              Zowi::bendTones(noteW, noteW-100, 1.02, 10, 10); 
              noteW-=101;
            }
            for(int index = 0; index<10; index++){
              Zowi::putAnimationMouth(wave,index);
              Zowi::bendTones(noteW, noteW-100, 1.02, 10, 10); 
              noteW-=101;
            }
        }    

        Zowi::clearMouth();
        delay(300);
        Zowi::putMouth(happyOpen);
    break;

    case ZowiVictory:
        
        Zowi::clearMouth();
        Zowi::putMouth(happyOpen);
    break;

    case ZowiFail:
        
        Zowi::clearMouth();
        Zowi::putMouth(happyOpen);
    break;

  }
}    