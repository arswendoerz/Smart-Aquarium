#include <Servo.h>
#include <Ultrasonic.h>

#define TRIG_PIN 9
#define ECHO_PIN 10
#define PUMP_PIN 8
#define SERVO_PIN 7

Ultrasonic ultrasonic(TRIG_PIN, ECHO_PIN);
Servo myservo;

unsigned long previousMillis = 0;
const long interval = 43200000; 

void setup() {
  Serial.begin(9600);
  pinMode(PUMP_PIN, OUTPUT);
  myservo.attach(SERVO_PIN);
  myservo.write(0);  // Initial position
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Check water level
  int distance = ultrasonic.read();
  Serial.print("Distance: ");
  Serial.println(distance);
  
  if (distance > 10) {
    digitalWrite(PUMP_PIN, HIGH);
    delay(5000);              
    digitalWrite(PUMP_PIN, LOW);   
  }

  
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    feedFish();
  }
  
  delay(1000);  // Wait for 1 second before the next loop
}

void feedFish() {
  myservo.write(90);  // Move servo to 90 degrees (adjust as needed to dispense food)
  delay(1000);        // Hold position for 1 second
  myservo.write(0);   // Move servo back to initial position
}
