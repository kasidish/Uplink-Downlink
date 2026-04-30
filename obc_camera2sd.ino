#include "Arducam_Mega.h"
#include "SdFat.h"

// --- Camera Module (Store-and-Forward System) ---

// Globals shared across tabs
// myCAM and sd are in mission_integrated.ino

bool camera_init() {
  Serial.print(F("[CAM] Initializing Arducam ... "));
  myCAM.begin();
  Serial.println(F("Success."));
  return true;
}

bool camera_capture_to_sd(uint16_t id) {
  char filename[15];
  sprintf(filename, "IMG_%d.JPG", id);
  
  Serial.printf("[CAM] Capturing to %s\r\n", filename);
  myCAM.takePicture(CAM_IMAGE_MODE_VGA, CAM_IMAGE_PIX_FMT_JPG);
  
  FsFile imgFile;
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
    
    // Start of Image
    if (imageData == 0xff && imageDataNext == 0xd8) {
      headFlag = 1;
      imageBuff[i++] = imageData;
      imageBuff[i++] = imageDataNext;
    }
    
    // End of Image
    if (imageData == 0xff && imageDataNext == 0xd9) {
      headFlag = 0;
      imgFile.write(imageBuff, i);
      i = 0;
      imgFile.close();
      Serial.println(F("[CAM] Image save succeed"));
      return true;
    }
  }
  
  imgFile.close();
  return false;
}
