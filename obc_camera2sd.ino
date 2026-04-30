#include "Arducam_Mega.h"
#include "SdFat.h"
#include "config.h"

// --- Camera Module (HAL) ---

// Module owns its hardware resources
Arducam_Mega myCAM(CAM_CS);

bool camera_init() {
  Serial.print(F("[HAL] Initializing Camera ... "));
  SPI.setMISO(CAM_MISO);
  SPI.setMOSI(CAM_MOSI);
  SPI.setSCLK(CAM_SCLK);
  SPI.begin();
  
  myCAM.begin();
  Serial.println(F("Success."));
  return true;
}

bool camera_capture_to_sd(uint16_t id) {
  char filename[15];
  sprintf(filename, "IMG_%d.JPG", id);
  
  Serial.printf("[HAL] Capturing to %s\r\n", filename);
  myCAM.takePicture(CAM_IMAGE_MODE_VGA, CAM_IMAGE_PIX_FMT_JPG);
  
  FsFile imgFile;
  extern SdFs sd; // Use the global SD object from storage module
  if (!imgFile.open(filename, O_WRONLY | O_CREAT | O_TRUNC)) {
    Serial.println(F("[SD] File open failed"));
    return false;
  }

  uint8_t imageData = 0;
  uint8_t imageDataNext = 0;
  uint8_t headFlag = 0;
  uint32_t i = 0;
  uint8_t imageBuff[128]; 

  while (myCAM.getReceivedLength()) {
    imageData = imageDataNext;
    imageDataNext = myCAM.readByte();
    
    if (headFlag == 1) {
      imageBuff[i++] = imageDataNext;
      if (i >= 128) {
        imgFile.write(imageBuff, i);
        i = 0;
      }
    }
    
    if (imageData == 0xff && imageDataNext == 0xd8) {
      headFlag = 1;
      imageBuff[i++] = imageData;
      imageBuff[i++] = imageDataNext;
    }
    
    if (imageData == 0xff && imageDataNext == 0xd9) {
      headFlag = 0;
      imgFile.write(imageBuff, i);
      i = 0;
      imgFile.close();
      Serial.println(F("[HAL] Image saved."));
      return true;
    }
  }
  
  imgFile.close();
  return false;
}
