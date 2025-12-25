#include "lua_app.h"
#include "utils.h"
#include "controls.h"
#include "lua_fs.h"
#include "keyboard.h"
#include "config.h"
#include "eeprom.h"
#include "cpp_app.h"
#include <FlipperDisplay.h>
#include <Lua.h>
#include <string>
#include <vector>
#include <map>

#if __has_include("security_config.h")
    #include "security_config.h"
#else
    #define ENABLE_ADVANCED_WIFI 0
    #define ENABLE_ADVANCED_IR 0
#endif

#if ENABLE_ADVANCED_WIFI
#include <WiFi.h>
#include "esp_wifi.h"
#endif

#if ENABLE_ADVANCED_IR
#include "ir_remote.h"
#endif

#include <Arduino.h>
#include <LittleFS.h>

extern "C" {
#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"
}

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

// BLE scanning support
#if __has_include("security_config.h")
    #include "security_config.h"
#else
    #define ENABLE_BLE 0
#endif

#if ENABLE_BLE
// Use NimBLE for Lua BLE scanning (more stable, less memory)
#include "NimBLEDevice.h"

struct BLEDeviceInfo {
    char address[20];
    char name[32];
    int rssi;
};

// Forward declaration of BLE related structs/callbacks
class MyAdvertisedDeviceCallbacks;
#endif

// Structs for Interactive GUI App
struct InteractiveAppItem {
    enum ItemType { ITEM_INPUT, ITEM_BUTTON };
    ItemType type;
    std::string label;      
    std::string* value;     
    int callbackRef;   
    int x, y, w;       
    int minInputWidth; 
};

struct InteractiveApp {
    std::string title;
    std::vector<InteractiveAppItem> items;
    int selectedIndex;
    int scrollY;
    bool wantsExit;
    bool needsRender;  
    lua_State* L;
    
    ~InteractiveApp() {
        for (auto& item : items) {
            if (item.type == InteractiveAppItem::ITEM_INPUT && item.value) {
                delete item.value;
            }
        }
    }
};

// Global Application State Container
// This ensures ALL memory related to the Lua app is allocated together and freed together.
struct AppGlobalState {
    Lua* luaInstance = nullptr;
    std::string currentScriptContent;
    bool luaWantsExit = false;
    bool scriptLoaded = false;
    std::map<int, int> pwmDutyCycles;
    
    // GUI
    InteractiveApp* interactiveApp = nullptr;
    
    // BLE State
    #if ENABLE_BLE
    std::vector<BLEDeviceInfo> scannedBLEDevices;
    bool bleScanComplete = false;
    bool nimbleInitialized = false;
    MyAdvertisedDeviceCallbacks* pCallbacks = nullptr;
    #endif
    
    AppGlobalState() {
        // Constructor
    }
    
    ~AppGlobalState() {
        // Force Lua garbage collection before cleanup if we have access to lua_State
        if (interactiveApp && interactiveApp->L) {
            lua_gc(interactiveApp->L, LUA_GCCOLLECT, 0);
        }
        
        // Cleanup GUI first (it holds references to Lua state)
        if (interactiveApp) {
            delete interactiveApp;
            interactiveApp = nullptr;
        }
        
        // Cleanup Lua
        if (luaInstance) {
            delete luaInstance;
            luaInstance = nullptr;
        }
        
        #if ENABLE_BLE
        // Cleanup BLE
        if (nimbleInitialized) {
            NimBLEScan* pScan = NimBLEDevice::getScan();
            if (pScan) {
                 pScan->stop(); 
                 pScan->clearResults();
            }
            NimBLEDevice::deinit(true);
        }
        
        if (pCallbacks) {
            // Memory managed elsewhere or deleted here if we manage it
            // We'll handle it explicitly in MyAdvertisedDeviceCallbacks or helper
        }
        #endif
    }
};

#if ENABLE_BLE
class MyAdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {
public:
    // Needs access to AppGlobalState, but it's global so it's fine.
    void onResult(NimBLEAdvertisedDevice* advertisedDevice);
};
#endif

// The SINGLE global pointer for the current app context
static AppGlobalState* appState = nullptr;

// Helper to ensure app state exists
static void ensureAppState() {
    if (!appState) {
        appState = new AppGlobalState();
    }
}

// Joystick globals (Hardware related, keep persistent)
static volatile int rawJoystickX = 2048;
static volatile int rawJoystickY = 2048;
static volatile bool rawButtonState = false;

#define JOY_X_PIN 34
#define JOY_Y_PIN 35
#define JOY_BTN_PIN 32

// Forward declarations
static void cleanupInteractiveApp();
static int lua_gui_appUpdate(lua_State* L);
static int lua_gui_appExit(lua_State* L);
static int lua_gui_appGetInputValue(lua_State* L);

// --- BLE Implementation ---
#if ENABLE_BLE
void MyAdvertisedDeviceCallbacks::onResult(NimBLEAdvertisedDevice* advertisedDevice) {
    if (!appState) return;
    
    BLEDeviceInfo info;
    strncpy(info.address, advertisedDevice->getAddress().toString().c_str(), sizeof(info.address) - 1);
    info.address[sizeof(info.address) - 1] = '\0';
    
    if (advertisedDevice->haveName()) {
        strncpy(info.name, advertisedDevice->getName().c_str(), sizeof(info.name) - 1);
    } else {
        strncpy(info.name, "(Unknown)", sizeof(info.name) - 1);
    }
    info.name[sizeof(info.name) - 1] = '\0';
    
    info.rssi = advertisedDevice->getRSSI();
    appState->scannedBLEDevices.push_back(info);
}
#endif

// --- Drawing Helpers ---
static void display_drawRect(FlipperDisplay* display, int x, int y, int w, int h, int color) {
    if (!display) return;
    display->fillRect(x, y, w, 1, color);         
    display->fillRect(x, y + h - 1, w, 1, color); 
    display->fillRect(x, y, 1, h, color);         
    display->fillRect(x + w - 1, y, 1, h, color); 
}

static const uint8_t icon_ellipsis[] = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b01100110,
    0b01100110,
    0b00000000,
    0b00000000,
    0b00000000
};

static void drawIconScaled(FlipperDisplay* display, int x, int y, const uint8_t* icon, int color) {
    int scale = display->getScale();
    if (scale > 1) {
        for (int row = 0; row < 8; row++) {
            uint8_t bits = icon[row];
            for (int col = 0; col < 8; col++) {
                if (bits & (1 << (7 - col))) {
                    display->fillRect(x + col * scale, y + row * scale, scale, scale, color);
                }
            }
        }
    } else {
        display->drawBitmap(x, y, icon, 8, 8, color);
    }
}

// Helper for GUI calculations
static void calculateInputDimensions(int x, int y, int w, const char* text, const char* label, int minInputWidth, bool focused,
                                     int* outInputX, int* outInputY, int* outInputWidth, int* outInputHeight, int* outRightX, int* outBottomY) {
    extern int CHAR_WIDTH;
    extern int CHAR_HEIGHT;
    
    int padding = 4 * getTextScale();
    int lineHeight = CHAR_HEIGHT;
    int labelSpacing = 4 * getTextScale();
    
    std::string textStr = text;
    int lineCount = 1;
    for (size_t i = 0; i < textStr.length(); i++) {
        if (textStr[i] == '\n') {
            lineCount++;
        }
    }
    
    int inputHeight = lineHeight * lineCount + padding * 2;
    int labelWidth = 0;
    if (label) {
        labelWidth = strlen(label) * CHAR_WIDTH + labelSpacing;
    }
    
    int inputX = x;
    if (label) {
        inputX = x + labelWidth;
    }
    
    int inputWidth = w - labelWidth;
    if (inputWidth < minInputWidth) {
        inputWidth = minInputWidth;
    }
    
    int actualTotalWidth = labelWidth + inputWidth;
    int rightX = x + actualTotalWidth;
    int bottomY = y + inputHeight;
    
    if (outInputX) *outInputX = inputX;
    if (outInputY) *outInputY = y;
    if (outInputWidth) *outInputWidth = inputWidth;
    if (outInputHeight) *outInputHeight = inputHeight;
    if (outRightX) *outRightX = rightX;
    if (outBottomY) *outBottomY = bottomY;
}

