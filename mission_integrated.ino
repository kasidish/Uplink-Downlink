#include "config.h"

// --- Store-and-Forward System: Main Orchestrator (HAL-based) ---

// Shared Mission State
uint16_t nextImageID = 1;

// Function Prototypes for HAL Modules
bool storage_init();
void storage_save_id();
bool storage_exists(uint16_t id);
FsFile storage_open_image(uint16_t id);

bool camera_init();
bool camera_capture_to_sd(uint16_t id);

bool comm_init();
String comm_listen();
int comm_transmit(uint8_t* data, size_t len);
void comm_transmit_str(String msg);

// Internal Orchestration Functions
void processCommand(String cmd);
void handleCapture();
void handleRequest(uint16_t id);
uint8_t calculateChecksum(uint8_t* data, int len);

void setup() {
  Serial.setTx(PA2);
  Serial.setRx(PA3);
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("\n--- Store-and-Forward System v2.0 (HAL) ---"));

  // HAL Initialization
  if (!storage_init()) Serial.println(F("[FATAL] SD Failed"));
  if (!camera_init())  Serial.println(F("[FATAL] Camera Failed"));
  if (!comm_init())    Serial.println(F("[FATAL] Radio Failed"));

  Serial.println(F("System Ready. Listening for Uplink..."));
}

void loop() {
  // Listen for commands through the Communication HAL
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
