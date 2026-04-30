#include <RadioLib.h>

// --- Communication Module (Store-and-Forward System) ---

// Globals shared across tabs
// RADIO_SPI and radio are defined in mission_integrated.ino

bool comm_init() {
  RADIO_SPI.begin();
  Serial.print(F("[SX1278] Initializing Radio ... "));
  
  // Baseline initialization settings
  int state = radio.beginFSK(435.0);
  radio.setDataShaping(RADIOLIB_SHAPING_0_5);
  state += radio.setOutputPower(12);
  
  FSKRate_t fskRate = {
    .bitRate = 9.6,
    .freqDev = 4.8,
  };
  DataRate_t dataRate = {
    .fsk = fskRate
  };
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
    radio.startReceive(); // Re-enable receive mode
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
