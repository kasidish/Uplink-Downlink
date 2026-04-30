// include the library
#include <RadioLib.h>
#define TRANSMIT 1
// #define RECEIVE 1

SPIClass RADIO_SPI(PA7, PA6, PA5, -1);
SX1278 radio = new Module(PB6, PA10, PC7, -1, RADIO_SPI);


void setup() {
  Serial.setTx(PA2);
  Serial.setRx(PA3);
  Serial.println("Hello World");

  Serial.begin(115200);
  // initialize SX1278 with default settings
  Serial.print(F("[SX1278] Initializing ... "));
  RADIO_SPI.begin();
  int state = radio.beginFSK(435.0);
  radio.setDataShaping(RADIOLIB_SHAPING_0_5);
  // state += radio.setDataShaping(RADIOLIB_SHAPING_0_5);
  // state += radio.setFrequencyDeviation(1.2);
  state += radio.setOutputPower(12);
  // radio.setFrequencyDeviation(2.4);
  FSKRate_t fskRate = {
    .bitRate = 9.6,
    .freqDev = 4.8,
  };
  DataRate_t dataRate = {
    .fsk = fskRate
  };
  radio.setDataRate(dataRate);
  // uint8_t syncWord[] = { 0xAA, 0xAA, 0xAA, 0x7E };
  // state = radio.setSyncWord(syncWord, 4);
  uint8_t syncWord[] = { 0x01, 0x23, 0x45, 0x67,
                         0x89, 0xAB, 0xCD, 0xEF };
  state = radio.setSyncWord(syncWord, 8);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }
  radio.startReceive(2000);
  delay(1000);
}


#if defined(TRANSMIT)
int count = 0;

void loop() {
  Serial.print(F("[SX1278] Transmitting packet ... "));

  // you can transmit C-string or Arduino string up to
  // 255 characters long
  String str = "Hello World! #" + String(count++);
  int state = radio.transmit(str);

  // you can also transmit byte array up to 256 bytes long
  /*
    byte byteArr[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    int state = radio.transmit(byteArr, 8);
  */

  if (state == RADIOLIB_ERR_NONE) {
    // the packet was successfully transmitted
    Serial.println(F(" success!"));
    // print measured data rate
    Serial.print(F("[SX1278] Datarate:\t"));
    Serial.print(radio.getDataRate());
    Serial.println(F(" bps"));

  } else if (state == RADIOLIB_ERR_PACKET_TOO_LONG) {
    // the supplied packet was longer than 256 bytes
    Serial.println(F("too long!"));

  } else if (state == RADIOLIB_ERR_TX_TIMEOUT) {
    // timeout occurred while transmitting packet
    Serial.println(F("timeout!"));

  } else {
    // some other error occurred
    Serial.print(F("failed, code "));
    Serial.println(state);
  }


  // wait for a second before transmitting again
  delay(1000);
}





#elif defined(RECEIVE)
void loop() {
  // Serial.print(F("[SX1278] Waiting for incoming transmission ... "));

  // you can receive data as an Arduino String
  String str;
  int state = radio.receive(str);

  // you can also receive data as byte array
  /*
    byte byteArr[8];
    int state = radio.receive(byteArr, 8);
  */

  if (state == RADIOLIB_ERR_NONE) {
    // packet was successfully received
    Serial.println(F("success!"));

    // print the data of the packet
    Serial.print(F("[SX1278] Data:\t\t\t"));
    Serial.println(str);

    // print the RSSI (Received Signal Strength Indicator)
    // of the last received packet
    Serial.print(F("[SX1278] RSSI:\t\t\t"));
    Serial.print(radio.getRSSI());
    Serial.println(F(" dBm"));

    // print the SNR (Signal-to-Noise Ratio)
    // of the last received packet
    Serial.print(F("[SX1278] SNR:\t\t\t"));
    Serial.print(radio.getSNR());
    Serial.println(F(" dB"));

    // print frequency error
    // of the last received packet
    Serial.print(F("[SX1278] Frequency error:\t"));
    Serial.print(radio.getFrequencyError());
    Serial.println(F(" Hz"));

  } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
    // timeout occurred while waiting for a packet
    Serial.println(F("timeout!"));

  } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
    // packet was received, but is malformed
    Serial.println(F("CRC error!"));

  } else {
    // some other error occurred
    Serial.print(F("failed, code "));
    Serial.println(state);
  }
  delay(10);
}

#else
#error "Define TRANSMIT or RECEIVE"
#endif