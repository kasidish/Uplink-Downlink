#include <SPI.h>
#include "SdFat.h"
#include "config.h"

// --- Storage Module ---

SPIClass SD_SPI(SD_MOSI, SD_MISO, SD_SCLK);
#define SD_HAL_CONFIG SdSpiConfig(SD_CS, DEDICATED_SPI, SD_SCK_MHZ(8), &SD_SPI)

SdFs sd;

static void formatImageFilename(uint16_t id, char* filename, size_t len) {
  snprintf(filename, len, "IMG_%04u.JPG", id);
}

bool storage_init() {
  Serial.print(F("[SD] Initializing card ... "));

  if (!sd.begin(SD_HAL_CONFIG)) {
    Serial.println(F("failed"));
    return false;
  }

  Serial.println(F("ready"));
  storage_load_id();
  return true;
}

void storage_load_id() {
  extern uint16_t nextImageID;

  if (!sd.exists("config.txt")) {
    nextImageID = 1;
    storage_save_id();
    return;
  }

  FsFile configFile = sd.open("config.txt", O_READ);
  if (!configFile) {
    nextImageID = 1;
    return;
  }

  uint16_t loadedId = (uint16_t)configFile.parseInt();
  configFile.close();

  if (loadedId == 0) {
    loadedId = 1;
  }

  nextImageID = loadedId;
  Serial.print(F("[SD] nextImageID="));
  Serial.println(nextImageID);
}

void storage_save_id() {
  extern uint16_t nextImageID;

  FsFile configFile = sd.open("config.txt", O_WRITE | O_CREAT | O_TRUNC);
  if (!configFile) {
    Serial.println(F("[SD] Could not save config.txt"));
    return;
  }

  configFile.print(F("nextImageID="));
  configFile.println(nextImageID);
  configFile.close();
}

bool storage_exists(uint16_t id) {
  char filename[16];
  formatImageFilename(id, filename, sizeof(filename));
  return sd.exists(filename);
}

FsFile storage_open_image(uint16_t id) {
  char filename[16];
  formatImageFilename(id, filename, sizeof(filename));
  return sd.open(filename, O_READ);
}