// --- Lua Modules ---

static int lua_display_clear(lua_State* L) {
    extern FlipperDisplay* display;
    if (display) {
        display->clearDisplay();
        resetCursor();
    }
    return 0;
}

static int lua_display_print(lua_State* L) {
    extern FlipperDisplay* display;
    extern int cursorX;
    extern int cursorY;
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    const char* text = luaL_checkstring(L, 3);
    
    if (display) {
        cursorX = x;
        cursorY = y;
        display->setCursor(x, y);
        display->print(text);
        cursorX += strlen(text) * CHAR_WIDTH;
    }
    return 0;
}

static int lua_display_println(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    customPrintlnUnlocked(text, false);
    return 0;
}

static int lua_display_drawRect(lua_State* L) {
    extern FlipperDisplay* display;
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int w = luaL_checkinteger(L, 3);
    int h = luaL_checkinteger(L, 4);
    
    if (display) {
        display_drawRect(display, x, y, w, h, COLOR_WHITE);
    }
    return 0;
}

static int lua_display_fillRect(lua_State* L) {
    extern FlipperDisplay* display;
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int w = luaL_checkinteger(L, 3);
    int h = luaL_checkinteger(L, 4);
    
    if (display) {
        display->fillRect(x, y, w, h, COLOR_WHITE);
    }
    return 0;
}

static int lua_display_drawPixel(lua_State* L) {
    extern FlipperDisplay* display;
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    
    if (display) {
        display->drawPixel(x, y, COLOR_WHITE);
    }
    return 0;
}

static int lua_display_refresh(lua_State* L) {
    requestDisplayRefresh();
    return 0;
}

static int lua_display_width(lua_State* L) {
    extern FlipperDisplay* display;
    int w = 128;
    if (display) {
        w = display->width();
    }
    lua_pushinteger(L, w);
    return 1;
}

static int lua_display_height(lua_State* L) {
    extern FlipperDisplay* display;
    int h = 64;
    if (display) {
        h = display->height();
    }
    lua_pushinteger(L, h);
    return 1;
}

static int lua_display_textScale(lua_State* L) {
    lua_pushinteger(L, getTextScale());
    return 1;
}

static int lua_display_textHeight(lua_State* L) {
    lua_pushinteger(L, getTextHeight());
    return 1;
}

static int lua_display_setTextColor(lua_State* L) {
    extern FlipperDisplay* display;
    int fg = luaL_checkinteger(L, 1);
    int bg = luaL_checkinteger(L, 2);
    
    if (display) {
        display->setTextColor(fg, bg);
    }
    return 0;
}

static int lua_display_setCursor(lua_State* L) {
    extern FlipperDisplay* display;
    extern int cursorX;
    extern int cursorY;
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    
    if (display) {
        cursorX = x;
        cursorY = y;
        display->setCursor(x, y);
    }
    return 0;
}

static int lua_display_busy(lua_State* L) {
    lua_pushboolean(L, isRenderBusy());
    return 1;
}

static int lua_display_lock(lua_State* L) {
    acquireDisplayLock();
    return 0;
}

static int lua_display_unlock(lua_State* L) {
    releaseDisplayLock();
    return 0;
}

static int luaopen_display(lua_State* L) {
    static const luaL_Reg displayLib[] = {
        {"clear", lua_display_clear},
        {"print", lua_display_print},
        {"println", lua_display_println},
        {"drawRect", lua_display_drawRect},
        {"fillRect", lua_display_fillRect},
        {"drawPixel", lua_display_drawPixel},
        {"refresh", lua_display_refresh},
        {"width", lua_display_width},
        {"height", lua_display_height},
        {"textScale", lua_display_textScale},
        {"textHeight", lua_display_textHeight},
        {"setTextColor", lua_display_setTextColor},
        {"setCursor", lua_display_setCursor},
        {"busy", lua_display_busy},
        {"lock", lua_display_lock},
        {"unlock", lua_display_unlock},
        {NULL, NULL}
    };
    luaL_newlib(L, displayLib);
    return 1;
}

// Input Module
static int lua_input_button(lua_State* L) {
    lua_pushboolean(L, isButtonReleased());
    return 1;
}

static int lua_input_up(lua_State* L) {
    lua_pushboolean(L, isUpPressed());
    return 1;
}

static int lua_input_down(lua_State* L) {
    lua_pushboolean(L, isDownPressed());
    return 1;
}

static int lua_input_left(lua_State* L) {
    lua_pushboolean(L, isLeftPressed());
    return 1;
}

static int lua_input_right(lua_State* L) {
    lua_pushboolean(L, isRightPressed());
    return 1;
}

static int lua_input_joystickX(lua_State* L) {
    lua_pushinteger(L, analogRead(JOY_X_PIN));
    return 1;
}

static int lua_input_joystickY(lua_State* L) {
    lua_pushinteger(L, analogRead(JOY_Y_PIN));
    return 1;
}

static int lua_input_buttonRaw(lua_State* L) {
    lua_pushboolean(L, digitalRead(JOY_BTN_PIN) == LOW);
    return 1;
}

static int luaopen_input(lua_State* L) {
    static const luaL_Reg inputLib[] = {
        {"button", lua_input_button},
        {"up", lua_input_up},
        {"down", lua_input_down},
        {"left", lua_input_left},
        {"right", lua_input_right},
        {"joystickX", lua_input_joystickX},
        {"joystickY", lua_input_joystickY},
        {"buttonRaw", lua_input_buttonRaw},
        {NULL, NULL}
    };
    luaL_newlib(L, inputLib);
    return 1;
}

// App Module
static int lua_app_exit(lua_State* L) {
    if (appState) appState->luaWantsExit = true;
    return 0;
}

static int lua_app_delay(lua_State* L) {
    int ms = luaL_checkinteger(L, 1);
    delay(ms);
    return 0;
}

static int lua_app_millis(lua_State* L) {
    lua_pushinteger(L, millis());
    return 1;
}

static int luaopen_app(lua_State* L) {
    static const luaL_Reg appLib[] = {
        {"exit", lua_app_exit},
        {"delay", lua_app_delay},
        {"millis", lua_app_millis},
        {NULL, NULL}
    };
    luaL_newlib(L, appLib);
    return 1;
}

// GPIO Module
static int lua_gpio_mode(lua_State* L) {
    int pin = luaL_checkinteger(L, 1);
    const char* mode = luaL_checkstring(L, 2);
    
    if (strcmp(mode, "output") == 0 || strcmp(mode, "OUTPUT") == 0) {
        pinMode(pin, OUTPUT);
    } else if (strcmp(mode, "input") == 0 || strcmp(mode, "INPUT") == 0) {
        pinMode(pin, INPUT);
    } else if (strcmp(mode, "input_pullup") == 0 || strcmp(mode, "INPUT_PULLUP") == 0) {
        pinMode(pin, INPUT_PULLUP);
    } else if (strcmp(mode, "input_pulldown") == 0 || strcmp(mode, "INPUT_PULLDOWN") == 0) {
        pinMode(pin, INPUT_PULLDOWN);
    }
    return 0;
}

static int lua_gpio_write(lua_State* L) {
    int pin = luaL_checkinteger(L, 1);
    int value = lua_toboolean(L, 2) ? HIGH : LOW;
    digitalWrite(pin, value);
    return 0;
}

static int lua_gpio_read(lua_State* L) {
    int pin = luaL_checkinteger(L, 1);
    lua_pushboolean(L, digitalRead(pin) == HIGH);
    return 1;
}

static int lua_gpio_analogRead(lua_State* L) {
    int pin = luaL_checkinteger(L, 1);
    lua_pushinteger(L, analogRead(pin));
    return 1;
}

static int lua_gpio_analogWrite(lua_State* L) {
    int pin = luaL_checkinteger(L, 1);
    int value = luaL_checkinteger(L, 2);
    analogWrite(pin, value);
    return 0;
}

