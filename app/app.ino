// freedeck arduino code for flashing to atmega32u4 based arduinos
// and compatible Copyright (C) 2020 Kilian Gosewisch
//
// This program is free software: you can redistribute it and/or
// modify it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License,
// or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public
// License along with this program. If not, see
// <https://www.gnu.org/licenses/>.

#include <HID-Project.h>

#include "./settings.h"
#include "./src/FreeDeck.h"
#include "./src/FreeDeckSerialAPI.h"
void setup() {
  Serial.begin(4000000);
  //Serial.begin(9600);
  Serial.setTimeout(100);
  delay(BOOT_DELAY);
  Keyboard.begin();
  Consumer.begin();
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(S0_PIN, OUTPUT);
#if BD_COUNT > 2
  pinMode(S1_PIN, OUTPUT);
#endif
#if BD_COUNT > 4
  pinMode(S2_PIN, OUTPUT);
#endif
// Stolen by deej pins
//#if BD_COUNT > 8
//  pinMode(S3_PIN, OUTPUT);
//#endif

  pinMode(D_S0_PIN, OUTPUT);
  pinMode(D_S1_PIN, OUTPUT);
  pinMode(D_S2_PIN, OUTPUT);

   pinMode(A3, INPUT);

  pinMode(D_BUTTON0_PIN, INPUT_PULLUP);
  pinMode(D_BUTTON1_PIN, INPUT_PULLUP);
  pinMode(D_BUTTON2_PIN, INPUT_PULLUP);
  pinMode(D_BUTTON3_PIN, INPUT_PULLUP);

  initAllDisplays();
  delay(100);
  initSdCard();
  postSetup();
  delay(100);
  displayDeej();
}

void loop() {
  handleSerial();
  for (uint8_t buttonIndex = 0; buttonIndex < BD_COUNT; buttonIndex++) {
    checkButtonState(buttonIndex);
  }
  readSliders();
  Serial.println(analogRead(A3));
  displayDeej();;
  if (TIMEOUT_TIME > 0)
    checkTimeOut();
}
