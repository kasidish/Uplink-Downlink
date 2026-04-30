#include <RadioLib.h>
#include "Arducam_Mega.h"
#include <SPI.h>
#include "SdFat.h"

// --- Store-and-Forward System Main Orchestrator ---

// Hardware Pin Definitions
const int CAM_CS = PE_7;
const int SD_CS = PC9;

// Radio Setup (SX1278)
SPIClass RADIO_SPI(PA7, PA6, PA5, -1);
SX1278 radio = new Module(PB6, PA10, PC7, -1, RADIO_SPI);

// SD Card Setup (SdFat)
#define SPI_DRIVER_SELECT 2
#define ENABLE_DEDICATED_SPI 1
SPIClass SD_SPI(PC12, PC11, PC10);
#define SD_CONFIG SdSpiConfig(SD_CS, DEDICATED_SPI, SD_SCK_MHZ(8), &SD_SPI)
SdFs sd;

// Camera Setup (Arducam Mega)
Arducam_Mega myCAM(CAM_CS);

// Global State
uint16_t nextImageID = 1;

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

// Function Prototypes
void processCommand(String cmd);
void handleCapture();
void handleRequest(uint16_t id);
uint8_t calculateChecksum(uint8_t* data, int len);

void setup() {
  // Serial Debug Setup
  Serial.setTx(PA2);
  Serial.setRx(PA3);
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("\n--- Store-and-Forward System v2.0 ---"));

  // 1. Initialize Storage
  if (!storage_init()) {
    Serial.println(F("[ERROR] Storage init failed!"));
  }

  // 2. Initialize Camera
  // Arducam Mega uses the standard SPI bus
  SPI.setMISO(PB_4);
  SPI.setMOSI(PB_5);
  SPI.setSCLK(PB_3);
  SPI.begin();
  camera_init();

  // 3. Initialize Communication
  comm_init();

  Serial.println(F("System Ready. Listening..."));
}

void loop() {
  String incomingCmd = comm_listen();
  if (incomingCmd.length() > 0) {
    processCommand(incomingCmd);
  }
}

void processCommand(String cmd) {
  if (cmd == "PING") {
    Serial.println(F("[CMD] PING"));
    comm_transmit_str("PONG");
  } 
  else if (cmd == "CAPT") {
    Serial.println(F("[CMD] CAPTURE"));
    handleCapture();
  } 
  else if (cmd.startsWith("REQP:")) {
    uint16_t id = cmd.substring(5).toInt();
    Serial.print(F("[CMD] REQUEST: ")); Serial.println(id);
    handleRequest(id);
  }
  else {
    Serial.print(F("[CMD] UNKNOWN: ")); Serial.println(cmd);
    comm_transmit_str("ERR:CMD_UNKNOWN");
  }
}

void handleCapture() {
  uint16_t id = nextImageID;
  if (camera_capture_to_sd(id)) {
    nextImageID++;
    storage_save_id();
    comm_transmit_str("CAPT:SUCCESS ID=" + String(id));
  } else {
    comm_transmit_str("CAPT:FAILED");
  }
}

void handleRequest(uint16_t id) {
  if (!storage_exists(id)) {
    comm_transmit_str("ERR:NOT_FOUND");
    return;
  }

  FsFile imgFile = storage_open_image(id);
  if (!imgFile) {
    comm_transmit_str("ERR:FILE_OPEN");
    return;
  }

  uint32_t fileSize = imgFile.size();
  uint16_t totalPackets = (fileSize + MAX_DATA_SIZE - 1) / MAX_DATA_SIZE;
  
  Serial.print(F("[DOWNLINK] Starting ID ")); Serial.print(id);
  Serial.print(F(", Packets: ")); Serial.println(totalPackets);

  uint8_t dataBuffer[MAX_DATA_SIZE];
  uint8_t packetBuffer[128];
  
  for (uint16_t i = 1; i <= totalPackets; i++) {
    int bytesRead = imgFile.read(dataBuffer, MAX_DATA_SIZE);
    
    PacketHeader header;
    header.imageID = id;
    header.packetIdx = i;
    header.totalPackets = totalPackets;
    header.dataSize = (uint8_t)bytesRead;
    header.checksum = calculateChecksum(dataBuffer, bytesRead);
    
    memcpy(packetBuffer, &header, HEADER_SIZE);
    memcpy(packetBuffer + HEADER_SIZE, dataBuffer, bytesRead);
    
    int state = comm_transmit(packetBuffer, HEADER_SIZE + bytesRead);
    
    if (state != RADIOLIB_ERR_NONE) {
      delay(10);
      comm_transmit(packetBuffer, HEADER_SIZE + bytesRead);
    }
    
    delay(40); 
  }
  
  imgFile.close();
  comm_transmit_str("EOF:" + String(id));
  Serial.println(F("[DOWNLINK] Finished."));
}

uint8_t calculateChecksum(uint8_t* data, int len) {
  uint8_t crc = 0;
  for (int i = 0; i < len; i++) {
    crc ^= data[i];
  }
  return crc;
}
