#define PH_PIN A0

int buf[10], temp;

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("================================");
    Serial.println("       pH Sensor Test");
    Serial.println("================================");
}

void loop()
{
    // Get 10 sample values from the sensor for smoothing
    for (int i = 0; i < 10; i++)
    {
        buf[i] = analogRead(PH_PIN);
        delay(10);
    }

    // Sort the values from small to large
    for (int i = 0; i < 9; i++)
    {
        for (int j = i + 1; j < 10; j++)
        {
            if (buf[i] > buf[j])
            {
                temp = buf[i];
                buf[i] = buf[j];
                buf[j] = temp;
            }
        }
    }

    // Average the middle 6 samples (discard 2 lowest, 2 highest)
    long avgValue = 0;
    for (int i = 2; i < 8; i++)
        avgValue += buf[i];

    float adcValue = avgValue / 6.0;
    float voltage = adcValue * (3.3 / 1023.0);

    // Calibrated using filtered water as neutral reference (~2.6255V = pH 7)
    float pH = 7 + ((2.7900 - voltage) / 0.18);

    Serial.println("--------------------------------");
    Serial.print("ADC Value : ");
    Serial.println(adcValue, 1);

    Serial.print("Voltage   : ");
    Serial.print(voltage, 3);
    Serial.println(" V");

    Serial.print("pH Value  : ");
    Serial.println(pH, 2);

    if (pH < 6.8)
    {
        Serial.println("Status    : Acidic");
    }
    else if (pH > 7.2)
    {
        Serial.println("Status    : Alkaline");
    }
    else
    {
        Serial.println("Status    : Neutral");
    }

    delay(1000);
}