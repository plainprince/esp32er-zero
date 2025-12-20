#include "lua_app.h"
#include "utils.h"
#include "controls.h"
#include "lua_fs.h"
#include "keyboard.h"
#include "config.h"
#include "eeprom.h"
#include <FlipperDisplay.h>
#include <Lua.h>
#include <WiFi.h>
#include <Arduino.h>


extern "C" {
#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"
}


static Lua* luaInstance = nullptr;
static String currentScriptContent;
static volatile bool luaWantsExit = false;
static bool scriptLoaded = false;


static volatile int rawJoystickX = 2048;
static volatile int rawJoystickY = 2048;
static volatile bool rawButtonState = false;


#define JOY_X_PIN 34
#define JOY_Y_PIN 35
#define JOY_BTN_PIN 32


static void display_drawRect(FlipperDisplay* display, int x, int y, int w, int h, int color) {
    if (!display) return;
    display->fillRect(x, y, w, 1, color);         
    display->fillRect(x, y + h - 1, w, 1, color); 
    display->fillRect(x, y, 1, h, color);         
    display->fillRect(x + w - 1, y, 1, h, color); 
}





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





static int lua_app_exit(lua_State* L) {
    luaWantsExit = true;
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
    
    
    
    
    
    int usedPins[] = {
        2,   
        32,  
        34,  
        35,  
    };
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
    for (int i = 0; i < usedPinsCount; i++) {
        if (usedPins[i] < 40) pinUsed[usedPins[i]] = true;
    }
    for (int i = 0; i < displayPinsCount; i++) {
        if (displayPins[i] < 40) pinUsed[displayPins[i]] = true;
    }
    
    
    
    int validPins[] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
        21, 22, 23,
        25, 26, 27,
        32, 33, 34, 35, 36, 37, 38, 39
    };
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

static int lua_wifi_scanGetCount(lua_State* L) {
    int n = WiFi.scanComplete();
    if (n < 0) n = 0;
    lua_pushinteger(L, n);
    return 1;
}

static int lua_wifi_scanGetSSID(lua_State* L) {
    int index = luaL_checkinteger(L, 1);
    String ssid = WiFi.SSID(index);
    if (ssid.length() == 0) {
        ssid = "(Hidden)";
    }
    lua_pushstring(L, ssid.c_str());
    return 1;
}

static int lua_wifi_scanGetRSSI(lua_State* L) {
    int index = luaL_checkinteger(L, 1);
    int32_t rssi = WiFi.RSSI(index);
    lua_pushinteger(L, rssi);
    return 1;
}

static int lua_wifi_scanGetEncryption(lua_State* L) {
    int index = luaL_checkinteger(L, 1);
    wifi_auth_mode_t encType = WiFi.encryptionType(index);
    lua_pushinteger(L, (int)encType);
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
    if (connected) {
        lua_pushstring(L, WiFi.localIP().toString().c_str());
    } else {
        lua_pushstring(L, "");
    }
    return 2;
}

static int lua_wifi_status(lua_State* L) {
    wl_status_t status = WiFi.status();
    lua_pushinteger(L, (int)status);
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
    if (eepromReadString(key, buffer, sizeof(buffer))) {
        lua_pushstring(L, buffer);
    } else {
        lua_pushstring(L, defaultValue);
    }
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
    bool exists = eepromKeyExists(key);
    lua_pushboolean(L, exists);
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
    
    
    String textStr = text;
    int lineCount = 1;
    for (int i = 0; i < textStr.length(); i++) {
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
        display->setTextColor(COLOR_WHITE, COLOR_BLACK);
        
        int labelY = y + (inputHeight - CHAR_HEIGHT) / 2;
        display->setCursor(x, labelY);
        display->print(label);
        inputX = x + labelWidth;
    }
    
    
    int inputWidth = w - labelWidth;
    if (inputWidth < minInputWidth) {
        inputWidth = minInputWidth;
    }
    
    
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
    int startIdx = 0;
    
    for (int line = 0; line < lineCount; line++) {
        int endIdx = textStr.indexOf('\n', startIdx);
        if (endIdx == -1) endIdx = textStr.length();
        
        String lineText = textStr.substring(startIdx, endIdx);
        
        
        if (lineText.length() > maxChars) {
            lineText = lineText.substring(lineText.length() - maxChars);
        }
        
        display->setCursor(inputX + padding, currentY);
        display->print(lineText.c_str());
        
        currentY += lineHeight;
        startIdx = endIdx + 1;
    }
    
    return 0;
}

