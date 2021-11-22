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
#include <Wire.h>
#include <Filter.h>

#include "./settings.h"
#include "./src/FreeDeck.h"
#include "./src/FreeDeckSerialAPI.h"

int analogSliderValues[4];
float analogSliderValuesFiltered[4];

// 19 and 20 are A1 and A2
uint8_t buttonPins[] = { 2, 3, 19, 20 };
boolean buttonsPressed[4] = { false, false, false, false };
boolean buttonsToggled[4] = { false, false, false, false };

ExponentialFilter<long> sliderFilter1(30, 500);
ExponentialFilter<long> sliderFilter2(30, 500);
ExponentialFilter<long> sliderFilter3(30, 500);
ExponentialFilter<long> sliderFilter4(30, 500);

void setup() {
  Serial.begin(4000000);
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
}

void loop() {
  handleSerial();
  for (uint8_t buttonIndex = 0; buttonIndex < BD_COUNT; buttonIndex++) {
    checkButtonState(buttonIndex);
  }

  if (TIMEOUT_TIME > 0) {
    checkTimeOut();
  }

  checkSliderButtons();

  updateSliderValues();
  sendSliderValues(); 

  displayDeej(analogSliderValuesFiltered);
}

void checkSliderButtons() {
  for(int i = 0; i < FADER_COUNT; i++) {
	if(!digitalRead(buttonPins[i]) && !buttonsPressed[i]){
      buttonsPressed[i] = true;
      buttonsToggled[i] = !buttonsToggled[i];
    }
    if(digitalRead(buttonPins[i]) && buttonsPressed[i]){
      buttonsPressed[i] = false;
    }
  }
};

void updateSliderValues() {
  for (int i = 0; i < FADER_COUNT; i++) {
     readSliders(i);
     analogSliderValues[i] = buttonsToggled[i] ? 1 : analogRead(A3);
  }

  sliderFilter1.Filter(analogSliderValues[0]);
  analogSliderValuesFiltered[0] = sliderFilter1.Current();
  
  sliderFilter2.Filter(analogSliderValues[1]);
  analogSliderValuesFiltered[1] = sliderFilter2.Current();
  
  sliderFilter3.Filter(analogSliderValues[2]);
  analogSliderValuesFiltered[2] = sliderFilter3.Current();
  
  sliderFilter4.Filter(analogSliderValues[3]);
  analogSliderValuesFiltered[3] = sliderFilter4.Current();
}

void sendSliderValues() {
  // TODO probably can share same baud here, was hearing wierd audio skips though
  Serial.flush();
  Serial.begin(9600);
  String builtString = String("");

  for (int i = 0; i < FADER_COUNT; i++) {
    builtString += String((int)analogSliderValues[i]);

    if (i < FADER_COUNT - 1) {
      builtString += String("|");
    }
  }
  
  Serial.println(builtString);
  
  Serial.flush();
  Serial.begin(4000000);
}
