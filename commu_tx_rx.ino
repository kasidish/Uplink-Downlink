#include <RadioLib.h>
#include "config.h"

// --- Communication Module ---

SPIClass RADIO_SPI(RADIO_MOSI, RADIO_MISO, RADIO_SCLK, -1);
SX1278 radio = new Module(RADIO_NSS, RADIO_DIO0, RADIO_BUSY, -1, RADIO_SPI);

bool comm_init() {
  RADIO_SPI.begin();
  Serial.print(F("[COMM] Initializing SX1278 FSK ... "));

  int state = radio.beginFSK(MISSION_FREQ_MHZ);
  if (state == RADIOLIB_ERR_NONE) {
    state = radio.setDataShaping(RADIOLIB_SHAPING_0_5);
  }
  if (state == RADIOLIB_ERR_NONE) {
    state = radio.setOutputPower(RADIO_OUTPUT_POWER_DBM);
  }

  FSKRate_t fskRate = {
    .bitRate = MISSION_BITRATE_KBPS,
    .freqDev = MISSION_FREQ_DEV_KHZ,
  };
  DataRate_t dataRate = {
    .fsk = fskRate,
  };

  if (state == RADIOLIB_ERR_NONE) {
    state = radio.setDataRate(dataRate);
  }

  uint8_t syncWord[] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };
  if (state == RADIOLIB_ERR_NONE) {
    state = radio.setSyncWord(syncWord, sizeof(syncWord));
  }

  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("failed, code "));
    Serial.println(state);
    return false;
  }

  radio.startReceive();
  Serial.println(F("ready"));
  return true;
}

String comm_listen() {
  String cmd;
  int state = radio.receive(cmd);

  if (state == RADIOLIB_ERR_NONE) {
    cmd.trim();
    radio.startReceive();
    return cmd;
  }

  if (state != RADIOLIB_ERR_RX_TIMEOUT) {
    Serial.print(F("[COMM] RX error "));
    Serial.println(state);
  }

  radio.startReceive();
  return "";
}

int comm_transmit(uint8_t* data, size_t len) {
  int state = radio.transmit(data, len);
  radio.startReceive();
  return state;
}

void comm_transmit_str(const String& msg) {
  int state = radio.transmit(msg);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[COMM] TX string failed, code "));
    Serial.println(state);
  }
  radio.startReceive();
}
