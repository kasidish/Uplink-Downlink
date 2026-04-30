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


  void setup() {
    Serial.setTx(PD8);
    Serial.setRx(PD9);
    Serial.begin(115200);

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

    // read card volume information
    if (!sd.card()->readCSD(&csd)) {
      Serial.print("SD Read Info failed\n");
      return;
    }

    uint32_t eraseSize = csd.eraseSize();
    Serial.print("Card Size :   ");
    Serial.print((int32_t)(0.000512 * csd.capacity()));
    Serial.println(" MB");

    if (!file.open("Hello.txt", O_RDWR | O_CREAT)) {
      Serial.println("Open File failed");
    }
    Serial.println("Writing to the file...");
    file.println("Hello From NBSPACE");
    file.rewind();

    Serial.println("Reading from the file...");
    Serial.println("--------------------------------------------------------");
    while (file.available()) {
      Serial.write(file.read());
    }
    Serial.println("--------------------------------------------------------");

    file.close();
    sd.end();

    Serial.println(F("Done."));
  }

  void loop(void) {
  }
