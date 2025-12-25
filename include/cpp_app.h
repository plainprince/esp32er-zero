#ifndef CPP_APP_H
#define CPP_APP_H

#include <Arduino.h>
#include "app_runner.h"
#include <FlipperDisplay.h>
#include <vector>
#include <functional>
#include <string>







namespace CppApp {
    
    void clear();
    void refresh();
    void print(int x, int y, const char* text);
    void println(const char* text);
    void drawRect(int x, int y, int w, int h);
    void fillRect(int x, int y, int w, int h);
    void drawPixel(int x, int y);
    int width();
    int height();
    
    
    bool button();      
    bool up();          
    bool down();        
    bool left();        
    bool right();       
    
    
    int joystickX();    
    int joystickY();    
    bool buttonRaw();   
    
    
    void exit();        
    bool shouldExit();  
    
    
    void waitFrame(int ms);  
    
    
    
}








typedef void (*CppAppMain)();


struct CppAppInfo {
    std::string name;        
    std::string uiPath;      
    CppAppMain mainFunc;
};


void registerCppApp(const char* name, const char* uiPath, CppAppMain mainFunc);


CppAppInfo* getCppApp(const char* name);


std::vector<CppAppInfo>& getCppApps();


AppState runCppApp(const char* name);


AppState runCppAppByPath(const char* uiPath);


void initCppApps();


















#define CPP_APP(name) void cppapp_##name()


#define REGISTER_CPP_APP(name, uiPath) \
    static const char* _cppAppPath_##name __attribute__((used)) = uiPath


void registerDemoApps();
void registerWifiApps();
void registerIRApps();
void registerBLEApps();

#if __has_include("security_config.h")
    #include "security_config.h"
#else
    #define ENABLE_BLE 0
#endif

#if ENABLE_BLE
// Cleanup function for C++ BLE keyboard objects (called from Lua BLE code)
void cleanupBLEKeyboard();
#endif

#endif 
