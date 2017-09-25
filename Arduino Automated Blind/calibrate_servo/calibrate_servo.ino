/* Simple script to calibrate the Parallax continuous rotation servo.

  Connect the servo to pin 9 and upload this script. Rotate the center-stop 
  adjustment screw on the servo until the servo doesn't rotate in either direction.
  (The screw is right above where they cable conects to the servo.)
  
*/
#include <Servo.h>
Servo servo;

void setup() { 
  servo.attach(9);
  servo.writeMicroseconds(1500);
} 

void loop() {
}
