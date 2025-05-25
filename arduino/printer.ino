int stepPin = 54; // X-axis step pin
int dirPin = 55;  // X-axis direction pin
int enablePin = 38; // Enable pin

void setup() {
  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);
  pinMode(enablePin, OUTPUT);
  
  digitalWrite(enablePin, LOW); // Enable the motor driver
}

void loop() {
  digitalWrite(dirPin, HIGH); // Set direction
  for (int i = 0; i < 200; i++) { // 200 steps for 1 revolution (depends on your motor)
    digitalWrite(stepPin, HIGH);
    // delayMicroseconds(500); // Adjust delay for speed
    // digitalWrite(stepPin, LOW);
    // delayMicroseconds(500);
  }
  delay(1000); // Wait for 1 second before the next move
}
