#include "cpp_app.h"
#include "utils.h"
#include "controls.h"
#include <FlipperDisplay.h>
#include <string>
#include <vector>

extern FlipperDisplay* display;


static std::vector<CppAppInfo> cppApps;


static volatile bool exitRequested = false;
static std::string currentCppAppName;


namespace CppApp {
    void clear() {
        if (::display) {
            acquireDisplayLock();
            ::display->clearDisplay();
            resetCursor();
            releaseDisplayLock();
        }
    }
    
    void refresh() {
        
        requestDisplayRefresh();
    }
    
    void print(int x, int y, const char* text) {
        if (::display) {
            cursorX = x;
            cursorY = y;
            customPrint(text, false);
        }
    }
    
    void println(const char* text) {
        customPrintln(text, false);
    }
    
    void drawRect(int x, int y, int w, int h) {
        if (::display) {
            acquireDisplayLock();
            ::display->fillRect(x, y, w, 1, COLOR_WHITE);
            ::display->fillRect(x, y + h - 1, w, 1, COLOR_WHITE);
            ::display->fillRect(x, y, 1, h, COLOR_WHITE);
            ::display->fillRect(x + w - 1, y, 1, h, COLOR_WHITE);
            releaseDisplayLock();
        }
    }
    
    void fillRect(int x, int y, int w, int h) {
        if (::display) {
            acquireDisplayLock();
            ::display->fillRect(x, y, w, h, COLOR_WHITE);
            releaseDisplayLock();
        }
    }
    
    void drawPixel(int x, int y) {
        if (::display) {
            acquireDisplayLock();
            ::display->drawPixel(x, y, COLOR_WHITE);
            releaseDisplayLock();
        }
    }
    
    int width() {
        return ::display ? ::display->width() : 128;
    }
    
    int height() {
        return ::display ? ::display->height() : 64;
    }
    
    
    static bool _inputUpdated = false;
    static bool _up = false;
    static bool _down = false;
    static bool _left = false;
    static bool _right = false;
    static bool _button = false;
    
    void updateInput() {
        updateControls();
        _up = isUpPressed();
        _down = isDownPressed();
        _left = isLeftPressed();
        _right = isRightPressed();
        _button = isButtonReleased();
        _inputUpdated = true;
    }
    
    void resetInputFrame() {
        _inputUpdated = false;
    }
    
    bool button() {
        if (!_inputUpdated) updateInput();
        bool val = _button;
        _button = false;  
        return val;
    }
    
    bool up() {
        if (!_inputUpdated) updateInput();
        bool val = _up;
        _up = false;  
        return val;
    }
    
    bool down() {
        if (!_inputUpdated) updateInput();
        bool val = _down;
        _down = false;  
        return val;
    }
    
    bool left() {
        if (!_inputUpdated) updateInput();
        bool val = _left;
        _left = false;  
        return val;
    }
    
    bool right() {
        if (!_inputUpdated) updateInput();
        bool val = _right;
        _right = false;  
        return val;
    }
    
    void waitFrame(int ms) {
        delay(ms);
        resetInputFrame();
    }
    
    int joystickX() {
        return analogRead(34);  
    }
    
    int joystickY() {
        return analogRead(35);  
    }
    
    bool buttonRaw() {
        return digitalRead(32) == LOW;  
    }
    
    void exit() {
        exitRequested = true;
    }
    
    bool shouldExit() {
        return exitRequested;
    }
}


void registerCppApp(const char* name, const char* uiPath, CppAppMain mainFunc) {
    CppAppInfo info;
    info.name = name;
    info.uiPath = uiPath;
    info.mainFunc = mainFunc;
    cppApps.push_back(info);
    
    Serial.print(F("Registered C++ app: "));
    Serial.print(name);
    Serial.print(F(" at "));
    Serial.println(uiPath);
}

CppAppInfo* getCppApp(const char* name) {
    for (auto& app : cppApps) {
        if (app.name == name) {
            return &app;
        }
    }
    return nullptr;
}

CppAppInfo* getCppAppByPath(const char* uiPath) {
    for (auto& app : cppApps) {
        if (app.uiPath == uiPath) {
            return &app;
        }
    }
    return nullptr;
}

std::vector<CppAppInfo>& getCppApps() {
    return cppApps;
}


static CppAppMain currentMainFunc = nullptr;

static AppState cppAppRunner() {
    if (!currentMainFunc) {
        return AppState::EXIT;
    }
    
    
    currentMainFunc();
    
    
    currentMainFunc = nullptr;
    exitRequested = false;
    
    return AppState::EXIT;
}

AppState runCppApp(const char* name) {
    CppAppInfo* app = getCppApp(name);
    if (!app) {
        Serial.print(F("C++ app not found: "));
        Serial.println(name);
        return AppState::EXIT;
    }
    
    Serial.print(F("Running C++ app: "));
    Serial.println(name);
    
    exitRequested = false;
    currentCppAppName = name;
    currentMainFunc = app->mainFunc;
    
    
    startApp(cppAppRunner);
    
    return AppState::RUNNING;
}

AppState runCppAppByPath(const char* uiPath) {
    CppAppInfo* app = getCppAppByPath(uiPath);
    if (!app) {
        Serial.print(F("C++ app not found at path: "));
        Serial.println(uiPath);
        return AppState::EXIT;
    }
    
    Serial.print(F("Running C++ app: "));
    Serial.print(app->name.c_str());
    Serial.print(F(" from "));
    Serial.println(uiPath);
    
    exitRequested = false;
    currentCppAppName = app->name;
    currentMainFunc = app->mainFunc;
    
    
    startApp(cppAppRunner);
    
    return AppState::RUNNING;
}

#if __has_include("security_config.h")
    #include "security_config.h"
#else
    #define ENABLE_BLE 0
#endif

void initCppApps() {
    
    registerDemoApps();
    registerWifiApps();
    registerIRApps();
#if ENABLE_BLE
    registerBLEApps();
#endif
    
    Serial.println(F("C++ app system initialized"));
    Serial.print(F("Registered apps: "));
    Serial.println(cppApps.size());
}
