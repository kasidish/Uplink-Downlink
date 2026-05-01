#include <RadioLib.h>
#include "SdFat.h"
#include "config.h"

// --- Store-and-Forward System: Main Orchestrator ---

uint16_t nextImageID = 1;

// Storage module
bool storage_init();
void storage_load_id();
void storage_save_id();
bool storage_exists(uint16_t id);
FsFile storage_open_image(uint16_t id);

// Camera module
bool camera_init();
bool camera_capture_to_sd(uint16_t id);

// Communication module
bool comm_init();
String comm_listen();
int comm_transmit(uint8_t* data, size_t len);
void comm_transmit_str(const String& msg);

void processCommand(String cmd);
void handleCapture();
void handleRequest(uint16_t id);
uint8_t calculateChecksum(const uint8_t* data, uint8_t len);

void setup() {
  Serial.setTx(SERIAL_TX);
  Serial.setRx(SERIAL_RX);
  Serial.begin(SERIAL_BAUD);
  delay(1000);

  Serial.println(F("\n--- Store-and-Forward Image System v1.0 ---"));

  bool storageReady = storage_init();
  bool cameraReady = camera_init();
  bool commReady = comm_init();

  if (!storageReady) {
    Serial.println(F("[FATAL] SD initialization failed. CAPT/REQP disabled."));
  }
  if (!cameraReady) {
    Serial.println(F("[FATAL] Camera initialization failed. CAPT disabled."));
  }
  if (!commReady) {
    Serial.println(F("[FATAL] Radio initialization failed. Uplink/downlink disabled."));
  }

  Serial.println(F("[SYS] Ready. Commands: PING, CAPT, REQP:<ID>"));
}

void loop() {
  String incomingCmd = comm_listen();
  if (incomingCmd.length() > 0) {
    processCommand(incomingCmd);
  }
}

void processCommand(String cmd) {
  cmd.trim();
  cmd.toUpperCase();

  if (cmd == "PING") {
    Serial.println(F("[CMD] PING"));
    comm_transmit_str("PONG");
    return;
  }

  if (cmd == "CAPT") {
    Serial.println(F("[CMD] CAPT"));
    handleCapture();
    return;
  }

  if (cmd.startsWith("REQP:")) {
    uint16_t id = (uint16_t)cmd.substring(5).toInt();
    Serial.print(F("[CMD] REQP "));
    Serial.println(id);
    handleRequest(id);
    return;
  }

  Serial.print(F("[CMD] UNKNOWN: "));
  Serial.println(cmd);
  comm_transmit_str("ERR:CMD_UNKNOWN");
}

void handleCapture() {
  uint16_t id = nextImageID;

  if (camera_capture_to_sd(id)) {
    nextImageID++;
    storage_save_id();
    comm_transmit_str("CAPT_OK:" + String(id));
    return;
  }

  comm_transmit_str("ERR:CAPT_FAILED");
}

void handleRequest(uint16_t id) {
  if (id == 0) {
    comm_transmit_str("ERR:BAD_ID");
    return;
  }

  if (!storage_exists(id)) {
    comm_transmit_str("ERR:NOT_FOUND:" + String(id));
    return;
  }

  FsFile imgFile = storage_open_image(id);
  if (!imgFile) {
    comm_transmit_str("ERR:FILE_OPEN:" + String(id));
    return;
  }

  uint32_t fileSize = imgFile.size();
  uint16_t totalPackets = (uint16_t)((fileSize + PACKET_DATA_SIZE - 1) / PACKET_DATA_SIZE);

  Serial.print(F("[DOWNLINK] ID="));
  Serial.print(id);
  Serial.print(F(" bytes="));
  Serial.print(fileSize);
  Serial.print(F(" packets="));
  Serial.println(totalPackets);

  uint8_t dataBuffer[PACKET_DATA_SIZE];
  uint8_t packetBuffer[PACKET_TOTAL_SIZE];

  for (uint16_t packetIdx = 1; packetIdx <= totalPackets; packetIdx++) {
    int bytesRead = imgFile.read(dataBuffer, PACKET_DATA_SIZE);
    if (bytesRead <= 0) {
      Serial.println(F("[DOWNLINK] Read failed before EOF"));
      comm_transmit_str("ERR:READ_FAILED:" + String(id));
      break;
    }

    PacketHeader header;
    header.imageID = id;
    header.packetIdx = packetIdx;
    header.totalPackets = totalPackets;
    header.dataSize = (uint8_t)bytesRead;
    header.checksum = calculateChecksum(dataBuffer, (uint8_t)bytesRead);

    memset(packetBuffer, 0, sizeof(packetBuffer));
    memcpy(packetBuffer, &header, PACKET_HEADER_SIZE);
    memcpy(packetBuffer + PACKET_HEADER_SIZE, dataBuffer, bytesRead);

    int state = -999;
    for (uint8_t attempt = 0; attempt <= DOWNLINK_RETRY_COUNT; attempt++) {
      state = comm_transmit(packetBuffer, PACKET_TOTAL_SIZE);
      if (state == RADIOLIB_ERR_NONE) {
        break;
      }
      delay(10);
    }

    if (state != RADIOLIB_ERR_NONE) {
      Serial.print(F("[DOWNLINK] TX failed, packet="));
      Serial.print(packetIdx);
      Serial.print(F(" code="));
      Serial.println(state);
    }

    delay(DOWNLINK_PACKET_DELAY_MS);
  }

  imgFile.close();
  comm_transmit_str("EOF:" + String(id));
  Serial.println(F("[DOWNLINK] Finished"));
}

uint8_t calculateChecksum(const uint8_t* data, uint8_t len) {
  uint8_t checksum = 0;
  for (uint8_t i = 0; i < len; i++) {
    checksum ^= data[i];
  }
  return checksum;
}