static int lua_gpio_freePins(lua_State* L) {
    int usedPins[] = {2, 32, 34, 35};
    int usedPinsCount = 4;
    #if DISPLAY_TYPE == SSD1306
        int displayPins[] = {21, 22};
        int displayPinsCount = 2;
    #elif DISPLAY_TYPE == EPAPER
        int displayPins[] = {4, 5, 16, 17, 18, 23};
        int displayPinsCount = 6;
    #elif DISPLAY_TYPE == DUAL
        int displayPins[] = {4, 5, 16, 17, 18, 21, 22, 23};
        int displayPinsCount = 8;
    #else
        int displayPins[] = {};
        int displayPinsCount = 0;
    #endif
    
    bool pinUsed[40] = {false};
    for (int i = 0; i < usedPinsCount; i++) if (usedPins[i] < 40) pinUsed[usedPins[i]] = true;
    for (int i = 0; i < displayPinsCount; i++) if (displayPins[i] < 40) pinUsed[displayPins[i]] = true;
    
    int validPins[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
        21, 22, 23, 25, 26, 27, 32, 33, 34, 35, 36, 37, 38, 39};
    int validPinsCount = sizeof(validPins) / sizeof(validPins[0]);
    
    lua_createtable(L, 0, 0);
    int tableIndex = 1;
    for (int i = 0; i < validPinsCount; i++) {
        int pin = validPins[i];
        if (!pinUsed[pin]) {
            lua_pushinteger(L, tableIndex++);
            lua_pushinteger(L, pin);
            lua_settable(L, -3);
        }
    }
    return 1;
}

static int luaopen_gpio(lua_State* L) {
    static const luaL_Reg gpioLib[] = {
        {"mode", lua_gpio_mode},
        {"write", lua_gpio_write},
        {"read", lua_gpio_read},
        {"analogRead", lua_gpio_analogRead},
        {"analogWrite", lua_gpio_analogWrite},
        {"freePins", lua_gpio_freePins},
        {NULL, NULL}
    };
    luaL_newlib(L, gpioLib);
    return 1;
}

// Status LED
static int lua_statusled_on(lua_State* L) {
    extern bool ledInitialized;
    if (ledInitialized) digitalWrite(LED_BUILTIN, HIGH);
    return 0;
}

static int lua_statusled_off(lua_State* L) {
    extern bool ledInitialized;
    if (ledInitialized) digitalWrite(LED_BUILTIN, LOW);
    return 0;
}

static int lua_statusled_toggle(lua_State* L) {
    extern bool ledInitialized;
    if (ledInitialized) digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    return 0;
}

static int luaopen_statusled(lua_State* L) {
    static const luaL_Reg statusledLib[] = {
        {"on", lua_statusled_on},
        {"off", lua_statusled_off},
        {"toggle", lua_statusled_toggle},
        {NULL, NULL}
    };
    luaL_newlib(L, statusledLib);
    return 1;
}

// PWM Module - Needs AppState access
static int lua_pwm_setup(lua_State* L) {
    if (!appState) return 0;
    int pin = luaL_checkinteger(L, 1);
    int frequency = luaL_optinteger(L, 2, 5000);
    int resolution = luaL_optinteger(L, 3, 8);
    
    static int pwmChannel = 0;
    ledcSetup(pwmChannel, frequency, resolution);
    ledcAttachPin(pin, pwmChannel);
    
    appState->pwmDutyCycles[pin] = 0;
    pwmChannel = (pwmChannel + 1) % 16;
    
    return 0;
}

static int lua_pwm_write(lua_State* L) {
    if (!appState) return 0;
    int pin = luaL_checkinteger(L, 1);
    int dutyCycle = luaL_checkinteger(L, 2);
    
    if (dutyCycle < 0) dutyCycle = 0;
    if (dutyCycle > 255) dutyCycle = 255;
    
    appState->pwmDutyCycles[pin] = dutyCycle;
    ledcWrite(pin, dutyCycle);
    return 0;
}

static int lua_pwm_read(lua_State* L) {
    if (!appState) {
        lua_pushinteger(L, 0);
        return 1;
    }
    int pin = luaL_checkinteger(L, 1);
    if (appState->pwmDutyCycles.find(pin) != appState->pwmDutyCycles.end()) {
        lua_pushinteger(L, appState->pwmDutyCycles[pin]);
    } else {
        lua_pushinteger(L, 0);
    }
    return 1;
}

static int lua_pwm_adjust(lua_State* L) {
    if (!appState) return 0;
    int pin = luaL_checkinteger(L, 1);
    int delta = luaL_checkinteger(L, 2);
    
    int currentDuty = 0;
    if (appState->pwmDutyCycles.find(pin) != appState->pwmDutyCycles.end()) {
        currentDuty = appState->pwmDutyCycles[pin];
    }
    
    int newDuty = currentDuty + delta;
    if (newDuty < 0) newDuty = 0;
    if (newDuty > 255) newDuty = 255;
    
    appState->pwmDutyCycles[pin] = newDuty;
    ledcWrite(pin, newDuty);
    
    lua_pushinteger(L, newDuty);
    return 1;
}

static int luaopen_pwm(lua_State* L) {
    static const luaL_Reg pwmLib[] = {
        {"setup", lua_pwm_setup},
        {"write", lua_pwm_write},
        {"read", lua_pwm_read},
        {"adjust", lua_pwm_adjust},
        {NULL, NULL}
    };
    luaL_newlib(L, pwmLib);
    return 1;
}

// BLE Module - Needs AppState access
#if ENABLE_BLE
static int lua_ble_scan(lua_State* L) {
    if (!appState) return 0;
    
    int scanTime = luaL_optinteger(L, 1, 5); 
    
    appState->scannedBLEDevices.clear();
    appState->scannedBLEDevices.shrink_to_fit();
    appState->bleScanComplete = false;
    
    cleanupBLEKeyboard();
    
    #if ENABLE_ADVANCED_WIFI
    Serial.println("BLE Scan: Stopping WiFi");
    WiFi.mode(WIFI_OFF);
    WiFi.disconnect(true);
    esp_wifi_stop();
    delay(500);
    #endif
    
    if (appState->nimbleInitialized) {
        Serial.println("BLE Scan: Deinitializing NimBLE");
        NimBLEScan* pScan = NimBLEDevice::getScan();
        if (pScan) {
             pScan->stop(); 
             pScan->clearResults();
        }
        NimBLEDevice::deinit(true);
        appState->nimbleInitialized = false;
        delay(100);
    }
    
    Serial.println("BLE Scan: Initializing NimBLE");
    NimBLEDevice::init("ESP32-Scanner");
    appState->nimbleInitialized = true;
    delay(500);
    
    NimBLEScan* pBLEScan = NimBLEDevice::getScan();
    if (!pBLEScan) {
        Serial.println("BLE Scan: ERROR - Failed to get scan object");
        NimBLEDevice::deinit(true);
        appState->nimbleInitialized = false;
        lua_pushboolean(L, false);
        return 1;
    }
    
    if (!appState->pCallbacks) {
        appState->pCallbacks = new MyAdvertisedDeviceCallbacks();
    }
    
    pBLEScan->setAdvertisedDeviceCallbacks(appState->pCallbacks, false);
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    
    Serial.print("BLE Scan: Starting scan for ");
    Serial.print(scanTime);
    Serial.println(" seconds");
    
    pBLEScan->start(scanTime, false);
    pBLEScan->clearResults();
    
    appState->bleScanComplete = true;
    Serial.println("BLE Scan: Complete");
    
    lua_pushboolean(L, true);
    return 1;
}

static int lua_ble_getCount(lua_State* L) {
    if (!appState) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, appState->scannedBLEDevices.size());
    return 1;
}

static int lua_ble_getDevice(lua_State* L) {
    if (!appState) { lua_pushnil(L); return 1; }
    int index = luaL_checkinteger(L, 1) - 1; 
    
    if (index < 0 || index >= (int)appState->scannedBLEDevices.size()) {
        lua_pushnil(L);
        return 1;
    }
    
    lua_newtable(L);
    lua_pushstring(L, "address");
    lua_pushstring(L, appState->scannedBLEDevices[index].address);
    lua_settable(L, -3);
    
    lua_pushstring(L, "name");
    lua_pushstring(L, appState->scannedBLEDevices[index].name);
    lua_settable(L, -3);
    
    lua_pushstring(L, "rssi");
    lua_pushinteger(L, appState->scannedBLEDevices[index].rssi);
    lua_settable(L, -3);
    
    return 1;
}

