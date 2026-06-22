const int sensorPin = 26;
int sensorState;
int lastState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

void setup() {
  Serial.begin(115200);
  pinMode(sensorPin, INPUT_PULLUP);
}

void loop() {
  int reading = digitalRead(sensorPin);

  if (reading != lastState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != sensorState) {
      sensorState = reading;

      if (sensorState == LOW) {
        Serial.println("Water Level HIGH - Tank Full!");
      } else {
        Serial.println("Water Level LOW - No Water!");
      }
    }
  }

  lastState = reading;
}