/*
  ZMPT101B AC Voltage Reader (ESP32)

  Improvements:
  - Single-pass calculation of mean (midpoint) and RMS
  - Uses float for better precision
  - Cleaner and more efficient code
*/

const int sensorPin = 26;          // ZMPT101B Analog Output
const int numSamples = 1000;       // Number of samples per reading

// Calibration factor
// Change this after comparing with a multimeter.
float sensitivity = 0.5604;

void setup()
{
    Serial.begin(115200);
    Serial.println();
    Serial.println("==================================");
    Serial.println(" ZMPT101B AC Voltage Reader");
    Serial.println("==================================");
}

float readVoltageRMS()
{
    float sum = 0.0;
    float sumSquares = 0.0;

    int firstSample = 0;

    // Collect samples
    for (int i = 0; i < numSamples; i++)
    {
        int sample = analogRead(sensorPin);

        if (i == 0)
            firstSample = sample;

        sum += sample;
        sumSquares += (float)sample * sample;

        delayMicroseconds(100);
    }

    // Calculate midpoint (mean)
    float midpoint = sum / numSamples;

    // RMS of AC component
    float meanSquare = (sumSquares / numSamples) - (midpoint * midpoint);

    if (meanSquare < 0)
        meanSquare = 0;   // Prevent negative due to floating-point rounding

    float rmsRaw = sqrt(meanSquare);

    // Convert ADC RMS to AC Voltage
    float voltage = rmsRaw * sensitivity;

    // Debug Information
    Serial.println("----------------------------------");
    Serial.print("First Sample : ");
    Serial.println(firstSample);

    Serial.print("Midpoint     : ");
    Serial.println(midpoint, 2);

    Serial.print("Raw RMS      : ");
    Serial.println(rmsRaw, 2);

    Serial.print("Voltage      : ");
    Serial.print(voltage, 2);
    Serial.println(" V");

    return voltage;
}

void loop()
{
    readVoltageRMS();
    delay(1000);
}