static int lua_ble_isComplete(lua_State* L) {
    if (!appState) { lua_pushboolean(L, false); return 1; }
    lua_pushboolean(L, appState->bleScanComplete);
    return 1;
}

static int lua_ble_cleanup(lua_State* L) {
    if (!appState) return 0;
    Serial.println("BLE: Lua cleanup called");
    
    appState->scannedBLEDevices.clear();
    appState->scannedBLEDevices.shrink_to_fit();
    
    cleanupBLEKeyboard();
    
    if (appState->pCallbacks) {
        delete appState->pCallbacks;
        appState->pCallbacks = nullptr;
    }
    
    if (appState->nimbleInitialized) {
        NimBLEScan* pScan = NimBLEDevice::getScan();
        if (pScan) {
             pScan->stop(); 
             pScan->clearResults();
        }
        NimBLEDevice::deinit(true);
        appState->nimbleInitialized = false;
    }
    
    delay(500);
    return 0;
}

static int luaopen_ble(lua_State* L) {
    static const luaL_Reg bleLib[] = {
        {"scan", lua_ble_scan},
        {"getCount", lua_ble_getCount},
        {"getDevice", lua_ble_getDevice},
        {"isComplete", lua_ble_isComplete},
        {"cleanup", lua_ble_cleanup},
        {NULL, NULL}
    };
    luaL_newlib(L, bleLib);
    return 1;
}
#endif

// WiFi and EEPROM modules use minimal globals, can keep as is for now or refactor later
// ... (Keeping them as is but ensuring they don't leak) ...
#if ENABLE_ADVANCED_WIFI
// ... lua_wifi functions ...
static int lua_wifi_scanStart(lua_State* L) {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    int n = WiFi.scanNetworks(false, true);
    int attempts = 0;
    while (WiFi.scanComplete() == -1 && attempts < 100) {
        delay(100);
        attempts++;
    }
    n = WiFi.scanComplete();
    if (n < 0) n = 0;
    lua_pushinteger(L, n);
    return 1;
}
// ... other wifi functions ...
static int lua_wifi_scanGetCount(lua_State* L) {
    int n = WiFi.scanComplete();
    if (n < 0) n = 0;
    lua_pushinteger(L, n);
    return 1;
}
static int lua_wifi_scanGetSSID(lua_State* L) {
    int index = luaL_checkinteger(L, 1);
    String ssid = WiFi.SSID(index);
    if (ssid.length() == 0) ssid = "(Hidden)";
    lua_pushstring(L, ssid.c_str());
    return 1;
}
static int lua_wifi_scanGetRSSI(lua_State* L) {
    int index = luaL_checkinteger(L, 1);
    lua_pushinteger(L, WiFi.RSSI(index));
    return 1;
}
static int lua_wifi_scanGetEncryption(lua_State* L) {
    int index = luaL_checkinteger(L, 1);
    lua_pushinteger(L, (int)WiFi.encryptionType(index));
    return 1;
}
static int lua_wifi_scanDelete(lua_State* L) {
    WiFi.scanDelete();
    return 0;
}
static int lua_wifi_connect(lua_State* L) {
    const char* ssid = luaL_checkstring(L, 1);
    const char* password = luaL_optstring(L, 2, "");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
    }
    bool connected = (WiFi.status() == WL_CONNECTED);
    lua_pushboolean(L, connected);
    if (connected) lua_pushstring(L, WiFi.localIP().toString().c_str());
    else lua_pushstring(L, "");
    return 2;
}
static int lua_wifi_status(lua_State* L) {
    lua_pushinteger(L, (int)WiFi.status());
    return 1;
}
static int lua_wifi_disconnect(lua_State* L) {
    WiFi.disconnect();
    return 0;
}
static int luaopen_wifi(lua_State* L) {
    static const luaL_Reg wifiLib[] = {
        {"scanStart", lua_wifi_scanStart},
        {"scanGetCount", lua_wifi_scanGetCount},
        {"scanGetSSID", lua_wifi_scanGetSSID},
        {"scanGetRSSI", lua_wifi_scanGetRSSI},
        {"scanGetEncryption", lua_wifi_scanGetEncryption},
        {"scanDelete", lua_wifi_scanDelete},
        {"connect", lua_wifi_connect},
        {"status", lua_wifi_status},
        {"disconnect", lua_wifi_disconnect},
        {NULL, NULL}
    };
    luaL_newlib(L, wifiLib);
    return 1;
}
#endif

// ... EEPROM module ...
static int lua_eeprom_writeString(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    const char* value = luaL_checkstring(L, 2);
    bool result = eepromWriteString(key, value);
    lua_pushboolean(L, result);
    return 1;
}
static int lua_eeprom_readString(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    const char* defaultValue = luaL_optstring(L, 2, "");
    char buffer[512];
    if (eepromReadString(key, buffer, sizeof(buffer))) lua_pushstring(L, buffer);
    else lua_pushstring(L, defaultValue);
    return 1;
}
static int lua_eeprom_writeInt(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    int value = luaL_checkinteger(L, 2);
    bool result = eepromWriteInt(key, value);
    lua_pushboolean(L, result);
    return 1;
}
static int lua_eeprom_readInt(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    int defaultValue = luaL_optinteger(L, 2, 0);
    int value = eepromReadInt(key, defaultValue);
    lua_pushinteger(L, value);
    return 1;
}
static int lua_eeprom_clear(lua_State* L) {
    bool result = eepromClear();
    lua_pushboolean(L, result);
    return 1;
}
static int lua_eeprom_keyExists(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    lua_pushboolean(L, eepromKeyExists(key));
    return 1;
}
static int luaopen_eeprom(lua_State* L) {
    static const luaL_Reg eepromLib[] = {
        {"writeString", lua_eeprom_writeString},
        {"readString", lua_eeprom_readString},
        {"writeInt", lua_eeprom_writeInt},
        {"readInt", lua_eeprom_readInt},
        {"clear", lua_eeprom_clear},
        {"keyExists", lua_eeprom_keyExists},
        {NULL, NULL}
    };
    luaL_newlib(L, eepromLib);
    return 1;
}

// ... Filesystem module ...
static int lua_filesystem_open(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    const char* mode = luaL_optstring(L, 2, "r");
    File* file = new File(LittleFS.open(path, mode));
    if (!file || !*file) {
        delete file;
        lua_pushnil(L);
        return 1;
    }
    lua_newtable(L);
    lua_pushlightuserdata(L, file);
    lua_setfield(L, -2, "_fileptr");
    
    lua_pushcfunction(L, [](lua_State* L) {
        lua_getfield(L, 1, "_fileptr");
        File* f = (File*)lua_touserdata(L, -1);
        if (f && *f) {
            String line = f->readStringUntil('\n');
            lua_pushstring(L, line.c_str());
            return 1;
        }
        return 0;
    });
    lua_setfield(L, -2, "readLine");
    
    lua_pushcfunction(L, [](lua_State* L) {
        lua_getfield(L, 1, "_fileptr");
        File* f = (File*)lua_touserdata(L, -1);
        if (f && *f) {
            f->seek(0);
            size_t size = f->size();
            std::string content = "";
            content.reserve(size + 1);
            while (f->available()) {
                char buffer[256];
                size_t bytesRead = f->readBytes(buffer, sizeof(buffer) - 1);
                if (bytesRead > 0) {
                    buffer[bytesRead] = '\0';
                    content += buffer;
                } else break;
            }
            lua_pushstring(L, content.c_str());
            return 1;
        }
        return 0;
    });
    lua_setfield(L, -2, "read");
    
    lua_pushcfunction(L, [](lua_State* L) {
        lua_getfield(L, 1, "_fileptr");
        File* f = (File*)lua_touserdata(L, -1);
        if (f && *f) {
            f->close();
            delete f;
        }
        return 0;
    });
    lua_setfield(L, -2, "close");
    
    lua_pushcfunction(L, [](lua_State* L) {
        lua_pushcfunction(L, [](lua_State* L) {
            lua_getfield(L, 1, "_fileptr");
            File* f = (File*)lua_touserdata(L, -1);
            if (f && *f && f->available()) {
                String line = f->readStringUntil('\n');
                line.trim();
                lua_pushstring(L, line.c_str());
                return 1;
            }
            return 0;
        });
        lua_pushvalue(L, 1);
        return 2;
    });
    lua_setfield(L, -2, "lines");
    return 1;
}

