#include "./FreeDeck.h"

#include "../settings.h"
#include "./Button.h"
#include "./OledTurboLight.h"
#include <HID-Project.h>
#include <SPI.h>
#include <SdFat.h>
#include <avr/power.h>

#define TYPE_DISPLAY 0
#define TYPE_BUTTON 1

SdFat SD;
File configFile;
Button buttons[BD_COUNT];

int currentPage = 0;
int pageCount;
unsigned short int fileImageDataOffset = 0;
short int contrast = 0;
unsigned char imageCache[IMG_CACHE_SIZE];
unsigned char progressBar[IMG_CACHE_SIZE];

#ifdef CUSTOM_ORDER
byte addressToScreen[] = ADDRESS_TO_SCREEN;
byte addressToButton[] = ADDRESS_TO_BUTTON;
#endif

unsigned long timeOutStartTime;
bool timeOut = false;

int getBitValue(int number, int place) {
  return (number & (1 << place)) >> place;
}

void setMuxAddress(int address, uint8_t type = TYPE_DISPLAY) {
#ifdef CUSTOM_ORDER
  if (type == TYPE_DISPLAY)
    address = addressToScreen[address];
  else if (type == TYPE_BUTTON)
    address = addressToButton[address];
#endif
  int S0 = getBitValue(address, 0);
  digitalWrite(S0_PIN, S0);

#if BD_COUNT > 2
  int S1 = getBitValue(address, 1);
  digitalWrite(S1_PIN, S1);
#endif

#if BD_COUNT > 4
  int S2 = getBitValue(address, 2);
  digitalWrite(S2_PIN, S2);
#endif

// deej steals this do not use
//#if BD_COUNT > 8
//  int S3 = getBitValue(address, 3);
//  digitalWrite(S3_PIN, S3);
//#endif

  delay(1); // wait for multiplexer to switch
}

// TODO remove type
void setMuxAddressDeej(int address, uint8_t type = TYPE_DISPLAY) {
  
  int D_S0 = getBitValue(address, 0);
  digitalWrite(D_S0_PIN, D_S0);

  int D_S1 = getBitValue(address, 1);
  digitalWrite(D_S1_PIN, D_S1);
  
  int D_S2 = getBitValue(address, 2);
  digitalWrite(D_S2_PIN, D_S2);
  
  delay(1); // wait for multiplexer to switch
}

void setGlobalContrast(unsigned short c) {
  if (c == 0)
    c = 1;
  contrast = c;
  for (uint8_t buttonIndex = 0; buttonIndex < BD_COUNT; buttonIndex++) {
    setMuxAddress(buttonIndex, TYPE_DISPLAY);
    delay(1);
    oledSetContrast(c);
  }
}

void setSetting() {
  uint8_t settingCommand;
  configFile.read(&settingCommand, 1);
  if (settingCommand == 1) { // decrease brightness
    contrast = max(contrast - 20, 1);
    setGlobalContrast(contrast);
  } else if (settingCommand == 2) { // increase brightness
    contrast = min(contrast + 20, 255);
    setGlobalContrast(contrast);
  } else if (settingCommand == 3) { // set brightness
    contrast = min(contrast + 20, 255);
    setGlobalContrast(configFile.read());
  }
}

void pressKeys() {
  byte i = 0;
  uint8_t key;
  configFile.read(&key, 1);
  while (key != 0 && i++ < 7) {
    Keyboard.press(KeyboardKeycode(key));
    configFile.read(&key, 1);
    delay(1);
  }
}

void sendText() {
  byte i = 0;
  uint8_t key;
  configFile.read(&key, 1);
  while (key != 0 && i++ < 15) {
    Keyboard.press(KeyboardKeycode(key));
    delay(8);
    if (key < 224) {
      Keyboard.releaseAll();
    }
    configFile.read(&key, 1);
  }
  Keyboard.releaseAll();
}

void changePage() {
  int16_t pageIndex;
  configFile.read(&pageIndex, 2);
  loadPage(pageIndex);
}

void pressSpecialKey() {
  uint16_t key;
  configFile.read(&key, 2);
  Consumer.press((ConsumerKeycode)key);
}

// Expects value between 0 - 1024
void setProgressBar(float value)
{
    float ydec = value / 1024.000; //get filtered value, convert to decimal percentage
	int ysize = round(ydec * 128.000); //convert decimal to a factor of screen size
	
	for(int i = 0; i < IMG_CACHE_SIZE; i++)
	{
		if(i <= ysize)
		{
			progressBar[i] = 0xff;
		}
		else
		{
			progressBar[i] = 0x00;
		}
	}
}

