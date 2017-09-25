/* An Arduino project to open/close miniblinds to help regulate temperature.
*/
#include <Servo.h>
#include <EEPROM.h>

/* Variables. (Changing these changes the behavior of the device.)*/
int ledBrightness = 150;  // Temp indicator brightness. 0 = full brightness, 255 = off.
int servoSpeed = 200;    // Servo rotation speed. 30 = slowest, 200 = fastest 
int maxServoTime = 9;     // Absolute maximum time the servo will turn. In seconds.
int sensitivity = 2;      // Number of degrees C either side of the target temp before the blinds change. Higher = blinds open/close less. Lower = blinds open/close more. 

/* Don't alter this other stuff. 
*/
// Pin assignments
int tempSensor = A0;      // JayCar LM335Z Temp sensor.
int calibrationPot = A1;  // 10K potentiometer.
int redLED = 3;           // 
int greenLED = 5;         // Connected to a Common Anode RGB LED.
int blueLED = 6;          // 
int manualButton = 4;     // A toggle, slide, or push-on push-off switch.
int upButton = 7;         // Momentary button
int downButton = 8;       // Momentary button
int servoPin = 9;         // Parallax Continuous Rotation Servo 900-00008

Servo blindServo;         // Create Servo object to control the blinds.

// Some constants.
int OPEN = 0;                // The state of the blinds.
int CLOSED = 1;              // 
int ledOFF = 255;            // PWM values for the color LED. 
int ledON = 0;               //
int servoNeutral = 1500;     // Microsecond pulse length for the servo to stay still.
int calibrationCenter = 512; // The midpoint of the calibration potentiometer.
int tempSamples = 100;       // Number of temperature samples to average together for a reading.
int tempUnits = 13;          // The number of values in the array below.
int redVals[] =   {0,   0,  0,   0,  0,  0,   0, 20, 35, 50, 75, 90, 100}; // These determine the color mix of the RGB led. (0-100%)
int greenVals[] = {0,   10, 25, 50, 70, 90, 100, 90, 70, 50, 35, 10,   0}; // (From hottest to coldest. Middle value is ideal temperature.)
int blueVals[] =  {100, 90, 75, 50, 35, 20,   0,  0,  0,  0,  0,  0,   0};          //
int thermostatMin = 10;            // Max and min Degrees C. that the  
int thermostatMax = 30;            // thermostat can be set to.

// Some globals.
long int tempAvg = 0;                  // Collects the temperature readings to average later.
int tempReadingCount = 0;              // How many temperature readings that need to be averaged.
int temperature = 0;                   // Current temperature. (In C)
int targetTemp = 20;                   // The temperature that we want it to be (in C)
boolean currentBlindStatus = CLOSED;   // Are the blinds currently open or closed?
int manualMode = LOW;                  // LOW if it's running on automatic. HIGH if running in manual mode.
boolean thermostatChanged = false;     // True if the user has changed the thermostat (targetTemp) and it hasn't yet been saved to memory.

void setup() { 
  
  Serial.begin(9600);       // For debugging output.
  
  // Attach everything to their pins.
  pinMode(redLED, OUTPUT); 
  analogWrite(redLED, ledOFF);
  pinMode(greenLED, OUTPUT); 
  analogWrite(greenLED, ledOFF);
  pinMode(blueLED, OUTPUT); 
  analogWrite(blueLED, ledOFF);
  pinMode(manualButton, INPUT); 
  pinMode(upButton, INPUT); 
  pinMode(downButton, INPUT); 

  // Get a rough starting temperature.
  for (int i=0; i < 10; i++){
    tempAvg += getTemperature();
    delay(100);
  }
  temperature = tempAvg/10;
  tempAvg = 0;
  updateTemperatureLED(temperature);
  
  // load the preferred temperature and the last state of the blinds from persistent memory.
  loadSettings();
  
  Serial.print("Thermostat setting: ");
  Serial.println(targetTemp);
  Serial.print("Current temperature: ");
  Serial.println(temperature);
  Serial.print("Blinds are ");
  if (currentBlindStatus == OPEN){
    Serial.println("open.");
  } else {
    Serial.println("closed.");
  }
} 

