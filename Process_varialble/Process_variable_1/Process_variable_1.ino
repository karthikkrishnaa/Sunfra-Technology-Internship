#include <ModbusMaster.h>

ModbusMaster node;

#define RXD2 16
#define TXD2 17

void setup() {
  Serial.begin(115200);

  // RS485 UART
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  // Modbus Slave ID = 1 (from your datasheet)
  node.begin(1, Serial2);

  Serial.println("Reading ONLY Register 0...");
}

void loop() {

  uint8_t result = node.readHoldingRegisters(0, 1);  // ONLY Reg 0

  if (result == node.ku8MBSuccess) {

    uint16_t reg0 = node.getResponseBuffer(0);

    Serial.print("Reg 0 = ");
    Serial.println(reg0);

  } else {
    Serial.print("Modbus Error: ");
    Serial.println(result);
  }

  delay(500);
}