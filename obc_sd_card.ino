#include <SPI.h>
#include "SdFat.h"

// Note: SD_SPI, SD_CS, and SD_CONFIG are defined in mission_integrated.ino
// sd and nextImageID are also shared globals.

bool storage_init() {
  Serial.print(F("[SD] Initializing SD Card ... "));
  if (!sd.begin(SD_CONFIG)) {
    Serial.println(F("Failed!"));
    return false;
  }
  Serial.println(F("Success."));
  storage_load_config();
  return true;
}

void storage_load_config() {
  if (sd.exists("config.txt")) {
    FsFile configFile = sd.open("config.txt", O_READ);
    if (configFile) {
      nextImageID = configFile.parseInt();
      configFile.close();
      Serial.print(F("[SD] Loaded nextImageID: ")); Serial.println(nextImageID);
    }
  }
}

void storage_save_config() {
  FsFile configFile = sd.open("config.txt", O_WRITE | O_CREAT | O_TRUNC);
  if (configFile) {
    configFile.print(nextImageID);
    configFile.close();
  }
}

bool storage_save_image_data(uint16_t id, Arducam_Mega& cam) {
  char filename[15];
  sprintf(filename, "IMG_%d.JPG", id);
  
  FsFile imgFile = sd.open(filename, O_WRITE | O_CREAT);
  if (!imgFile) {
    Serial.println(F("[SD] File Open Failed!"));
    return false;
  }

  uint8_t buffer[120]; // Matching our data chunk size for consistency
  while (cam.getReceivedLength() > 0) {
    int len = cam.readBuff(buffer, 120);
    imgFile.write(buffer, len);
  }
  imgFile.close();
  Serial.print(F("[SD] Saved: ")); Serial.println(filename);
  return true;
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
