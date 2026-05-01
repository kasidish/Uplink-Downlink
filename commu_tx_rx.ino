#include <RadioLib.h>
#include "config.h"

// --- Communication Module ---

SPIClass RADIO_SPI(RADIO_MOSI, RADIO_MISO, RADIO_SCLK, -1);
SX1278 radio = new Module(RADIO_NSS, RADIO_DIO0, RADIO_BUSY, -1, RADIO_SPI);

static void ax25EncodeAddress(uint8_t* frame, uint8_t offset, const char* callsign, uint8_t ssid, bool lastAddress) {
  for (uint8_t i = 0; i < 6; i++) {
    char c = callsign[i];
    if (c == '\0') {
      c = ' ';
    }
    frame[offset + i] = ((uint8_t)c) << 1;
  }

  frame[offset + 6] = 0x60 | ((ssid & 0x0F) << 1);
  if (lastAddress) {
    frame[offset + 6] |= 0x01;
  }
}

static uint16_t ax25CalculateFcs(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;

  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (crc & 0x0001) {
        crc = (crc >> 1) ^ 0x8408;
      } else {
        crc >>= 1;
      }
    }
  }

  return ~crc;
}

static size_t ax25BuildUiFrame(const uint8_t* info, size_t infoLen, uint8_t* frame, size_t frameLen) {
  size_t neededLen = AX25_HEADER_LEN + infoLen + AX25_FCS_LEN;
  if (infoLen > AX25_MAX_INFO_SIZE || neededLen > frameLen) {
    return 0;
  }

  ax25EncodeAddress(frame, 0, AX25_DEST_CALLSIGN, AX25_DEST_SSID, false);
  ax25EncodeAddress(frame, 7, AX25_SRC_CALLSIGN, AX25_SRC_SSID, true);
  frame[14] = AX25_CONTROL_UI;
  frame[15] = AX25_PID_NO_LAYER3;
  memcpy(frame + AX25_HEADER_LEN, info, infoLen);

  uint16_t fcs = ax25CalculateFcs(frame, AX25_HEADER_LEN + infoLen);
  frame[AX25_HEADER_LEN + infoLen] = fcs & 0xFF;
  frame[AX25_HEADER_LEN + infoLen + 1] = fcs >> 8;
  return neededLen;
}

static bool ax25ExtractInfo(const uint8_t* frame, size_t frameLen, uint8_t* info, size_t& infoLen) {
  if (frameLen < AX25_HEADER_LEN + AX25_FCS_LEN) {
    return false;
  }
  if (frame[14] != AX25_CONTROL_UI || frame[15] != AX25_PID_NO_LAYER3) {
    return false;
  }

  uint16_t receivedFcs = frame[frameLen - 2] | ((uint16_t)frame[frameLen - 1] << 8);
  uint16_t calculatedFcs = ax25CalculateFcs(frame, frameLen - AX25_FCS_LEN);
  if (receivedFcs != calculatedFcs) {
    Serial.println(F("[COMM] AX.25 FCS mismatch"));
    return false;
  }

  infoLen = frameLen - AX25_HEADER_LEN - AX25_FCS_LEN;
  memcpy(info, frame + AX25_HEADER_LEN, infoLen);
  return true;
}

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
  uint8_t frame[AX25_MAX_FRAME_SIZE];
  int state = radio.receive(frame, sizeof(frame));

  if (state == RADIOLIB_ERR_NONE) {
    uint8_t info[AX25_MAX_INFO_SIZE];
    size_t infoLen = 0;
    String cmd;

    if (USE_AX25_FRAME && ax25ExtractInfo(frame, radio.getPacketLength(), info, infoLen)) {
      for (size_t i = 0; i < infoLen; i++) {
        cmd += (char)info[i];
      }
    } else {
      for (size_t i = 0; i < radio.getPacketLength(); i++) {
        cmd += (char)frame[i];
      }
    }

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
  if (!USE_AX25_FRAME) {
    int state = radio.transmit(data, len);
    radio.startReceive();
    return state;
  }

  uint8_t frame[AX25_MAX_FRAME_SIZE];
  size_t frameLen = ax25BuildUiFrame(data, len, frame, sizeof(frame));
  if (frameLen == 0) {
    radio.startReceive();
    return RADIOLIB_ERR_PACKET_TOO_LONG;
  }

  int state = radio.transmit(frame, frameLen);
  radio.startReceive();
  return state;
}

void comm_transmit_str(const String& msg) {
  if (!USE_AX25_FRAME) {
    int state = radio.transmit(msg);
    if (state != RADIOLIB_ERR_NONE) {
      Serial.print(F("[COMM] TX string failed, code "));
      Serial.println(state);
    }
    radio.startReceive();
    return;
  }

  uint8_t frame[AX25_MAX_FRAME_SIZE];
  size_t frameLen = ax25BuildUiFrame((const uint8_t*)msg.c_str(), msg.length(), frame, sizeof(frame));
  if (frameLen == 0) {
    Serial.println(F("[COMM] TX string too long for AX.25 frame"));
    radio.startReceive();
    return;
  }

  int state = radio.transmit(frame, frameLen);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[COMM] TX string failed, code "));
    Serial.println(state);
  }
  radio.startReceive();
}
