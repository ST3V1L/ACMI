/* Simple script to test the Parallax continuous rotation servo.

  When the servo is attached to pin 9 it should rotate back and
  forth, waiting two seconds between changes.
*/
#include <Servo.h>
Servo servo;

void setup() { 
  servo.attach(9);
} 

void loop() { 
  servo.writeMicroseconds(1700);  // Counter clockwise
  delay(2000);                      
  servo.writeMicroseconds(1500);  // Stop
  delay(2000);
  servo.writeMicroseconds(1300);  // Clockwise
  delay(2000); 
  servo.writeMicroseconds(1500);  // Stop
  delay(2000);
}
