#include <RadioLib.h>
#include "SdFat.h"
#include "config.h"

// --- Store-and-Forward System: Main Orchestrator ---

uint16_t nextImageID = 1;
bool storageReady = false;
bool cameraReady = false;
bool commReady = false;

// Storage module
bool storage_init();
void storage_load_id();
void storage_save_id();
bool storage_exists(uint16_t id);
FsFile storage_open_image(uint16_t id);
bool storage_delete_image(uint16_t id);

// Camera module
bool camera_init();
bool camera_capture_to_sd(uint16_t id);

// Communication module
bool comm_init();
String comm_listen();
int comm_transmit(uint8_t* data, size_t len);
void comm_transmit_str(const String& msg);

void processCommand(String cmd);
bool parseTwoIds(const String& args, uint16_t& first, uint16_t& second);
void handleCapture();
void handleRequestImage(uint16_t imageID);
void handleRequestChunk(uint16_t imageID, uint16_t chunkID);
void handleDeleteImage(uint16_t imageID);
void handleListImages();
void handleStatus();
bool transmitImageChunk(FsFile& imgFile, uint16_t imageID, uint16_t chunkID, uint16_t totalChunks);
uint16_t calculateChecksum(const uint8_t* data, size_t len);

void setup() {
  Serial.setTx(SERIAL_TX);
  Serial.setRx(SERIAL_RX);
  Serial.begin(SERIAL_BAUD);
  delay(1000);

  Serial.println(F("\n--- Store-and-Forward Image System v1.0 ---"));

  storageReady = storage_init();
  cameraReady = camera_init();
  commReady = comm_init();

  if (!storageReady) {
    Serial.println(F("[FATAL] SD initialization failed. CAPT/REQP/REQM/DELI disabled."));
  }
  if (!cameraReady) {
    Serial.println(F("[FATAL] Camera initialization failed. CAPT disabled."));
  }
  if (!commReady) {
    Serial.println(F("[FATAL] Radio initialization failed. Uplink/downlink disabled."));
  }

  Serial.println(F("[SYS] Ready. Commands: PING, STAT, LIST, CAPT, REQP:<IMAGE_ID>, REQM:<IMAGE_ID>:<CHUNK_ID>, DELI:<IMAGE_ID>"));
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

  if (cmd == "STAT") {
    Serial.println(F("[CMD] STAT"));
    handleStatus();
    return;
  }

  if (cmd == "LIST") {
    Serial.println(F("[CMD] LIST"));
    handleListImages();
    return;
  }

  if (cmd == "CAPT") {
    Serial.println(F("[CMD] CAPT"));
    handleCapture();
    return;
  }

  if (cmd.startsWith("REQP:")) {
    uint16_t imageID = (uint16_t)cmd.substring(5).toInt();
    Serial.print(F("[CMD] REQP image="));
    Serial.println(imageID);
    handleRequestImage(imageID);
    return;
  }

  if (cmd.startsWith("REQM:")) {
    uint16_t imageID = 0;
    uint16_t chunkID = 0;
    if (!parseTwoIds(cmd.substring(5), imageID, chunkID)) {
      comm_transmit_str("ERR:BAD_ARGS");
      return;
    }

    Serial.print(F("[CMD] REQM image="));
    Serial.print(imageID);
    Serial.print(F(" chunk="));
    Serial.println(chunkID);
    handleRequestChunk(imageID, chunkID);
    return;
  }

  if (cmd.startsWith("DELI:")) {
    uint16_t imageID = (uint16_t)cmd.substring(5).toInt();
    Serial.print(F("[CMD] DELI image="));
    Serial.println(imageID);
    handleDeleteImage(imageID);
    return;
  }

  Serial.print(F("[CMD] UNKNOWN: "));
  Serial.println(cmd);
  comm_transmit_str("ERR:CMD_UNKNOWN");
}

bool parseTwoIds(const String& args, uint16_t& first, uint16_t& second) {
  int separator = args.indexOf(':');
  if (separator <= 0 || separator >= (int)args.length() - 1) {
    return false;
  }

  first = (uint16_t)args.substring(0, separator).toInt();
  second = (uint16_t)args.substring(separator + 1).toInt();
  return first > 0 && second > 0;
}

void handleCapture() {
  if (!storageReady || !cameraReady) {
    comm_transmit_str("ERR:CAPT_UNAVAILABLE");
    return;
  }

  uint16_t id = nextImageID;

  if (camera_capture_to_sd(id)) {
    nextImageID++;
    storage_save_id();
    comm_transmit_str("CAPT_OK:" + String(id));
    return;
  }

  comm_transmit_str("ERR:CAPT_FAILED");
}

