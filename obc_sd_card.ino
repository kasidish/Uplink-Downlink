#include <SPI.h>
#include "SdFat.h"

// --- Storage Module (Store-and-Forward System) ---

// Globals shared across tabs
// SD_SPI, SD_CS, SD_CONFIG, sd, and nextImageID are in mission_integrated.ino

bool storage_init() {
  Serial.print(F("[SD] Initializing SD card ... "));
  
  if (!sd.begin(SD_CONFIG)) {
    Serial.println(F("Failed! Check card and wiring."));
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
      nextImageID = configFile.parseInt();
      configFile.close();
      Serial.print(F("[SD] Loaded nextImageID: ")); Serial.println(nextImageID);
    }
  }
}

void storage_save_id() {
  FsFile configFile = sd.open("config.txt", O_WRITE | O_CREAT | O_TRUNC);
  if (configFile) {
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
