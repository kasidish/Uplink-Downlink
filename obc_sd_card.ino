#include <SPI.h>
#include "SdFat.h"
#include "config.h"

// --- Storage Module (HAL) ---

// Module owns its hardware resources
SPIClass SD_SPI(SD_MOSI, SD_MISO, SD_SCLK);
#define SD_HAL_CONFIG SdSpiConfig(SD_CS, DEDICATED_SPI, SD_SCK_MHZ(8), &SD_SPI)
SdFs sd;

bool storage_init() {
  Serial.print(F("[HAL] Initializing SD Card ... "));
  if (!sd.begin(SD_HAL_CONFIG)) {
    Serial.println(F("Failed!"));
    return false;
  }
  Serial.println(F("Success."));
  storage_load_id();
  return true;
}

void storage_load_id() {
  if (sd.exists("config.txt")) {
    FsFile configFile = sd.open("config.txt", O_READ);
    if (configFile) {
      extern uint16_t nextImageID; // Shared global
      nextImageID = configFile.parseInt();
      configFile.close();
      Serial.print(F("[SD] Loaded nextImageID: ")); Serial.println(nextImageID);
    }
  }
}

void storage_save_id() {
  FsFile configFile = sd.open("config.txt", O_WRITE | O_CREAT | O_TRUNC);
  if (configFile) {
    extern uint16_t nextImageID;
    configFile.print(nextImageID);
    configFile.close();
  }
}

bool storage_exists(uint16_t id) {
  char filename[15];
  sprintf(filename, "IMG_%d.JPG", id);
  return sd.exists(filename);
}

FsFile storage_open_image(uint16_t id) {
  char filename[15];
  sprintf(filename, "IMG_%d.JPG", id);
  return sd.open(filename, O_READ);
}