void handleRequestImage(uint16_t imageID) {
  if (!storageReady) {
    comm_transmit_str("ERR:SD_UNAVAILABLE");
    return;
  }

  if (imageID == 0) {
    comm_transmit_str("ERR:BAD_ID");
    return;
  }

  if (!storage_exists(imageID)) {
    comm_transmit_str("ERR:NOT_FOUND:" + String(imageID));
    return;
  }

  FsFile imgFile = storage_open_image(imageID);
  if (!imgFile) {
    comm_transmit_str("ERR:FILE_OPEN:" + String(imageID));
    return;
  }

  uint32_t fileSize = imgFile.size();
  uint16_t totalChunks = (uint16_t)((fileSize + PACKET_DATA_SIZE - 1) / PACKET_DATA_SIZE);

  Serial.print(F("[DOWNLINK] ID="));
  Serial.print(imageID);
  Serial.print(F(" bytes="));
  Serial.print(fileSize);
  Serial.print(F(" chunks="));
  Serial.println(totalChunks);

  for (uint16_t chunkID = 1; chunkID <= totalChunks; chunkID++) {
    if (!transmitImageChunk(imgFile, imageID, chunkID, totalChunks)) {
      break;
    }
  }

  imgFile.close();
  comm_transmit_str("EOF:" + String(imageID));
  Serial.println(F("[DOWNLINK] Finished"));
}

void handleRequestChunk(uint16_t imageID, uint16_t chunkID) {
  if (!storageReady) {
    comm_transmit_str("ERR:SD_UNAVAILABLE");
    return;
  }

  if (!storage_exists(imageID)) {
    comm_transmit_str("ERR:NOT_FOUND:" + String(imageID));
    return;
  }

  FsFile imgFile = storage_open_image(imageID);
  if (!imgFile) {
    comm_transmit_str("ERR:FILE_OPEN:" + String(imageID));
    return;
  }

  uint32_t fileSize = imgFile.size();
  uint16_t totalChunks = (uint16_t)((fileSize + PACKET_DATA_SIZE - 1) / PACKET_DATA_SIZE);

  if (chunkID == 0 || chunkID > totalChunks) {
    imgFile.close();
    comm_transmit_str("ERR:BAD_CHUNK:" + String(imageID) + ":" + String(chunkID));
    return;
  }

  transmitImageChunk(imgFile, imageID, chunkID, totalChunks);
  imgFile.close();
}

void handleDeleteImage(uint16_t imageID) {
  if (!storageReady) {
    comm_transmit_str("ERR:SD_UNAVAILABLE");
    return;
  }

  if (imageID == 0) {
    comm_transmit_str("ERR:BAD_ID");
    return;
  }

  if (storage_delete_image(imageID)) {
    comm_transmit_str("DELI_OK:" + String(imageID));
    return;
  }

  comm_transmit_str("ERR:DELETE_FAILED:" + String(imageID));
}

void handleListImages() {
  if (!storageReady) {
    comm_transmit_str("ERR:SD_UNAVAILABLE");
    return;
  }

  String line = "LIST:";
  bool found = false;

  for (uint16_t imageID = 1; imageID < nextImageID; imageID++) {
    if (!storage_exists(imageID)) {
      continue;
    }

    String item = String(imageID) + ",";
    if (line.length() + item.length() > 180) {
      comm_transmit_str(line);
      line = "LIST:";
    }

    line += item;
    found = true;
  }

  if (!found) {
    comm_transmit_str("LIST:EMPTY");
    return;
  }

  if (line.endsWith(",")) {
    line.remove(line.length() - 1);
  }
  comm_transmit_str(line);
  comm_transmit_str("LIST:END");
}

void handleStatus() {
  String status = "STAT:";
  status += "SD=";
  status += (storageReady ? "1" : "0");
  status += ",CAM=";
  status += (cameraReady ? "1" : "0");
  status += ",RADIO=";
  status += (commReady ? "1" : "0");
  status += ",NEXT_IMAGE_ID=";
  status += String(nextImageID);
  status += ",PKT_TOTAL=";
  status += String(PACKET_TOTAL_SIZE);
  status += ",PKT_DATA=";
  status += String(PACKET_DATA_SIZE);
  comm_transmit_str(status);
}

bool transmitImageChunk(FsFile& imgFile, uint16_t imageID, uint16_t chunkID, uint16_t totalChunks) {
  uint32_t offset = (uint32_t)(chunkID - 1) * PACKET_DATA_SIZE;

  if (!imgFile.seekSet(offset)) {
    comm_transmit_str("ERR:SEEK_FAILED:" + String(imageID) + ":" + String(chunkID));
    return false;
  }

  uint8_t dataBuffer[PACKET_DATA_SIZE];
  uint8_t packetBuffer[PACKET_TOTAL_SIZE];
  int bytesRead = imgFile.read(dataBuffer, PACKET_DATA_SIZE);
  if (bytesRead <= 0) {
    Serial.println(F("[DOWNLINK] Read failed before EOF"));
    comm_transmit_str("ERR:READ_FAILED:" + String(imageID) + ":" + String(chunkID));
    return false;
  }

  PacketHeader header;
  header.imageID = imageID;
  header.chunkID = chunkID;
  header.totalChunks = totalChunks;
  header.checksum = calculateChecksum(dataBuffer, (uint8_t)bytesRead);
  header.dataSize = (uint8_t)bytesRead;

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
    Serial.print(F("[DOWNLINK] TX failed, chunk="));
    Serial.print(chunkID);
    Serial.print(F(" code="));
    Serial.println(state);
    return false;
  }

  delay(DOWNLINK_PACKET_DELAY_MS);
  return true;
}

uint16_t calculateChecksum(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;

  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (crc & 0x8000) {
        crc = (crc << 1) ^ 0x1021;
      } else {
        crc <<= 1;
      }
    }
  }

  return crc;
}
