#include <RadioLib.h>

// Note: RADIO_SPI and radio are defined as globals in mission_integrated.ino
// and are accessible to this tab in the Arduino IDE.

bool comm_init() {
  RADIO_SPI.begin();
  Serial.print(F("[SX1278] Initializing Radio ... "));
  int state = radio.beginFSK(435.0);
  if (state == RADIOLIB_ERR_NONE) {
    radio.setDataShaping(RADIOLIB_SHAPING_0_5);
    radio.setOutputPower(12);
    FSKRate_t fskRate = { .bitRate = 9.6, .freqDev = 4.8 };
    DataRate_t dataRate = { .fsk = fskRate };
    radio.setDataRate(dataRate);
    uint8_t syncWord[] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };
    radio.setSyncWord(syncWord, 8);
    Serial.println(F("Success!"));
    radio.startReceive();
    return true;
  } else {
    Serial.print(F("Failed, code ")); Serial.println(state);
    return false;
  }
}

String comm_listen() {
  if (radio.available()) {
    String cmd;
    int state = radio.readString(cmd);
    radio.startReceive(); // Resume listening
    if (state == RADIOLIB_ERR_NONE) {
      cmd.trim();
      return cmd;
    }
  }
  return "";
}

int comm_transmit(uint8_t* data, size_t len) {
  return radio.transmit(data, len);
}

void comm_transmit_str(String msg) {
  radio.transmit(msg);
}