void loop() { 

  manualMode = digitalRead(manualButton); // See if we're in manual or automatic mode.
  
  tempAvg += getTemperature();     // take a temperature reading.
  tempReadingCount ++;
  
  if (tempReadingCount > tempSamples){    // If we have enough samples, calculate the average.
    temperature = tempAvg/tempReadingCount;
    Serial.print("Temperature: ");
    Serial.println(temperature);
    updateTemperatureLED(temperature);
    tempAvg = 0;
    tempReadingCount = 0;
    if (manualMode == LOW){                  // If we're in automatic mode check to see if we need to open or close the blinds.
      if (temperature - targetTemp >= sensitivity){
        closeBlinds();
      } else if (temperature - targetTemp <= -sensitivity){
        openBlinds(); 
      }
      if (thermostatChanged == true){ // If the thermostat temp has changed, save the new value to EEPROM.
        saveSettings();
        thermostatChanged = false;
      }
    }
  }
  
  if (manualMode == HIGH){                 // If we're in manual mode...
     updateTemperatureLED(temperature);    // Turn the indicator light to white.
     if (digitalRead(upButton) == HIGH){   // If the up button is pressed open the blinds.
       if (currentBlindStatus == CLOSED){
         openBlinds();
       } else {                            // If they're already open, blink the light so the user knows pushing the button is futile.
         for (int i=0;i<5;i++){
           delay(50);
           analogWrite(redLED, ledOFF);
           analogWrite(greenLED, ledOFF);
           analogWrite(blueLED, ledOFF);
           delay(50);
           updateTemperatureLED(temperature);
         }
       }
     } else if (digitalRead(downButton) == HIGH){  // Close the blinds if the down button is pressed.
       if (currentBlindStatus == OPEN){
         closeBlinds();
       } else {
         for (int i=0;i<5;i++){
           delay(50);                              // If they're already closed, blink the light so the user knows pushing the button is futile.
           analogWrite(redLED, ledOFF);
           analogWrite(greenLED, ledOFF);
           analogWrite(blueLED, ledOFF);
           delay(50);
           updateTemperatureLED(temperature);
         }
       }
     }
     delay(50);    // A short delay. We're not in too much of a hurry.

  } else {                                   // Begin automatic mode
     updateTemperatureLED(temperature);
     // Check for button presses to adjust temperature.
     if (digitalRead(upButton) == HIGH){     // Raise the target temperature.
        delay(10);   // Primitive debounce
        if ((digitalRead(upButton) == HIGH) && (targetTemp < thermostatMax)){
           targetTemp ++;
           updateTemperatureLED(temperature);
           thermostatChanged = true;
           delay(250);    // Delay to keep from changing the temperature too fast when the button is held down.
        }
     }

     if (digitalRead(downButton) == HIGH){    // Lower the target temperature.
        delay(10);   // Primitive debounce
        if ((digitalRead(downButton) == HIGH) && (targetTemp > thermostatMin)){
           targetTemp --;
           updateTemperatureLED(temperature);
           thermostatChanged = true;
           delay(250);    // Delay to keep from changing the temperature too fast when the button is held down.
        }
     }
     delay(50);
  }
}


// Get the current temperature (in degrees C)
// Converts the value of the analog input on the TMP36 temp sensor to Celcius.
int getTemperature(){
  float voltage = (analogRead(tempSensor) * 5.0) / 1024.0;
  int temp = (voltage - 0.5) * 100;
  return temp;
}

// Open the blinds. (If they're not already open.)
void openBlinds(){
  if (currentBlindStatus == CLOSED){
    blindServo.attach(servoPin); 
    int calibration = getCalibration();
    if (isOpenClockwise(calibration) == true){
      blindServo.writeMicroseconds(servoNeutral - servoSpeed);  // Clockwise
      delay(turnTime(calibration));
      blindServo.writeMicroseconds(servoNeutral);  // Stop
    } else {
      blindServo.writeMicroseconds(servoNeutral + servoSpeed);  // Counterclockwise
      delay(turnTime(calibration));
      blindServo.writeMicroseconds(servoNeutral);  // Stop
    }
    currentBlindStatus = OPEN;
    saveSettings();
    blindServo.detach(); 
  }
}

