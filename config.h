#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ==========================================
// STORE-AND-FORWARD SYSTEM CONFIGURATION
// ==========================================

// --- SD Card Pins (PC-series) ---
const int SD_CS   = PC9;
const int SD_MOSI = PC12;
const int SD_MISO = PC11;
const int SD_SCLK = PC10;

// --- Radio Pins (PA/PB/PC-series) ---
const int RADIO_NSS  = PB6;
const int RADIO_DIO0 = PA10;
const int RADIO_BUSY = PC7; // Using PC7 as busy/reset if needed
const int RADIO_MOSI = PA7;
const int RADIO_MISO = PA6;
const int RADIO_SCLK = PA5;

// --- Camera Pins (PB/PE-series) ---
const int CAM_CS   = PE_7;
const int CAM_MOSI = PB_5;
const int CAM_MISO = PB_4;
const int CAM_SCLK = PB_3;

// --- Mission Constants ---
const uint16_t MISSION_BITRATE = 9.6; // kbps
const float    MISSION_FREQ    = 435.0; // MHz

// --- Packet Protocol (Total 128 bytes) ---
struct PacketHeader {
  uint16_t imageID;      // 2 bytes
  uint16_t packetIdx;    // 2 bytes
  uint16_t totalPackets;  // 2 bytes
  uint8_t dataSize;      // 1 byte
  uint8_t checksum;      // 1 byte
};

const int MAX_DATA_SIZE = 120;
const int HEADER_SIZE = sizeof(PacketHeader);

#endif
