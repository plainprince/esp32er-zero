#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>
#include <FlipperDisplay.h>


extern int CHAR_WIDTH;
extern int CHAR_HEIGHT;


extern int cursorX;
extern int cursorY;


extern FlipperDisplay* display;


void initUtils(FlipperDisplay* disp);


void customPrint(const char* text, bool invert = false);


void customPrintln(const char* text, bool invert = false);


void customPrintUnlocked(const char* text, bool invert);


void customPrintlnUnlocked(const char* text, bool invert);


void resetCursor();


void showLoading(const char* title, const char* subtitle = nullptr, bool showSpinner = false);


void showLoadingMessage(const char* message);







void initStatusLED();


void setLEDBusy();


void setLEDReady();






void acquireDisplayLock();


void releaseDisplayLock();







void initRenderQueue();


void requestDisplayRefresh();


bool isRenderBusy();


bool isRenderPending();


int getDisplayScale();


int getTextScale();


int getTextHeight();






String loadAboutText();


bool loadLoadingScreen();


void showLoadingScreenOverlay(const char* text = nullptr);


void hideLoadingScreenOverlay();


bool isLoadingScreenVisible();

#endif 