static int lua_gui_getInputYBelow(lua_State* L) {
    extern int CHAR_WIDTH;
    extern int CHAR_HEIGHT;
    
    int y = luaL_checkinteger(L, 1);
    const char* text = luaL_checkstring(L, 2);
    const char* label = luaL_optstring(L, 3, NULL);
    
    int padding = 4 * getTextScale();
    int lineHeight = CHAR_HEIGHT;
    
    
    String textStr = text;
    int lineCount = 1;
    for (int i = 0; i < textStr.length(); i++) {
        if (textStr[i] == '\n') {
            lineCount++;
        }
    }
    
    
    int inputHeight = lineHeight * lineCount + padding * 2;
    
    
    
    
    lua_pushinteger(L, y + inputHeight);
    return 1;
}


static void calculateInputDimensions(int x, int y, int w, const char* text, const char* label, int minInputWidth, bool focused,
                                     int* outInputX, int* outInputY, int* outInputWidth, int* outInputHeight, int* outRightX, int* outBottomY) {
    extern int CHAR_WIDTH;
    extern int CHAR_HEIGHT;
    
    int padding = 4 * getTextScale();
    int lineHeight = CHAR_HEIGHT;
    int labelSpacing = 4 * getTextScale();
    
    
    String textStr = text;
    int lineCount = 1;
    for (int i = 0; i < textStr.length(); i++) {
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
    int remainingWidth = availableWidth - textW; 
    
    bool needsTruncation = false;
    String displayText = text;
    
    if (remainingWidth <= 128 * textScale) {
        
        int iconW = 8 * textScale; 
        int maxTextW = availableWidth - padding * 2 - iconW;
        int maxChars = maxTextW / CHAR_WIDTH;
        
        if (maxChars > 0) {
            displayText = String(text).substring(0, maxChars);
            needsTruncation = true;
            btnW = strlen(displayText.c_str()) * CHAR_WIDTH + padding * 2 + iconW;
        } else {
            
            displayText = "";
            needsTruncation = true;
            btnW = padding * 2 + iconW;
        }
    } else if (btnW > availableWidth) {
        
        int maxTextW = availableWidth - padding * 2;
        int maxChars = maxTextW / CHAR_WIDTH;
        if (maxChars > 0) {
            displayText = String(text).substring(0, maxChars);
        } else {
            displayText = "";
        }
        btnW = availableWidth;
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
        int iconX = tx + strlen(displayText.c_str()) * CHAR_WIDTH;
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
        int textScale = getTextScale();
        int availableWidth = displayW - x;
        int remainingWidth = availableWidth - textW;
        
        if (remainingWidth <= 128 * textScale) {
            
            int iconW = 8 * textScale;
            int maxTextW = availableWidth - padding * 2 - iconW;
            int maxChars = maxTextW / CHAR_WIDTH;
            if (maxChars > 0) {
                btnW = maxChars * CHAR_WIDTH + padding * 2 + iconW;
            } else {
                btnW = padding * 2 + iconW;
            }
        } else if (btnW > availableWidth) {
            btnW = availableWidth;
        }
    }
    
    lua_pushinteger(L, btnW);
    return 1;
}

static int lua_gui_getButtonBottomY(lua_State* L) {
    extern int CHAR_WIDTH;
    extern int CHAR_HEIGHT;
    extern FlipperDisplay* display;
    
    int y = luaL_checkinteger(L, 1);
    const char* text = luaL_checkstring(L, 2);
    bool focused = (lua_gettop(L) >= 3 && !lua_isnoneornil(L, 3)) ? lua_toboolean(L, 3) : false;
    int x = luaL_optinteger(L, 4, 0); 
    
    int textH = CHAR_HEIGHT;
    int padding = 4 * getTextScale();
    int btnH = textH + padding * 2;
    
    
    
    int borderThickness = focused ? 2 : 1;
    
    
    
    
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





struct InteractiveAppItem {
    enum ItemType { ITEM_INPUT, ITEM_BUTTON };
    ItemType type;
    String label;      
    String* value;     
    int callbackRef;   
    int x, y, w;       
    int minInputWidth; 
};

struct InteractiveApp {
    String title;
    std::vector<InteractiveAppItem> items;
    int selectedIndex;
    int scrollY;
    bool wantsExit;
    bool needsRender;  
    lua_State* L;
};

static InteractiveApp* interactiveApp = nullptr;


static int lua_gui_appUpdate(lua_State* L);
static int lua_gui_appExit(lua_State* L);
static int lua_gui_appGetInputValue(lua_State* L);

static void cleanupInteractiveApp() {
    if (interactiveApp && interactiveApp->L) {
        
        for (auto& item : interactiveApp->items) {
            if (item.callbackRef != LUA_REFNIL && item.callbackRef != -1) {
                luaL_unref(interactiveApp->L, LUA_REGISTRYINDEX, item.callbackRef);
            }
        }
        
        for (auto& item : interactiveApp->items) {
            if (item.type == InteractiveAppItem::ITEM_INPUT && item.value) {
                delete item.value;
            }
        }
        delete interactiveApp;
        interactiveApp = nullptr;
    }
}

static int lua_gui_createApp(lua_State* L) {
    
    cleanupInteractiveApp();
    
    
    const char* title = luaL_checkstring(L, 1);
    
    
    interactiveApp = new InteractiveApp();
    interactiveApp->title = String(title);
    interactiveApp->selectedIndex = 0;
    interactiveApp->scrollY = 0;
    interactiveApp->wantsExit = false;
    interactiveApp->needsRender = true;  
    interactiveApp->L = L;
    
    
    luaL_checktype(L, 2, LUA_TTABLE);
    
    
    lua_pushnil(L);  
    while (lua_next(L, 2) != 0) {
        
        luaL_checktype(L, -1, LUA_TTABLE);
        
        InteractiveAppItem item;
        
        
        lua_getfield(L, -1, "type");
        const char* typeStr = luaL_checkstring(L, -1);
        if (strcmp(typeStr, "input") == 0) {
            item.type = InteractiveAppItem::ITEM_INPUT;
        } else if (strcmp(typeStr, "button") == 0) {
            item.type = InteractiveAppItem::ITEM_BUTTON;
        } else {
            luaL_error(L, "Invalid item type: %s (must be 'input' or 'button')", typeStr);
        }
        lua_pop(L, 1);
        
        
        lua_getfield(L, -1, "label");
        if (lua_isstring(L, -1)) {
            item.label = String(lua_tostring(L, -1));
        } else {
            luaL_error(L, "Item must have a 'label' field");
        }
        lua_pop(L, 1);
        
        
        if (item.type == InteractiveAppItem::ITEM_INPUT) {
            lua_getfield(L, -1, "value");
            if (lua_isstring(L, -1)) {
                item.value = new String(lua_tostring(L, -1));
            } else {
                item.value = new String("");
            }
            lua_pop(L, 1);
        } else {
            item.value = nullptr;
        }
        
        
        lua_getfield(L, -1, "callback");
        if (lua_isfunction(L, -1)) {
            item.callbackRef = luaL_ref(L, LUA_REGISTRYINDEX);
        } else {
            item.callbackRef = LUA_REFNIL;
            lua_pop(L, 1);
        }
        
        
        lua_getfield(L, -1, "x");
        item.x = luaL_optinteger(L, -1, 0);
        lua_pop(L, 1);
        
        lua_getfield(L, -1, "y");
        item.y = luaL_optinteger(L, -1, 0);  
        lua_pop(L, 1);
        
        lua_getfield(L, -1, "w");
        item.w = luaL_optinteger(L, -1, 128);
        lua_pop(L, 1);
        
        lua_getfield(L, -1, "minInputWidth");
        item.minInputWidth = luaL_optinteger(L, -1, 40 * getTextScale());
        lua_pop(L, 1);
        
        interactiveApp->items.push_back(item);
        
        lua_pop(L, 1);  
    }
    
    
    lua_newtable(L);
    
    
    lua_pushstring(L, "update");
    lua_pushcfunction(L, lua_gui_appUpdate);
    lua_settable(L, -3);
    
    
    lua_pushstring(L, "exit");
    lua_pushcfunction(L, lua_gui_appExit);
    lua_settable(L, -3);
    
    
    lua_pushstring(L, "getInputValue");
    lua_pushcfunction(L, lua_gui_appGetInputValue);
    lua_settable(L, -3);
    
    return 1;
}

static int lua_gui_appGetInputValue(lua_State* L) {
    if (!interactiveApp) {
        luaL_error(L, "No active interactive app");
        return 0;
    }
    
    int index = luaL_checkinteger(L, 1);
    index--;  
    
    if (index < 0 || index >= (int)interactiveApp->items.size()) {
        luaL_error(L, "Invalid item index: %d", index + 1);
        return 0;
    }
    
    InteractiveAppItem& item = interactiveApp->items[index];
    if (item.type != InteractiveAppItem::ITEM_INPUT) {
        luaL_error(L, "Item at index %d is not an input field", index + 1);
        return 0;
    }
    
    lua_pushstring(L, item.value->c_str());
    return 1;
}

static int lua_gui_appUpdate(lua_State* L) {
    if (!interactiveApp) {
        
        return 0;
    }
    
    extern FlipperDisplay* display;
    extern int CHAR_WIDTH;
    extern int CHAR_HEIGHT;
    if (!display) return 0;
    
    
    updateControls();
    
    
    bool inputChanged = false;
    
    if (isUpPressed()) {
        if (interactiveApp->selectedIndex > 0) {
            interactiveApp->selectedIndex--;
            inputChanged = true;
        } else {
            
            interactiveApp->selectedIndex = interactiveApp->items.size() - 1;
            inputChanged = true;
        }
    }
    
    if (isDownPressed()) {
        if (interactiveApp->selectedIndex < (int)interactiveApp->items.size() - 1) {
            interactiveApp->selectedIndex++;
            inputChanged = true;
        } else {
            
            interactiveApp->selectedIndex = 0;
            inputChanged = true;
        }
    }
    
    
    if (isButtonReleased()) {
        InteractiveAppItem& item = interactiveApp->items[interactiveApp->selectedIndex];
        
        if (item.type == InteractiveAppItem::ITEM_INPUT) {
            
            char buffer[128];
            strncpy(buffer, item.value->c_str(), sizeof(buffer) - 1);
            buffer[sizeof(buffer) - 1] = 0;
            
            if (showKeyboard("", buffer, sizeof(buffer))) {
                *item.value = String(buffer);
                inputChanged = true;
            }
        } else {
            
            if (item.callbackRef != LUA_REFNIL && item.callbackRef != -1) {
                lua_rawgeti(L, LUA_REGISTRYINDEX, item.callbackRef);
                if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                    
                    lua_pop(L, 1);
                }
                
                
                if (!interactiveApp) {
                    return 0;
                }
            }
        }
    }
    
    
    
    const int BASE_CHAR_HEIGHT = 8;
    int textScale = getTextScale();
    int displayH = display->height();
    int displayW = display->width();
    int headerHeight = (BASE_CHAR_HEIGHT + 4) * textScale;
    int visibleHeight = displayH - headerHeight;
    int startY = headerHeight;
    int spacing = 2 * textScale;
    
    
    std::vector<int> itemTops;
    std::vector<int> itemBottoms;
    int currentY = startY;
    
    for (size_t i = 0; i < interactiveApp->items.size(); i++) {
        const InteractiveAppItem& item = interactiveApp->items[i];
        itemTops.push_back(currentY);
        
        if (item.type == InteractiveAppItem::ITEM_INPUT) {
            int bottomY;
            calculateInputDimensions(item.x, currentY, item.w, item.value->c_str(), item.label.c_str(), item.minInputWidth, false, NULL, NULL, NULL, NULL, NULL, &bottomY);
            itemBottoms.push_back(bottomY);
            currentY = bottomY + spacing;
        } else {
            
            int textH = CHAR_HEIGHT;
            int padding = 4 * getTextScale();
            int btnH = textH + padding * 2;
            int bottomY = currentY + btnH;
            itemBottoms.push_back(bottomY);
            currentY = bottomY + spacing;
        }
    }
    
    
    int selectedTop = itemTops[interactiveApp->selectedIndex];
    int selectedBottom = itemBottoms[interactiveApp->selectedIndex];
    int oldScrollY = interactiveApp->scrollY;
    
    if (selectedTop - interactiveApp->scrollY < startY) {
        interactiveApp->scrollY = selectedTop - startY;
    } else if (selectedBottom - interactiveApp->scrollY > startY + visibleHeight) {
        interactiveApp->scrollY = selectedBottom - (startY + visibleHeight);
    }
    
    if (interactiveApp->scrollY < 0) interactiveApp->scrollY = 0;
    
    int totalContentHeight = itemBottoms.back() - startY;
    if (totalContentHeight <= visibleHeight) {
        interactiveApp->scrollY = 0;
    } else {
        int maxScroll = totalContentHeight - visibleHeight;
        if (interactiveApp->scrollY > maxScroll) interactiveApp->scrollY = maxScroll;
    }
    
    
    if (interactiveApp->scrollY != oldScrollY) {
        inputChanged = true;
    }
    
    
    if (!interactiveApp->needsRender && !inputChanged) {
        return 0;  
    }
    
    interactiveApp->needsRender = false;  
    
    acquireDisplayLock();
    
    
    if (isLoadingScreenVisible()) {
        display->clearDisplay();
        showLoadingScreenOverlay();
        releaseDisplayLock();
        requestDisplayRefresh();
        return 0;
    }
    
    display->clearDisplay();
    
    
    for (size_t i = 0; i < interactiveApp->items.size(); i++) {
        const InteractiveAppItem& item = interactiveApp->items[i];
        int itemTop = itemTops[i] - interactiveApp->scrollY;
        int itemBottom = itemBottoms[i] - interactiveApp->scrollY;
        
        if (itemBottom > 0 && itemTop < displayH) {
            bool isFocused = (int)i == interactiveApp->selectedIndex;
            
            if (item.type == InteractiveAppItem::ITEM_INPUT) {
                
                lua_pushinteger(L, item.x);
                lua_pushinteger(L, itemTop);
                lua_pushinteger(L, item.w);
                lua_pushstring(L, item.value->c_str());
                lua_pushboolean(L, isFocused);
                lua_pushstring(L, item.label.c_str());
                lua_pushinteger(L, item.minInputWidth);
                lua_gui_drawInput(L);
                lua_settop(L, 0);  
            } else {
                
                lua_pushinteger(L, item.x);
                lua_pushinteger(L, itemTop);
                lua_pushstring(L, item.label.c_str());
                lua_pushboolean(L, isFocused);
                lua_pushboolean(L, false);
                lua_gui_drawButton(L);
                lua_settop(L, 0);  
            }
        }
    }
    
    
    display->fillRect(0, 0, displayW, headerHeight, COLOR_WHITE);
    display->setTextColor(COLOR_BLACK, COLOR_WHITE);
    int scaledTextHeight = BASE_CHAR_HEIGHT * textScale;
    int textY = (headerHeight - scaledTextHeight) / 2;
    int textX = 2 * textScale;
    display->setCursor(textX, textY);
    display->print(interactiveApp->title.c_str());
    display->setTextColor(COLOR_WHITE, COLOR_BLACK);
    
    
    
    if (isLoadingScreenVisible()) {
        showLoadingScreenOverlay();
    }
    
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





void initLua() {
    if (luaInstance) {
        delete luaInstance;
        luaInstance = nullptr;
    }
    Serial.println(F("Lua system initialized"));
}

void setLuaScript(const char* script) {
    currentScriptContent = String(script);
    scriptLoaded = false;
    luaWantsExit = false;
}

void setLuaScriptFromFile(const char* path) {
    currentScriptContent = loadLuaScript(path);
    scriptLoaded = false;
    luaWantsExit = false;
}

AppState luaApp() {
    
    if (!luaInstance) {
        Serial.println(F("Creating Lua instance..."));
        luaInstance = new Lua();
        if (!luaInstance) {
            Serial.println(F("Failed to create Lua instance!"));
            return AppState::EXIT;
        }
        
        
        luaInstance->addModule("string", luaopen_string);
        luaInstance->addModule("utf8", luaopen_utf8);
        
        
        luaInstance->addModule("display", luaopen_display);
        luaInstance->addModule("input", luaopen_input);
        luaInstance->addModule("app", luaopen_app);
        luaInstance->addModule("gpio", luaopen_gpio);
        luaInstance->addModule("gui", luaopen_gui);
        Serial.println(F("Lua instance created"));
    }
    
    if (currentScriptContent.length() == 0) {
        Serial.println(F("No Lua script set"));
        return AppState::EXIT;
    }
    
    
    if (!scriptLoaded) {
        setLEDBusy();  
        Serial.println(F("Loading Lua script..."));
        
        
        luaWantsExit = false;
        
        
        delete luaInstance;
        luaInstance = new Lua();
        
        
        luaInstance->addModule("string", luaopen_string);
        luaInstance->addModule("utf8", luaopen_utf8);
        
        
        luaInstance->addModule("display", luaopen_display);
        luaInstance->addModule("input", luaopen_input);
        luaInstance->addModule("app", luaopen_app);
        luaInstance->addModule("gpio", luaopen_gpio);
        luaInstance->addModule("gui", luaopen_gui);
        luaInstance->addModule("eeprom", luaopen_eeprom);
        luaInstance->addModule("wifi", luaopen_wifi);
        
        
        luaInstance->run(currentScriptContent.c_str(), &Serial);
        scriptLoaded = true;
        Serial.println(F("Script loaded, calling setup()..."));
        
        
        std::vector<std::string> emptyArgs;
        luaInstance->call("setup", emptyArgs, &Serial);
        Serial.println(F("setup() done"));
        setLEDReady();  
    }
    
    
    
    if (!interactiveApp) {
        updateControls();
    }
    
    
    std::vector<std::string> emptyArgs;
    luaInstance->call("loop", emptyArgs, nullptr);
    
    
    if (luaWantsExit) {
        Serial.println(F("Lua requested exit"));
        luaWantsExit = false;
        scriptLoaded = false;
        return AppState::EXIT;
    }
    
    return AppState::RUNNING;
}
