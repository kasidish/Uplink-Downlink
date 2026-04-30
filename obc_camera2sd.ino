#include "Arducam_Mega.h"
#include <SPI.h>
#include "SdFat.h"

// These 2 lines must be defined as it is used in internal functions
#define SPI_DRIVER_SELECT 2
#define ENABLE_DEDICATED_SPI 1

SPIClass SD_SPI(PC12, PC11, PC10);
const int chipSelect = PC9;

#define SD_CONFIG SdSpiConfig(chipSelect, DEDICATED_SPI, SD_SCK_MHZ(8), &SD_SPI)

SdFs sd;
FsFile file;
csd_t csd;

#define BUFFER_SIZE 0xff

const int CAM_CS = PE_7;  // Camera CS
uint8_t count = 0;
char name[10] = { 0 };
uint8_t rtLength = 0;
uint8_t imageData = 0;
uint8_t imageDataNext = 0;
uint8_t headFlag = 0;
unsigned int i = 0;
uint8_t imageBuff[BUFFER_SIZE] = { 0 };

Arducam_Mega myCAM(CAM_CS);
uint8_t keyState = 0;
uint8_t isCaptureFlag = 0;


void setup() {

  SPI.setMISO(PB_4);
  SPI.setMOSI(PB_5);
  SPI.setSCLK(PB_3);
  SPI.begin();

  Serial.setTx(PD8);
  Serial.setRx(PD9);
  Serial.begin(115200);

  myCAM.begin();
  myCAM.takePicture(CAM_IMAGE_MODE_VGA, CAM_IMAGE_PIX_FMT_JPG);

  Serial.print("\nInitializing SD card...");

  // we'll use the initialization code from the utility libraries
  // since we're just testing if the card is working!
  if (!sd.begin(SD_CONFIG)) {
    Serial.println("initialization failed. Things to check:");
    Serial.println("* is a card inserted?");
    Serial.println("* is your wiring correct?");
    Serial.println("* did you change the chipSelect pin to match your shield or module?");
    Serial.println("Note: press reset button on the board and reopen this Serial Monitor after fixing your issue!");
    while (1)
      ;
  } else {
    Serial.println("Wiring is correct and a card is present.");
  }

  // print the type of card
  Serial.println();
  Serial.print("Card type:   ");
  switch (sd.card()->type()) {
    case SD_CARD_TYPE_SD1:
      Serial.println("SD1");
      break;
    case SD_CARD_TYPE_SD2:
      Serial.println("SD2");
      break;
    case SD_CARD_TYPE_SDHC:
      Serial.println("SDHC");
      break;
    default:
      Serial.println("Unknown");
  }

  // init the volume of the card
  if (!sd.volumeBegin()) {
    Serial.print("\nvolumeBegin failed. Is the card formatted?\n");
    return;
  }

}

void loop(void) {
  for (int cnt = 0; cnt < 3;cnt++){
    Serial.printf("Taking %d picture\r\n",cnt+1);
    delay(500);
    myCAM.takePicture(CAM_IMAGE_MODE_WQXGA2,CAM_IMAGE_PIX_FMT_JPG);
    while (myCAM.getReceivedLength())
    {
        imageData = imageDataNext;a
        imageDataNext = myCAM.readByte();
        if (headFlag == 1)
        {
            imageBuff[i++]=imageDataNext;  
            if (i >= BUFFER_SIZE)
            {
                file.write(imageBuff, i);
                i = 0;
            }
        }
        if (imageData == 0xff && imageDataNext ==0xd8)
        {
            headFlag = 1;
            sprintf(name,"%d.jpg",count);
            count++;
            if (! file.open(name,O_RDWR | O_CREAT))
            {
                Serial.println(F("File open failed"));
                while (1);
            }
            imageBuff[i++]=imageData;
            imageBuff[i++]=imageDataNext;  
        }
        if (imageData == 0xff && imageDataNext ==0xd9)
        {
            headFlag = 0;
            file.write(imageBuff, i);
            i = 0;
            file.close();
            Serial.println(F("Image save succeed"));
            break;
        }
    }
    delay(100);
  }
  Serial.printf("Done\r\n");
  sd.end();
  while(1){}
}