void displayImage(int16_t imageNumber, float value) {
  configFile.seekSet(fileImageDataOffset + imageNumber * 1024L);
  
  uint8_t byteI = 0;
  
  if(value > 0)
  {
    while (configFile.available() && byteI < ((1024 - IMG_CACHE_SIZE) / IMG_CACHE_SIZE)) {
      configFile.read(imageCache, IMG_CACHE_SIZE);
	
      oledLoadBMPPart(imageCache, IMG_CACHE_SIZE, byteI * IMG_CACHE_SIZE);
      byteI++;
    }
    setProgressBar(value);
    oledLoadBMPPart(progressBar, IMG_CACHE_SIZE, byteI * IMG_CACHE_SIZE);
  }
  else {
    while (configFile.available() && byteI < (1024 / IMG_CACHE_SIZE)) {
      configFile.read(imageCache, IMG_CACHE_SIZE);
	
      oledLoadBMPPart(imageCache, IMG_CACHE_SIZE, byteI * IMG_CACHE_SIZE);
      byteI++;
    }
  }
}

void displayDeej(float analogValues[]) {
  setMuxAddress(6, TYPE_DISPLAY);
  
  // TODO makes more sense to start deej icons at page one then offset freedeck start
  // deej screens (starts at offset 4)
  uint8_t pageOffset = 6 * 3; // Page 4
  
  for (uint8_t index = 0; index < FADER_COUNT; index++) {
	  setMuxAddressDeej(index + FADER_COUNT, TYPE_DISPLAY);
	  delay(1);
	  displayImage(pageOffset + index, analogValues[index]);
  }
}

uint8_t getCommand(uint8_t button, uint8_t secondary) {
  configFile.seek((BD_COUNT * currentPage + button + 1) * 16 + 8 * secondary);
  uint8_t command;
  command = configFile.read();
  return command;
}

void onButtonPress(uint8_t buttonIndex, uint8_t secondary) {
  uint8_t command = getCommand(buttonIndex, secondary) & 0xf;
  if (command == 1) {
    changePage();
  } else if (command == 0) {
    pressKeys();
  } else if (command == 3) {
    pressSpecialKey();
  } else if (command == 4) {
    sendText();
  } else if (command == 5) {
    setSetting();
  }
}

void onButtonRelease(uint8_t buttonIndex, uint8_t secondary) {
  uint8_t command = getCommand(buttonIndex, secondary) & 0xf;
  if (command == 0) {
    Keyboard.releaseAll();
  } else if (command == 3) {
    Consumer.releaseAll();
  }
}

void loadPage(int16_t pageIndex) {
  currentPage = pageIndex;
  for (uint8_t buttonIndex = 0; buttonIndex < BD_COUNT; buttonIndex++) {
    uint8_t command = getCommand(buttonIndex, false);
    buttons[buttonIndex].hasSecondary = command > 15;
    buttons[buttonIndex].onPressCallback = onButtonPress;
    buttons[buttonIndex].onReleaseCallback = onButtonRelease;

    setMuxAddress(buttonIndex, TYPE_DISPLAY);
    delay(1);
    displayImage(pageIndex * BD_COUNT + buttonIndex, -1);;
  }
}

void checkButtonState(uint8_t buttonIndex) {
  setMuxAddress(buttonIndex, TYPE_BUTTON);
  uint8_t state = digitalRead(BUTTON_PIN);
  buttons[buttonIndex].update(state);
  return;
}

void initAllDisplays() {
  for (uint8_t buttonIndex = 0; buttonIndex < BD_COUNT; buttonIndex++) {
    buttons[buttonIndex].index = buttonIndex;
    setMuxAddress(buttonIndex, TYPE_DISPLAY);
    delay(1);
    oledInit(0x3c, 0, 0);
    oledFill(255);
  }
  
  setMuxAddress(BD_COUNT, TYPE_DISPLAY);
  
  // deej screens (starts at offset 4)
  for (uint8_t faderIndex = 4; faderIndex < (FADER_COUNT * 2); faderIndex++) {
	  setMuxAddressDeej(faderIndex, TYPE_DISPLAY);
	  delay(1);
	  oledInit(0x3c, 0, 0);
	  oledFill(255);
  }
}

void readSliders(int index) {
  setMuxAddress(6, TYPE_BUTTON);
  setMuxAddressDeej(index, TYPE_BUTTON);
}

void loadConfigFile() {
  configFile = SD.open(CONFIG_NAME, FILE_READ);
  configFile.seek(2);
  configFile.read(&fileImageDataOffset, 2);
  pageCount = (fileImageDataOffset - 1) / BD_COUNT;
  fileImageDataOffset = fileImageDataOffset * 16;
}

void initSdCard() {
  while (!SD.begin(SD_CS_PIN, SD_SCK_MHZ(16))) {
    delay(1);
  }
}

void postSetup() {
  loadConfigFile();
  configFile.seekSet(4);
  setGlobalContrast(configFile.read());
  loadPage(0);
}

void checkTimeOut() {
  unsigned long currentTime = millis();
  if (currentTime - timeOutStartTime >= TIMEOUT_TIME) {
    if (timeOut == false)
      switchScreensOff();
  }
}

void switchScreensOff() {
  timeOut = true;
  for (uint8_t buttonIndex = 0; buttonIndex < BD_COUNT; buttonIndex++) {
    setMuxAddress(buttonIndex, TYPE_DISPLAY);
    delay(1);
    oledFill(0);
  }
}

void switchScreensOn() {
  timeOut = false;
  timeOutStartTime = millis();
  loadPage(currentPage);
}