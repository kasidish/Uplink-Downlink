#include <RadioLib.h>
#include "Arducam_Mega.h"
#include <SPI.h>
#include "SdFat.h"

// --- Radio Configuration (SX1278) ---
SPIClass RADIO_SPI(PA7, PA6, PA5, -1);
SX1278 radio = new Module(PB6, PA10, PC7, -1, RADIO_SPI);

// --- SD Card Configuration ---
#define SPI_DRIVER_SELECT 2
#define ENABLE_DEDICATED_SPI 1
SPIClass SD_SPI(PC12, PC11, PC10);
const int SD_CS = PC9;
#define SD_CONFIG SdSpiConfig(SD_CS, DEDICATED_SPI, SD_SCK_MHZ(8), &SD_SPI)
SdFs sd;
FsFile file;

// --- Camera Configuration ---
const int CAM_CS = PE_7;
Arducam_Mega myCAM(CAM_CS);

// --- Packet Protocol Definition ---
struct PacketHeader {
  uint16_t imageID;      // 2 bytes
  uint16_t packetIdx;    // 2 bytes
  uint16_t totalPackets;  // 2 bytes
  uint8_t dataSize;      // 1 byte
  uint8_t checksum;      // 1 byte (Simple XOR checksum for demonstration)
};

const int MAX_DATA_SIZE = 240; // 240 bytes of image data per packet
const int HEADER_SIZE = sizeof(PacketHeader);

// --- Global Variables ---
uint16_t nextImageID = 1;
const char* CONFIG_FILE = "config.txt";

// --- Function Prototypes ---
void loadConfig();
void saveConfig();
void takeAndStorePicture();
void transmitImage(uint16_t id);
uint8_t calculateChecksum(uint8_t* data, int len);

void setup() {
  // Serial Debug Setup
  Serial.setTx(PA2);
  Serial.setRx(PA3);
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("\n--- CubeSat Mission Control System ---"));

  // 1. Initialize Radio
  RADIO_SPI.begin();
  Serial.print(F("[SX1278] Initializing Radio ... "));
  int state = radio.beginFSK(435.0);
  if (state == RADIOLIB_ERR_NONE) {
    radio.setDataShaping(RADIOLIB_SHAPING_0_5);
    radio.setOutputPower(12);
    FSKRate_t fskRate = { .bitRate = 9.6, .freqDev = 4.8 };
    DataRate_t dataRate = { .fsk = fskRate };
    radio.setDataRate(dataRate);
    uint8_t syncWord[] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };
    radio.setSyncWord(syncWord, 8);
    Serial.println(F("Success!"));
  } else {
    Serial.print(F("Failed, code ")); Serial.println(state);
  }

  // 2. Initialize SD Card
  Serial.print(F("[SD] Initializing SD Card ... "));
  if (!sd.begin(SD_CONFIG)) {
    Serial.println(F("Failed!"));
  } else {
    Serial.println(F("Success."));
    loadConfig(); // Load the next image ID
  }

  // 3. Initialize Camera
  SPI.setMISO(PB_4);
  SPI.setMOSI(PB_5);
  SPI.setSCLK(PB_3);
  SPI.begin();
  Serial.print(F("[CAM] Initializing Arducam ... "));
  myCAM.begin();
  Serial.println(F("Success."));

  // Start listening for commands
  radio.startReceive();
}

void loop() {
  // Check for Uplink Commands
  if (radio.available()) {
    String cmd;
    int state = radio.readString(cmd);

    if (state == RADIOLIB_ERR_NONE) {
      Serial.print(F("[UPLINK] Received: ")); Serial.println(cmd);

      if (cmd == "CAPTURE") {
        takeAndStorePicture();
      } 
      else if (cmd.startsWith("REQ:")) {
        int id = cmd.substring(4).toInt();
        transmitImage(id);
      }
    }
    // Resume listening after processing
    radio.startReceive();
  }
}

// --- Mission Logic Functions ---

void loadConfig() {
  if (sd.exists(CONFIG_FILE)) {
    file = sd.open(CONFIG_FILE, O_READ);
    if (file) {
      nextImageID = file.parseInt();
      file.close();
      Serial.print(F("Loaded nextImageID: ")); Serial.println(nextImageID);
    }
  }
}

void saveConfig() {
  file = sd.open(CONFIG_FILE, O_WRITE | O_CREAT | O_TRUNC);
  if (file) {
    file.print(nextImageID);
    file.close();
  }
}

void takeAndStorePicture() {
  Serial.println(F("[MISSION] Capturing Image..."));
  
  // 1. Capture to Camera Buffer
  myCAM.takePicture(CAM_IMAGE_MODE_VGA, CAM_IMAGE_PIX_FMT_JPG);

  // 2. Create Filename
  char filename[15];
  sprintf(filename, "IMG_%d.JPG", nextImageID);
  
  // 3. Save to SD
  file = sd.open(filename, O_WRITE | O_CREAT);
  if (file) {
    uint8_t buffer[255];
    while (myCAM.getReceivedLength() > 0) {
      int len = myCAM.readBuff(buffer, 255);
      file.write(buffer, len);
    }
    file.close();
    Serial.print(F("[SD] Saved as: ")); Serial.println(filename);
    
    // 4. Update ID
    nextImageID++;
    saveConfig();
  } else {
    Serial.println(F("[ERROR] SD Write Failed!"));
  }
}

void transmitImage(uint16_t id) {
  char filename[15];
  sprintf(filename, "IMG_%d.JPG", id);
  
  Serial.print(F("[DOWNLINK] Opening: ")); Serial.println(filename);
  
  if (!sd.exists(filename)) {
    radio.transmit("ERR:NOT_FOUND");
    return;
  }

  file = sd.open(filename, O_READ);
  uint32_t fileSize = file.size();
  uint16_t totalPackets = (fileSize + MAX_DATA_SIZE - 1) / MAX_DATA_SIZE;
  
  Serial.print(F("Size: ")); Serial.print(fileSize);
  Serial.print(F(" bytes, Packets: ")); Serial.println(totalPackets);

  uint8_t dataBuffer[MAX_DATA_SIZE];
  uint8_t packetBuffer[HEADER_SIZE + MAX_DATA_SIZE];
  
  for (uint16_t i = 1; i <= totalPackets; i++) {
    int bytesRead = file.read(dataBuffer, MAX_DATA_SIZE);
    
    // Prepare Header
    PacketHeader header;
    header.imageID = id;
    header.packetIdx = i;
    header.totalPackets = totalPackets;
    header.dataSize = (uint8_t)bytesRead;
    header.checksum = calculateChecksum(dataBuffer, bytesRead);
    
    // Assemble Packet
    memcpy(packetBuffer, &header, HEADER_SIZE);
    memcpy(packetBuffer + HEADER_SIZE, dataBuffer, bytesRead);
    
    // Transmit
    Serial.print(F("Sending packet ")); Serial.print(i);
    Serial.print(F("/")); Serial.println(totalPackets);
    
    int state = radio.transmit(packetBuffer, HEADER_SIZE + bytesRead);
    
    if (state != RADIOLIB_ERR_NONE) {
      Serial.print(F("Transmit error: ")); Serial.println(state);
    }
    
    delay(50); // Small delay between packets
  }
  
  file.close();
  radio.transmit("EOF");
  Serial.println(F("[DOWNLINK] Completed."));
}

uint8_t calculateChecksum(uint8_t* data, int len) {
  uint8_t crc = 0;
  for (int i = 0; i < len; i++) {
    crc ^= data[i];
  }
  return crc;
}
