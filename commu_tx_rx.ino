#include <RadioLib.h>
#include "config.h"

// --- Communication Module (HAL) ---

// Module owns its hardware resources
SPIClass RADIO_SPI(RADIO_MOSI, RADIO_MISO, RADIO_SCLK, -1);
SX1278 radio = new Module(RADIO_NSS, RADIO_DIO0, RADIO_BUSY, -1, RADIO_SPI);

bool comm_init() {
  RADIO_SPI.begin();
  Serial.print(F("[HAL] Initializing Radio ... "));
  
  int state = radio.beginFSK(MISSION_FREQ);
  radio.setDataShaping(RADIOLIB_SHAPING_0_5);
  state += radio.setOutputPower(12);
  
  FSKRate_t fskRate = { .bitRate = MISSION_BITRATE, .freqDev = 4.8 };
  DataRate_t dataRate = { .fsk = fskRate };
  radio.setDataRate(dataRate);
  
  uint8_t syncWord[] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };
  state += radio.setSyncWord(syncWord, 8);

  if (state == RADIOLIB_ERR_NONE) {
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
    radio.startReceive();
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
