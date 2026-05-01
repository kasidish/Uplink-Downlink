#include "Arducam_Mega.h"
#include "SdFat.h"
#include "config.h"

// --- Camera Module ---

Arducam_Mega myCAM(CAM_CS);
extern SdFs sd;

bool camera_init() {
  Serial.print(F("[CAM] Initializing Arducam Mega ... "));

  SPI.setMISO(CAM_MISO);
  SPI.setMOSI(CAM_MOSI);
  SPI.setSCLK(CAM_SCLK);
  SPI.begin();

  myCAM.begin();
  Serial.println(F("ready"));
  return true;
}

bool camera_capture_to_sd(uint16_t id) {
  char filename[16];
  snprintf(filename, sizeof(filename), "IMG_%04u.JPG", id);

  Serial.print(F("[CAM] Capturing "));
  Serial.println(filename);

  FsFile imgFile = sd.open(filename, O_WRONLY | O_CREAT | O_TRUNC);
  if (!imgFile) {
    Serial.println(F("[CAM] File open failed"));
    return false;
  }

  myCAM.takePicture(CAM_IMAGE_MODE_VGA, CAM_IMAGE_PIX_FMT_JPG);

  uint8_t imageData = 0;
  uint8_t imageDataNext = 0;
  bool inJpeg = false;
  uint8_t imageBuff[128];
  uint16_t buffLen = 0;
  uint32_t startedAt = millis();

  while (millis() - startedAt < CAMERA_CAPTURE_TIMEOUT_MS) {
    if (!myCAM.getReceivedLength()) {
      delay(1);
      continue;
    }

    imageData = imageDataNext;
    imageDataNext = myCAM.readByte();

    if (!inJpeg && imageData == 0xFF && imageDataNext == 0xD8) {
      inJpeg = true;
      imageBuff[buffLen++] = imageData;
      imageBuff[buffLen++] = imageDataNext;
      continue;
    }

    if (!inJpeg) {
      continue;
    }

    imageBuff[buffLen++] = imageDataNext;
    if (buffLen >= sizeof(imageBuff)) {
      imgFile.write(imageBuff, buffLen);
      buffLen = 0;
    }

    if (imageData == 0xFF && imageDataNext == 0xD9) {
      if (buffLen > 0) {
        imgFile.write(imageBuff, buffLen);
      }
      imgFile.close();
      Serial.println(F("[CAM] Image saved"));
      return true;
    }
  }

  imgFile.close();
  sd.remove(filename);
  Serial.println(F("[CAM] Capture timeout or invalid JPEG"));
  return false;
}