static int lua_filesystem_exists(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    lua_pushboolean(L, LittleFS.exists(path));
    return 1;
}

static int luaopen_filesystem(lua_State* L) {
    static const luaL_Reg filesystemLib[] = {
        {"open", lua_filesystem_open},
        {"exists", lua_filesystem_exists},
        {NULL, NULL}
    };
    luaL_newlib(L, filesystemLib);
    return 1;
}

// ... GUI functions ...
static int lua_gui_keyboard(lua_State* L) {
    const char* title = luaL_checkstring(L, 1);
    const char* initial = luaL_optstring(L, 2, "");
    char buffer[128];
    strncpy(buffer, initial, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = 0;
    if (showKeyboard(title, buffer, sizeof(buffer))) {
        lua_pushstring(L, buffer);
        return 1;
    }
    return 0; 
}

static int lua_gui_drawInput(lua_State* L) {
    extern FlipperDisplay* display;
    extern int CHAR_WIDTH;
    extern int CHAR_HEIGHT;
    if (!display) return 0;
    
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int w = luaL_checkinteger(L, 3);
    const char* text = luaL_checkstring(L, 4);
    bool focused = lua_toboolean(L, 5);
    const char* label = luaL_optstring(L, 6, NULL); 
    int minInputWidth = luaL_optinteger(L, 7, 40 * getTextScale());
    
    int padding = 4 * getTextScale(); 
    int lineHeight = CHAR_HEIGHT;
    int labelSpacing = 4 * getTextScale(); 
    
    std::string textStr = text;
    int lineCount = 1;
    for (size_t i = 0; i < textStr.length(); i++) {
        if (textStr[i] == '\n') lineCount++;
    }
    
    int inputHeight = lineHeight * lineCount + padding * 2;
    int labelWidth = 0;
    if (label) labelWidth = strlen(label) * CHAR_WIDTH + labelSpacing;
    
    int inputX = x;
    if (label) {
        display->setTextColor(COLOR_WHITE, COLOR_BLACK);
        int labelY = y + (inputHeight - CHAR_HEIGHT) / 2;
        display->setCursor(x, labelY);
        display->print(label);
        inputX = x + labelWidth;
    }
    
    int inputWidth = w - labelWidth;
    if (inputWidth < minInputWidth) inputWidth = minInputWidth;
    
    if (focused) {
        display->fillRect(inputX, y, inputWidth, inputHeight, COLOR_WHITE);
        display->fillRect(inputX + 2, y + 2, inputWidth - 4, inputHeight - 4, COLOR_BLACK);
        display->setTextColor(COLOR_WHITE, COLOR_BLACK);
    } else {
        display_drawRect(display, inputX, y, inputWidth, inputHeight, COLOR_WHITE);
        display->setTextColor(COLOR_WHITE, COLOR_BLACK);
    }
    
    int maxChars = (inputWidth - padding * 2) / CHAR_WIDTH;
    int currentY = y + padding;
    size_t startIdx = 0;
    
    for (int line = 0; line < lineCount; line++) {
        size_t endIdx = textStr.find('\n', startIdx);
        if (endIdx == std::string::npos) endIdx = textStr.length();
        std::string lineText = textStr.substr(startIdx, endIdx - startIdx);
        if ((int)lineText.length() > maxChars) lineText = lineText.substr(lineText.length() - maxChars);
        
        display->setCursor(inputX + padding, currentY);
        display->print(lineText.c_str());
        currentY += lineHeight;
        startIdx = endIdx + 1;
    }
    return 0;
}

static int lua_gui_getInputYBelow(lua_State* L) {
    extern int CHAR_HEIGHT;
    int y = luaL_checkinteger(L, 1);
    const char* text = luaL_checkstring(L, 2);
    int padding = 4 * getTextScale();
    int lineHeight = CHAR_HEIGHT;
    std::string textStr = text;
    int lineCount = 1;
    for (size_t i = 0; i < textStr.length(); i++) if (textStr[i] == '\n') lineCount++;
    int inputHeight = lineHeight * lineCount + padding * 2;
    lua_pushinteger(L, y + inputHeight);
    return 1;
}

static int lua_gui_getInputRightX(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int w = luaL_checkinteger(L, 3);
    const char* text = luaL_checkstring(L, 4);
    const char* label = luaL_optstring(L, 5, NULL);
    int minInputWidth = luaL_optinteger(L, 6, 40 * getTextScale());
    bool focused = (lua_gettop(L) >= 7 && !lua_isnoneornil(L, 7)) ? lua_toboolean(L, 7) : false;  
    int rightX;
    calculateInputDimensions(x, y, w, text, label, minInputWidth, focused, NULL, NULL, NULL, NULL, &rightX, NULL);
    lua_pushinteger(L, rightX);
    return 1;
}

static int lua_gui_getInputBottomY(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int w = luaL_checkinteger(L, 3);
    const char* text = luaL_checkstring(L, 4);
    const char* label = luaL_optstring(L, 5, NULL);
    bool focused = (lua_gettop(L) >= 6 && !lua_isnoneornil(L, 6)) ? lua_toboolean(L, 6) : false;  
    int bottomY;
    calculateInputDimensions(x, y, w, text, label, 0, focused, NULL, NULL, NULL, NULL, NULL, &bottomY);
    lua_pushinteger(L, bottomY);
    return 1;
}

static int lua_gui_getInputLeftX(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int w = luaL_checkinteger(L, 3);
    const char* text = luaL_checkstring(L, 4);
    const char* label = luaL_optstring(L, 5, NULL);
    int minInputWidth = luaL_optinteger(L, 6, 40 * getTextScale());
    bool focused = (lua_gettop(L) >= 7 && !lua_isnoneornil(L, 7)) ? lua_toboolean(L, 7) : false;  
    int inputX;
    calculateInputDimensions(x, y, w, text, label, minInputWidth, focused, &inputX, NULL, NULL, NULL, NULL, NULL);
    lua_pushinteger(L, inputX);
    return 1;
}

static int lua_gui_getInputTopY(lua_State* L) {
    int y = luaL_checkinteger(L, 2);
    lua_pushinteger(L, y);
    return 1;
}

static int lua_gui_drawButton(lua_State* L) {
    extern FlipperDisplay* display;
    extern int CHAR_WIDTH;
    extern int CHAR_HEIGHT;
    if (!display) return 0;
    
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    const char* text = luaL_checkstring(L, 3);
    bool focused = lua_toboolean(L, 4);
    bool clicked = lua_toboolean(L, 5);
    
    int textW = strlen(text) * CHAR_WIDTH;
    int textH = CHAR_HEIGHT;
    int padding = 4 * getTextScale(); 
    int btnW = textW + padding * 2;
    int btnH = textH + padding * 2;
    int displayW = display->width();
    int textScale = getTextScale();
    int availableWidth = displayW - x; 
    
    bool needsTruncation = false;
    std::string displayText = text;
    
    if (btnW > availableWidth) {
        int maxTextW = availableWidth - padding * 2;
        int maxChars = maxTextW / CHAR_WIDTH;
        if (maxChars > 0) displayText = std::string(text).substr(0, maxChars);
        else displayText = "";
        btnW = availableWidth;
        needsTruncation = true;
    }
    
    int btnX = x;
    int btnY = y;
    int tx = btnX + padding;
    int ty = btnY + padding;
    
    if (clicked) {
        display->fillRect(btnX, btnY, btnW, btnH, COLOR_WHITE);
        display->setTextColor(COLOR_BLACK, COLOR_WHITE);
    } else if (focused) {
        display_drawRect(display, btnX, btnY, btnW, btnH, COLOR_WHITE);
        display_drawRect(display, btnX+1, btnY+1, btnW-2, btnH-2, COLOR_WHITE); 
        display->setTextColor(COLOR_WHITE, COLOR_BLACK);
    } else {
        display_drawRect(display, btnX, btnY, btnW, btnH, COLOR_WHITE);
        display->setTextColor(COLOR_WHITE, COLOR_BLACK);
    }
    
    display->setCursor(tx, ty);
    display->print(displayText.c_str());
    
    if (needsTruncation && displayText.length() < strlen(text)) {
        int iconX = tx + displayText.length() * CHAR_WIDTH;
        int iconY = ty + (CHAR_HEIGHT - 8 * textScale) / 2;
        drawIconScaled(display, iconX, iconY, icon_ellipsis, focused ? (clicked ? COLOR_BLACK : COLOR_WHITE) : COLOR_WHITE);
    }
    return 0;
}

static int lua_gui_getButtonWidth(lua_State* L) {
    extern int CHAR_WIDTH;
    extern FlipperDisplay* display;
    const char* text = luaL_checkstring(L, 1);
    int x = luaL_optinteger(L, 2, 0); 
    int textW = strlen(text) * CHAR_WIDTH;
    int padding = 4 * getTextScale();
    int btnW = textW + padding * 2;
    if (display) {
        int displayW = display->width();
        int availableWidth = displayW - x;
        if (btnW > availableWidth) btnW = availableWidth;
    }
    lua_pushinteger(L, btnW);
    return 1;
}

static int lua_gui_getButtonBottomY(lua_State* L) {
    extern int CHAR_HEIGHT;
    int y = luaL_checkinteger(L, 1);
    int textH = CHAR_HEIGHT;
    int padding = 4 * getTextScale();
    int btnH = textH + padding * 2;
    int bottomY = y + btnH;
    lua_pushinteger(L, bottomY);
    return 1;
}

static int lua_gui_showLoadingScreen(lua_State* L) {
    showLoadingScreenOverlay();
    return 0;
}

static int lua_gui_hideLoadingScreen(lua_State* L) {
    hideLoadingScreenOverlay();
    return 0;
}

static int lua_gui_isLoadingScreenVisible(lua_State* L) {
    lua_pushboolean(L, isLoadingScreenVisible());
    return 1;
}

static void cleanupInteractiveApp() {
    if (appState && appState->interactiveApp && appState->interactiveApp->L) {
        for (auto& item : appState->interactiveApp->items) {
            if (item.callbackRef != LUA_REFNIL && item.callbackRef != -1) {
                luaL_unref(appState->interactiveApp->L, LUA_REGISTRYINDEX, item.callbackRef);
            }
        }
        delete appState->interactiveApp;
        appState->interactiveApp = nullptr;
    }
}

// ... createApp, appUpdate, etc. use appState->interactiveApp ...
// (Omitting full copy, logic is just s/interactiveApp/appState->interactiveApp/)

static int lua_gui_createApp(lua_State* L) {
    if (!appState) return 0;
    cleanupInteractiveApp();
    
    const char* title = luaL_checkstring(L, 1);
    appState->interactiveApp = new InteractiveApp();
    appState->interactiveApp->title = std::string(title);
    appState->interactiveApp->selectedIndex = 0;
    appState->interactiveApp->scrollY = 0;
    appState->interactiveApp->wantsExit = false;
    appState->interactiveApp->needsRender = true;
    appState->interactiveApp->L = L;
    
    luaL_checktype(L, 2, LUA_TTABLE);
    lua_pushnil(L);
    while (lua_next(L, 2) != 0) {
        luaL_checktype(L, -1, LUA_TTABLE);
        InteractiveAppItem item;
        
        lua_getfield(L, -1, "type");
        const char* typeStr = luaL_checkstring(L, -1);
        if (strcmp(typeStr, "input") == 0) item.type = InteractiveAppItem::ITEM_INPUT;
        else if (strcmp(typeStr, "button") == 0) item.type = InteractiveAppItem::ITEM_BUTTON;
        else luaL_error(L, "Invalid item type");
        lua_pop(L, 1);
        
        lua_getfield(L, -1, "label");
        const char* labelStr = lua_tostring(L, -1);
        item.label = std::string(labelStr ? labelStr : "");
        lua_pop(L, 1);
        
        if (item.type == InteractiveAppItem::ITEM_INPUT) {
            lua_getfield(L, -1, "value");
            const char* valueStr = lua_isstring(L, -1) ? lua_tostring(L, -1) : nullptr;
            item.value = new std::string(valueStr ? valueStr : "");
            lua_pop(L, 1);
        } else item.value = nullptr;
        
        lua_getfield(L, -1, "callback");
        if (lua_isfunction(L, -1)) item.callbackRef = luaL_ref(L, LUA_REGISTRYINDEX);
        else { item.callbackRef = LUA_REFNIL; lua_pop(L, 1); }
        
        lua_getfield(L, -1, "x"); item.x = luaL_optinteger(L, -1, 0); lua_pop(L, 1);
        lua_getfield(L, -1, "y"); item.y = luaL_optinteger(L, -1, 0); lua_pop(L, 1);
        lua_getfield(L, -1, "w"); item.w = luaL_optinteger(L, -1, 128); lua_pop(L, 1);
        lua_getfield(L, -1, "minInputWidth"); item.minInputWidth = luaL_optinteger(L, -1, 40 * getTextScale()); lua_pop(L, 1);
        
        appState->interactiveApp->items.push_back(item);
        lua_pop(L, 1);
    }
    
    lua_newtable(L);
    lua_pushstring(L, "update"); lua_pushcfunction(L, lua_gui_appUpdate); lua_settable(L, -3);
    lua_pushstring(L, "exit"); lua_pushcfunction(L, lua_gui_appExit); lua_settable(L, -3);
    lua_pushstring(L, "getInputValue"); lua_pushcfunction(L, lua_gui_appGetInputValue); lua_settable(L, -3);
    return 1;
}

static int lua_gui_appGetInputValue(lua_State* L) {
    if (!appState || !appState->interactiveApp) { luaL_error(L, "No active interactive app"); return 0; }
    if (!L || L != appState->interactiveApp->L) return 0; // Ensure Lua state is valid
    int index = luaL_checkinteger(L, 1) - 1;
    if (index < 0 || index >= (int)appState->interactiveApp->items.size()) { luaL_error(L, "Invalid index"); return 0; }
    if (appState->interactiveApp->items[index].type != InteractiveAppItem::ITEM_INPUT) { luaL_error(L, "Not input"); return 0; }
    if (!appState->interactiveApp->items[index].value) { lua_pushstring(L, ""); return 1; }
    lua_pushstring(L, appState->interactiveApp->items[index].value->c_str());
    return 1;
}

static int lua_gui_appUpdate(lua_State* L) {
    if (!appState || !appState->interactiveApp) return 0;
    if (!L || L != appState->interactiveApp->L) return 0; // Ensure Lua state is valid
    
    extern FlipperDisplay* display;
    if (!display) return 0;
    updateControls();
    
    // Check again after updateControls() - app might have been deleted
    if (!appState || !appState->interactiveApp) return 0;
    
    bool inputChanged = false;
    InteractiveApp* ia = appState->interactiveApp;
    
    // Safety check: ensure interactive app still exists
    if (!ia) return 0;
    
    if (isUpPressed() && ia->items.size() > 0) {
        // Check again before accessing
        if (!appState || !appState->interactiveApp) return 0;
        ia = appState->interactiveApp;
        ia->selectedIndex = (ia->selectedIndex > 0) ? ia->selectedIndex - 1 : ia->items.size() - 1;
        inputChanged = true;
    }
    if (isDownPressed() && ia->items.size() > 0) {
        // Check again before accessing
        if (!appState || !appState->interactiveApp) return 0;
        ia = appState->interactiveApp;
        ia->selectedIndex = (ia->selectedIndex < (int)ia->items.size() - 1) ? ia->selectedIndex + 1 : 0;
        inputChanged = true;
    }
    
    if (isButtonReleased() && ia->items.size() > 0) {
        // Check if interactive app still exists before processing button
        if (!appState || !appState->interactiveApp) return 0;
        // Refresh pointer in case it changed
        ia = appState->interactiveApp;
        
        if (ia->selectedIndex >= 0 && ia->selectedIndex < (int)ia->items.size()) {
            InteractiveAppItem& item = ia->items[ia->selectedIndex];
            if (item.type == InteractiveAppItem::ITEM_INPUT) {
                char buffer[128];
                strncpy(buffer, item.value->c_str(), sizeof(buffer)-1); buffer[sizeof(buffer)-1]=0;
                if (showKeyboard("", buffer, sizeof(buffer))) {
                    *item.value = std::string(buffer);
                    inputChanged = true;
                }
            } else {
                if (item.callbackRef != LUA_REFNIL && item.callbackRef != -1) {
                    // Check again before calling callback
                    if (!appState || !appState->interactiveApp) return 0;
                    lua_rawgeti(L, LUA_REGISTRYINDEX, item.callbackRef);
                    if (lua_pcall(L, 0, 0, 0) != LUA_OK) lua_pop(L, 1);
                    // Check after callback - it may have called exit()
                    if (!appState || !appState->interactiveApp) return 0;
                }
            }
        }
    }
    
    // Refresh pointer before rendering - app might have been deleted during input processing
    if (!appState || !appState->interactiveApp) return 0;
    ia = appState->interactiveApp;
    
    // ... Rendering logic identical to before, just using 'ia' pointer ...
    const int BASE_CHAR_HEIGHT = 8;
    int textScale = getTextScale();
    int headerHeight = (BASE_CHAR_HEIGHT + 4) * textScale;
    int visibleHeight = display->height() - headerHeight;
    int startY = headerHeight;
    int spacing = 2 * textScale;
    
    // Safety check: ensure selectedIndex is valid
    if (ia->items.size() == 0) {
        return 0; // No items to render
    }
    if (ia->selectedIndex < 0) ia->selectedIndex = 0;
    if (ia->selectedIndex >= (int)ia->items.size()) ia->selectedIndex = ia->items.size() - 1;
    
    std::vector<int> itemTops, itemBottoms;
    int currentY = startY;
    
    for (const auto& item : ia->items) {
        itemTops.push_back(currentY);
        int bottomY;
        if (item.type == InteractiveAppItem::ITEM_INPUT) {
            calculateInputDimensions(item.x, currentY, item.w, item.value->c_str(), item.label.c_str(), item.minInputWidth, false, NULL, NULL, NULL, NULL, NULL, &bottomY);
        } else {
            bottomY = currentY + CHAR_HEIGHT + 4 * getTextScale() * 2;
        }
        itemBottoms.push_back(bottomY);
        currentY = bottomY + spacing;
    }
    
    // Safety check: ensure vectors are not empty
    if (itemTops.empty() || itemBottoms.empty()) {
        return 0;
    }
    
    int selectedTop = itemTops[ia->selectedIndex];
    int selectedBottom = itemBottoms[ia->selectedIndex];
    int oldScrollY = ia->scrollY;
    
    if (selectedTop - ia->scrollY < startY) ia->scrollY = selectedTop - startY;
    else if (selectedBottom - ia->scrollY > startY + visibleHeight) ia->scrollY = selectedBottom - (startY + visibleHeight);
    
    if (ia->scrollY < 0) ia->scrollY = 0;
    int totalH = itemBottoms.back() - startY;
    if (totalH <= visibleHeight) ia->scrollY = 0;
    else if (ia->scrollY > totalH - visibleHeight) ia->scrollY = totalH - visibleHeight;
    
    if (ia->scrollY != oldScrollY) inputChanged = true;
    
    if (!ia->needsRender && !inputChanged) return 0;
    ia->needsRender = false;
    
    acquireDisplayLock();
    if (isLoadingScreenVisible()) {
        releaseDisplayLock();
        // Don't refresh, just return as loading screen is blocking
        return 0;
    }
    
    display->clearDisplay();
    // Safety check: ensure vectors match items size
    if (itemTops.size() != ia->items.size() || itemBottoms.size() != ia->items.size()) {
        releaseDisplayLock();
        return 0; // Skip rendering if mismatch
    }
    
    for (size_t i = 0; i < ia->items.size(); i++) {
        const auto& item = ia->items[i];
        int itemTop = itemTops[i] - ia->scrollY;
        int itemBottom = itemBottoms[i] - ia->scrollY;
        
        if (itemBottom > 0 && itemTop < display->height()) {
            bool isFocused = (int)i == ia->selectedIndex;
            if (item.type == InteractiveAppItem::ITEM_INPUT) {
                // Safety check: ensure value pointer is valid
                const char* valueStr = (item.value && item.value->length() > 0) ? item.value->c_str() : "";
                lua_pushinteger(L, item.x); lua_pushinteger(L, itemTop); lua_pushinteger(L, item.w);
                lua_pushstring(L, valueStr); lua_pushboolean(L, isFocused); lua_pushstring(L, item.label.c_str());
                lua_pushinteger(L, item.minInputWidth); lua_gui_drawInput(L); lua_settop(L, 0);
            } else {
                lua_pushinteger(L, item.x); lua_pushinteger(L, itemTop); lua_pushstring(L, item.label.c_str());
                lua_pushboolean(L, isFocused); lua_pushboolean(L, false); lua_gui_drawButton(L); lua_settop(L, 0);
            }
        }
    }
    
    display->fillRect(0, 0, display->width(), headerHeight, COLOR_WHITE);
    display->setTextColor(COLOR_BLACK, COLOR_WHITE);
    display->setCursor(2 * textScale, (headerHeight - BASE_CHAR_HEIGHT * textScale) / 2);
    display->print(ia->title.c_str());
    display->setTextColor(COLOR_WHITE, COLOR_BLACK);
    
    releaseDisplayLock();
    requestDisplayRefresh();
    return 0;
}

static int lua_gui_appExit(lua_State* L) {
    cleanupInteractiveApp();
    return 0;
}

static int luaopen_gui(lua_State* L) {
    static const luaL_Reg guiLib[] = {
        {"keyboard", lua_gui_keyboard},
        {"drawInput", lua_gui_drawInput},
        {"getInputYBelow", lua_gui_getInputYBelow},
        {"getInputRightX", lua_gui_getInputRightX},
        {"getInputBottomY", lua_gui_getInputBottomY},
        {"getInputLeftX", lua_gui_getInputLeftX},
        {"getInputTopY", lua_gui_getInputTopY},
        {"drawButton", lua_gui_drawButton},
        {"getButtonWidth", lua_gui_getButtonWidth},
        {"getButtonBottomY", lua_gui_getButtonBottomY},
        {"createApp", lua_gui_createApp},
        {"appUpdate", lua_gui_appUpdate},
        {"appExit", lua_gui_appExit},
        {"showLoadingScreen", lua_gui_showLoadingScreen},
        {"hideLoadingScreen", lua_gui_hideLoadingScreen},
        {"isLoadingScreenVisible", lua_gui_isLoadingScreenVisible},
        {NULL, NULL}
    };
    luaL_newlib(L, guiLib);
    return 1;
}

// ... IR Module ...
#if ENABLE_ADVANCED_IR
// ... (Keeping IR module as is, it doesn't use complex globals) ...
static int lua_ir_send(lua_State* L) {
    const char* protocol = luaL_checkstring(L, 1);
    uint32_t address = luaL_checkinteger(L, 2);
    uint32_t command = luaL_checkinteger(L, 3);
    int nbits = luaL_optinteger(L, 4, 32);
    int repeat = luaL_optinteger(L, 5, 0);
    
    char protocolBuf[128];
    strncpy(protocolBuf, protocol, sizeof(protocolBuf)-1); protocolBuf[sizeof(protocolBuf)-1]=0;
    
    decode_type_t protoType = protocolNameToType(protocolBuf);
    if (protoType == UNKNOWN) { lua_pushboolean(L, false); return 1; }
    
    uint64_t data;
    if (protoType == NEC) data = ((uint64_t)address << 16) | (command & 0xFF);
    else if (protoType == PANASONIC) data = ((uint64_t)address << 32) | command;
    else if (protoType == SHARP) data = ((uint64_t)address << 8) | (command & 0xFF);
    else if (protoType == RC5 || protoType == RC6) data = ((uint64_t)address << 6) | (command & 0x3F);
    else data = command;
    
    lua_pushboolean(L, sendIR(protocolBuf, data, nbits, repeat));
    return 1;
}

static int lua_ir_sendRaw(lua_State* L) {
    int frequency = luaL_checkinteger(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    int len = lua_rawlen(L, 2);
    if (len > 0) {
        uint16_t* rawData = new uint16_t[len];
        for (int i = 0; i < len; i++) {
            lua_rawgeti(L, 2, i + 1);
            rawData[i] = luaL_checkinteger(L, -1);
            lua_pop(L, 1);
        }
        sendRaw(rawData, len, frequency);
        delete[] rawData;
    }
    return 0;
}

static int lua_ir_scan(lua_State* L) {
    int timeoutMs = luaL_checkinteger(L, 1);
    TVCode code;
    char protocolName[32];
    if (scanIRCode(timeoutMs, &code, protocolName, sizeof(protocolName))) {
        lua_newtable(L);
        lua_pushstring(L, "protocol"); lua_pushstring(L, protocolName); lua_settable(L, -3);
        lua_pushstring(L, "address"); lua_pushinteger(L, code.address); lua_settable(L, -3);
        lua_pushstring(L, "command"); lua_pushinteger(L, code.command); lua_settable(L, -3);
        lua_pushstring(L, "nbits"); lua_pushinteger(L, code.nbits); lua_settable(L, -3);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

static int lua_ir_getProtocols(lua_State* L) {
    const char** protocols = getIRProtocols();
    lua_newtable(L);
    int index = 1;
    while (*protocols) {
        lua_pushinteger(L, index++);
        lua_pushstring(L, *protocols++);
        lua_settable(L, -3);
    }
    return 1;
}

static int lua_ir_getCodeCount(lua_State* L) {
    lua_pushinteger(L, getTVCodeCount());
    return 1;
}

static int lua_ir_getCode(lua_State* L) {
    int index = luaL_checkinteger(L, 1);
    const TVCode* allCodes = getAllTVCodes();
    if (index >= 0 && index < getTVCodeCount()) {
        const TVCode& code = allCodes[index];
        lua_newtable(L);
        lua_pushstring(L, "protocol"); lua_pushstring(L, code.protocol); lua_settable(L, -3);
        lua_pushstring(L, "brand"); lua_pushstring(L, code.brand); lua_settable(L, -3);
        lua_pushstring(L, "button"); lua_pushstring(L, code.button); lua_settable(L, -3);
        lua_pushstring(L, "address"); lua_pushinteger(L, code.address); lua_settable(L, -3);
        lua_pushstring(L, "command"); lua_pushinteger(L, code.command); lua_settable(L, -3);
        lua_pushstring(L, "nbits"); lua_pushinteger(L, code.nbits); lua_settable(L, -3);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

static int lua_ir_sendPulse(lua_State* L) {
    int onTime = luaL_checkinteger(L, 1);
    int offTime = luaL_checkinteger(L, 2);
    int frequency = luaL_optinteger(L, 3, 38000);
    sendIRPulse(onTime, offTime, frequency);
    return 0;
}

static int luaopen_ir(lua_State* L) {
    static const luaL_Reg irLib[] = {
        {"send", lua_ir_send},
        {"sendRaw", lua_ir_sendRaw},
        {"scan", lua_ir_scan},
        {"getProtocols", lua_ir_getProtocols},
        {"getCodeCount", lua_ir_getCodeCount},
        {"getCode", lua_ir_getCode},
        {"sendPulse", lua_ir_sendPulse},
        {NULL, NULL}
    };
    luaL_newlib(L, irLib);
    return 1;
}
#endif

// --- Main Lua Entry Points ---

void initLua() {
    if (appState) {
        // Ensure loading screen is freed before deleting app state
        freeLoadingScreen();
        delete appState;
        appState = nullptr;
    }
    Serial.println(F("Lua system reset"));
}

void setLuaScript(const char* script) {
    ensureAppState();
    appState->currentScriptContent = std::string(script);
    appState->scriptLoaded = false;
    appState->luaWantsExit = false;
}

void setLuaScriptFromFile(const char* path) {
    ensureAppState();
    appState->currentScriptContent = std::string(loadLuaScript(path).c_str());
    appState->scriptLoaded = false;
    appState->luaWantsExit = false;
}

AppState luaApp() {
    if (!appState) {
        Serial.println(F("No app state!"));
        return AppState::EXIT;
    }
    
    if (!appState->luaInstance) {
        Serial.println(F("Creating Lua instance..."));
        appState->luaInstance = new Lua();
        if (!appState->luaInstance) {
            Serial.println(F("Failed to create Lua instance!"));
            return AppState::EXIT;
        }
        
        Lua* L = appState->luaInstance;
        L->addModule("string", luaopen_string);
        L->addModule("utf8", luaopen_utf8);
        L->addModule("math", luaopen_math);
        L->addModule("table", luaopen_table);
        L->addModule("os", luaopen_os);
        
        L->addModule("display", luaopen_display);
        L->addModule("input", luaopen_input);
        L->addModule("app", luaopen_app);
        L->addModule("gpio", luaopen_gpio);
        L->addModule("gui", luaopen_gui);
        L->addModule("statusled", luaopen_statusled);
        L->addModule("pwm", luaopen_pwm);
        L->addModule("eeprom", luaopen_eeprom);
        L->addModule("filesystem", luaopen_filesystem);
        
        #if ENABLE_BLE
        L->addModule("ble", luaopen_ble);
        #endif
        
        #if ENABLE_ADVANCED_WIFI
        L->addModule("wifi", luaopen_wifi);
        #endif
        
        #if ENABLE_ADVANCED_IR
        L->addModule("ir", luaopen_ir);
        #endif
        
        Serial.println(F("Lua instance created"));
    }
    
    if (appState->currentScriptContent.length() == 0) {
        Serial.println(F("No Lua script set"));
        return AppState::EXIT;
    }
    
    if (!appState->scriptLoaded) {
        setLEDBusy();  
        Serial.println(F("Loading Lua script..."));
        
        appState->luaWantsExit = false;
        
        // Force garbage collection before running
        ESP.getFreeHeap(); 
        
        appState->luaInstance->run(appState->currentScriptContent.c_str(), &Serial);
        appState->scriptLoaded = true;
        Serial.println(F("Script loaded, calling setup()..."));
        
        std::vector<std::string> emptyArgs;
        appState->luaInstance->call("setup", emptyArgs, &Serial);
        Serial.println(F("setup() done"));
        setLEDReady();  
    }
    
    if (!appState->interactiveApp) {
        updateControls();
    }
    
    std::vector<std::string> emptyArgs;
    appState->luaInstance->call("loop", emptyArgs, nullptr);
    
    if (appState->luaWantsExit) {
        Serial.println(F("Lua requested exit"));
        
        // Ensure loading screen is freed
        freeLoadingScreen();
        
        // Force Lua garbage collection before cleanup if we have access to lua_State
        if (appState->interactiveApp && appState->interactiveApp->L) {
            lua_gc(appState->interactiveApp->L, LUA_GCCOLLECT, 0);
        }
        
        // Clear script content to free memory before deletion
        appState->currentScriptContent.clear();
        appState->currentScriptContent.shrink_to_fit();
        
        // DESTROY EVERYTHING associated with this app execution
        Serial.println(F("Cleaning up app state..."));
        delete appState;
        appState = nullptr;
        
        Serial.print(F("Free heap after cleanup: "));
        Serial.println(ESP.getFreeHeap());
        
        return AppState::EXIT;
    }
    
    return AppState::RUNNING;
}
