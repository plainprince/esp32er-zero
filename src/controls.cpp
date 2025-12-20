#include "controls.h"
#include "config.h"


ControlState controls = {false, false, false, false, false, false};


static bool lastButtonState = false;
static bool yPlusLatch = false;
static bool yMinusLatch = false;
static bool xPlusLatch = false;
static bool xMinusLatch = false;

void initControls() {
    pinMode(JOYSTICK_BUTTON_PIN, INPUT_PULLUP);
    
    
    controls = {false, false, false, false, false, false};
    lastButtonState = false;
    
    
    int yValue = analogRead(JOYSTICK_Y_PIN);
    int xValue = analogRead(JOYSTICK_X_PIN);
    Serial.print(F("Joystick center values - X: "));
    Serial.print(xValue);
    Serial.print(F(", Y: "));
    Serial.println(yValue);
    
    
    
    #if JOYSTICK_INVERT_Y
        yMinusLatch = (yValue < JOYSTICK_LOW_THRESHOLD);
        yPlusLatch = (yValue > JOYSTICK_HIGH_THRESHOLD);
    #else
        yPlusLatch = (yValue < JOYSTICK_LOW_THRESHOLD);
        yMinusLatch = (yValue > JOYSTICK_HIGH_THRESHOLD);
    #endif
    
    #if JOYSTICK_INVERT_X
        xMinusLatch = (xValue < JOYSTICK_LOW_THRESHOLD);
        xPlusLatch = (xValue > JOYSTICK_HIGH_THRESHOLD);
    #else
        xPlusLatch = (xValue < JOYSTICK_LOW_THRESHOLD);
        xMinusLatch = (xValue > JOYSTICK_HIGH_THRESHOLD);
    #endif
}

void updateControls() {
    int yValue = analogRead(JOYSTICK_Y_PIN);
    int xValue = analogRead(JOYSTICK_X_PIN);
    bool buttonState = digitalRead(JOYSTICK_BUTTON_PIN) == LOW;
    
    
    controls.yPlusPressed = false;
    controls.yMinusPressed = false;
    controls.xPlusPressed = false;
    controls.xMinusPressed = false;
    controls.buttonReleased = false;
    
    
    #if JOYSTICK_INVERT_Y
        
        if (yValue < JOYSTICK_LOW_THRESHOLD && !yMinusLatch) {
            controls.yMinusPressed = true;
            yMinusLatch = true;
        } else if (yValue >= JOYSTICK_LOW_THRESHOLD && yValue <= JOYSTICK_HIGH_THRESHOLD) {
            yMinusLatch = false;
        }
        
        if (yValue > JOYSTICK_HIGH_THRESHOLD && !yPlusLatch) {
            controls.yPlusPressed = true;
            yPlusLatch = true;
        } else if (yValue >= JOYSTICK_LOW_THRESHOLD && yValue <= JOYSTICK_HIGH_THRESHOLD) {
            yPlusLatch = false;
        }
    #else
        
        if (yValue < JOYSTICK_LOW_THRESHOLD && !yPlusLatch) {
            controls.yPlusPressed = true;
            yPlusLatch = true;
        } else if (yValue >= JOYSTICK_LOW_THRESHOLD && yValue <= JOYSTICK_HIGH_THRESHOLD) {
            yPlusLatch = false;
        }
        
        if (yValue > JOYSTICK_HIGH_THRESHOLD && !yMinusLatch) {
            controls.yMinusPressed = true;
            yMinusLatch = true;
        } else if (yValue >= JOYSTICK_LOW_THRESHOLD && yValue <= JOYSTICK_HIGH_THRESHOLD) {
            yMinusLatch = false;
        }
    #endif
    
    
    #if JOYSTICK_INVERT_X
        
        if (xValue < JOYSTICK_LOW_THRESHOLD && !xMinusLatch) {
            controls.xMinusPressed = true;
            xMinusLatch = true;
        } else if (xValue >= JOYSTICK_LOW_THRESHOLD && xValue <= JOYSTICK_HIGH_THRESHOLD) {
            xMinusLatch = false;
        }
        
        if (xValue > JOYSTICK_HIGH_THRESHOLD && !xPlusLatch) {
            controls.xPlusPressed = true;
            xPlusLatch = true;
        } else if (xValue >= JOYSTICK_LOW_THRESHOLD && xValue <= JOYSTICK_HIGH_THRESHOLD) {
            xPlusLatch = false;
        }
    #else
        
        if (xValue < JOYSTICK_LOW_THRESHOLD && !xPlusLatch) {
            controls.xPlusPressed = true;
            xPlusLatch = true;
        } else if (xValue >= JOYSTICK_LOW_THRESHOLD && xValue <= JOYSTICK_HIGH_THRESHOLD) {
            xPlusLatch = false;
        }
        
        if (xValue > JOYSTICK_HIGH_THRESHOLD && !xMinusLatch) {
            controls.xMinusPressed = true;
            xMinusLatch = true;
        } else if (xValue >= JOYSTICK_LOW_THRESHOLD && xValue <= JOYSTICK_HIGH_THRESHOLD) {
            xMinusLatch = false;
        }
    #endif
    
    
    controls.buttonPressed = buttonState;
    if (lastButtonState && !buttonState) {
        controls.buttonReleased = true;
    }
    lastButtonState = buttonState;
}

bool isUpPressed() {
    return controls.yMinusPressed;
}

bool isDownPressed() {
    return controls.yPlusPressed;
}

bool isLeftPressed() {
    return controls.xMinusPressed;
}

bool isRightPressed() {
    return controls.xPlusPressed;
}

bool isButtonReleased() {
    return controls.buttonReleased;
}

bool isButtonHeld() {
    return controls.buttonPressed;
}

void waitForButtonRelease() {
    
    while (digitalRead(JOYSTICK_BUTTON_PIN) == LOW) {
        delay(10);
    }
    
    controls.buttonPressed = false;
    controls.buttonReleased = false;
    lastButtonState = false;
}

void consumeButtonRelease() {
    controls.buttonReleased = false;
}

