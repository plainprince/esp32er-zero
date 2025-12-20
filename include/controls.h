#ifndef CONTROLS_H
#define CONTROLS_H

#include <Arduino.h>


#define JOYSTICK_X_PIN 34
#define JOYSTICK_Y_PIN 35
#define JOYSTICK_BUTTON_PIN 32


#define JOYSTICK_LOW_THRESHOLD 1000
#define JOYSTICK_HIGH_THRESHOLD 3000


struct ControlState {
    bool yPlusPressed;
    bool yMinusPressed;
    bool xPlusPressed;
    bool xMinusPressed;
    bool buttonPressed;
    bool buttonReleased;  
};

extern ControlState controls;


void initControls();


void updateControls();


bool isUpPressed();


bool isDownPressed();


bool isLeftPressed();


bool isRightPressed();


bool isButtonReleased();


bool isButtonHeld();


void waitForButtonRelease();


void consumeButtonRelease();

#endif 

