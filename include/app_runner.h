#ifndef APP_RUNNER_H
#define APP_RUNNER_H

#include <Arduino.h>
#include <FlipperDisplay.h>


enum class AppState {
    RUNNING,    
    EXIT        
};



typedef AppState (*AppRenderFunc)();


extern AppRenderFunc currentApp;
extern bool appIsRunning;


void startApp(AppRenderFunc app);


void stopApp();


bool isAppRunning();


bool runAppFrame();






void setTextViewerContent(const char* title, const char* content);


void setTextViewerFile(const char* title, const char* filePath);

AppState textViewerApp();


AppState aboutApp();

#endif 

