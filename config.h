#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// These SdFat options must be visible before SdFat is included by modules.
#ifndef SPI_DRIVER_SELECT
#define SPI_DRIVER_SELECT 2
#endif
#ifndef ENABLE_DEDICATED_SPI
#define ENABLE_DEDICATED_SPI 1
#endif

// ==========================================
// STORE-AND-FORWARD SYSTEM CONFIGURATION
// ==========================================

// --- Serial Pins ---
const int SERIAL_TX = PA2;
const int SERIAL_RX = PA3;
const uint32_t SERIAL_BAUD = 115200;

// --- SD Card Pins (PC-series) ---
const int SD_CS   = PC9;
const int SD_MOSI = PC12;
const int SD_MISO = PC11;
const int SD_SCLK = PC10;

// --- Radio Pins (PA/PB/PC-series) ---
const int RADIO_NSS  = PB6;
const int RADIO_DIO0 = PA10;
const int RADIO_BUSY = PC7;
const int RADIO_MOSI = PA7;
const int RADIO_MISO = PA6;
const int RADIO_SCLK = PA5;

// --- Camera Pins (PB/PE-series) ---
const int CAM_CS   = PE_7;
const int CAM_MOSI = PB_5;
const int CAM_MISO = PB_4;
const int CAM_SCLK = PB_3;

// --- Mission Constants ---
const float MISSION_BITRATE_KBPS = 9.6;
const float MISSION_FREQ_MHZ = 435.0;
const float MISSION_FREQ_DEV_KHZ = 4.8;
const int8_t RADIO_OUTPUT_POWER_DBM = 12;

// --- Capture/Downlink Settings ---
const uint32_t CAMERA_CAPTURE_TIMEOUT_MS = 15000;
const uint8_t PACKET_DATA_SIZE = 120;
const uint8_t PACKET_HEADER_SIZE = 8;
const uint8_t PACKET_TOTAL_SIZE = PACKET_HEADER_SIZE + PACKET_DATA_SIZE;
const uint8_t DOWNLINK_RETRY_COUNT = 1;
const uint16_t DOWNLINK_PACKET_DELAY_MS = 40;

struct PacketHeader {
  uint16_t imageID;
  uint16_t packetIdx;
  uint16_t totalPackets;
  uint8_t dataSize;
  uint8_t checksum;
};

static_assert(sizeof(PacketHeader) == PACKET_HEADER_SIZE, "PacketHeader must stay 8 bytes");

#endif