// Close the blinds. (If they're not already closed.)
void closeBlinds(){
  if (currentBlindStatus == OPEN){
    blindServo.attach(servoPin); 
    int calibration = getCalibration();
    if (isOpenClockwise(calibration) == true){
      blindServo.writeMicroseconds(servoNeutral + servoSpeed);  // Counterclockwise
      delay(turnTime(calibration));
      blindServo.writeMicroseconds(servoNeutral);  // Stop
    } else {
      blindServo.writeMicroseconds(servoNeutral - servoSpeed);  // Clockwise
      delay(turnTime(calibration));
      blindServo.writeMicroseconds(servoNeutral);  // Stop
    }
    currentBlindStatus = CLOSED;
    saveSettings();
    blindServo.detach(); 
  }
}

// Read the position/value of the calibration potentiometer.
int getCalibration(){
  // Do it a few times to average out any noise.
  int totalVal = 0;
  int total = 10;
  for (int i = 0;i<total; i++){
    totalVal += analogRead(calibrationPot);
    delay(15);
  }
  return totalVal/total;
}


// Returns true if rotating the servo clockwise will open the blinds.
// This is based on the calibration pot value.
boolean isOpenClockwise(int calibrationPotVal){
   if (calibrationPotVal > calibrationCenter){
    return true;
  } else {
    return false;
  }
}

// Returns the time (in milliseconds) to rotate the servo based on the 
// calibration pot location.
int turnTime(int calibrationPotVal){
  int val = abs(map(calibrationPotVal - calibrationCenter, 0, 512, 0, maxServoTime*1000));
  return val;
}

// Sets the color of the temperature LED.
// Display depends on how many units it is away from the preferred temp.
// Too cold = toward blue, good = green, too warm = toward red. 
// Manual mode = white.
void updateTemperatureLED(int temp){
  if (manualMode == 0){
    int difference = temp - targetTemp;
  
    int location = difference + (tempUnits/2);
    if (location < 0){
       location = 0;
    } else if (location > tempUnits - 1){
      location = tempUnits - 1;
    }
    analogWrite(redLED, map(redVals[location], 0, 100, ledOFF, ledBrightness));
    analogWrite(greenLED, map(greenVals[location], 0, 100, ledOFF, ledBrightness));
    analogWrite(blueLED, map(blueVals[location], 0, 100, ledOFF, ledBrightness));
  } else { // manual mode is on. Turn LED white-ish.
    analogWrite(redLED,  map(25, 0, 100, ledOFF, ledBrightness));
    analogWrite(greenLED, map(50, 0, 100, ledOFF, ledBrightness));
    analogWrite(blueLED, map(60, 0, 100, ledOFF, ledBrightness));
  }
}


/* ---------------------------------------------------------------------------
   EEPROM Stuff:
   
   Notes on reading and writing from EEPROM:
   Since we can't naturally count on the state of the EEPROM (we don't know 
   if it contains good data or not) we check the first two characters
   for a special validation code. If that code matches, we assume the rest of the data is good.
   If not, we assume the data is bad. Change the validation code characters when
   using with other projects.
*/

char validationCode[] = "hi";

// load the open/close state and preferred temperature from EEPROM.
void loadSettings(){
  char loadedCode[] = "  ";
  loadedCode[0] = EEPROM.read(0);
  loadedCode[1] = EEPROM.read(1);
  if ((loadedCode[0] == validationCode[0]) && (loadedCode[1] == validationCode[1])){
    // Read preferred temperature (a 2-byte int.)
    int a=EEPROM.read(2);
    int b=EEPROM.read(3);
    targetTemp = (a * 256) + b;
    // read the blind status (technically 1 bit, but it's easier to read a whole bit anyway.
    currentBlindStatus = EEPROM.read(4);
  } else { // valid data not found. Set up some default data.
    targetTemp = 20;
    currentBlindStatus = CLOSED;
    saveSettings();
  } 
}

// Save the open/close state and preferred temperature to EEPROM.
void saveSettings(){
  // Write validation code.
  EEPROM.write(0, validationCode[0]);
  EEPROM.write(1, validationCode[1]);
  // Write preferred temperature (a 2-byte int).
  int a = targetTemp / 256;
  int b = targetTemp % 256;
  EEPROM.write(2,a);
  EEPROM.write(3,b);
  // Write blind status (A boolean.)
  EEPROM.write(4,currentBlindStatus);
  
  Serial.println("Saving settings to EEPROM.");
  Serial.print("Thermostat is set to: ");
  Serial.println(targetTemp);
  Serial.print("Blinds are ");
  if (currentBlindStatus == OPEN){
    Serial.println("open.");
  } else {
    Serial.println("closed.");
  }
}

