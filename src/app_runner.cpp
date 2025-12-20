#include "app_runner.h"
#include "utils.h"
#include "controls.h"
#include "menu.h"
#include <LittleFS.h>


AppRenderFunc currentApp = nullptr;
bool appIsRunning = false;


static const char* tvTitle = nullptr;
static const char* tvContent = nullptr;
static String tvContentBuffer; 
static int tvScrollOffset = 0;

void startApp(AppRenderFunc app) {
    currentApp = app;
    appIsRunning = true;
    tvScrollOffset = 0;
}

void stopApp() {
    currentApp = nullptr;
    appIsRunning = false;
    invalidateMenu();
}

bool isAppRunning() {
    return appIsRunning;
}

bool runAppFrame() {
    if (!appIsRunning || !currentApp) {
        return false;
    }
    
    AppState state = currentApp();
    
    if (state == AppState::EXIT) {
        stopApp();
        return false;
    }
    
    return true;
}





void setTextViewerContent(const char* title, const char* content) {
    tvTitle = title;
    tvContent = content;
    tvScrollOffset = 0;
}

void setTextViewerFile(const char* title, const char* filePath) {
    
    String cleanTitle = title;
    
    for (int i = cleanTitle.length() - 1; i >= 0; i--) {
        char c = cleanTitle[i];
        if (c < 32 || c > 126) {
            cleanTitle.remove(i, 1);
        }
    }
    static String titleBuffer;
    titleBuffer = cleanTitle;
    tvTitle = titleBuffer.c_str();
    
    tvScrollOffset = 0;
    
    
    File file = LittleFS.open(filePath, "r");
    if (file) {
        
        tvContentBuffer = "";
        while (file.available() && tvContentBuffer.length() < 4096) {
            char c = file.read();
            
            if (c >= 32 || c == '\n' || c == '\t' || c == '\r') {
                tvContentBuffer += c;
            }
        }
        if (file.available()) {
            tvContentBuffer += "\n...(truncated)";
        }
        file.close();
        tvContent = tvContentBuffer.c_str();
    } else {
        tvContentBuffer = "(File not found: " + String(filePath) + ")";
        tvContent = tvContentBuffer.c_str();
    }
    
}

AppState textViewerApp() {
    extern FlipperDisplay* display;
    if (!display) return AppState::EXIT;
    
    static bool initialized = false;
    static bool needsRedraw = true;
    static bool leftPressedLastFrame = false;
    
    
    if (!initialized) {
        initialized = true;
        needsRedraw = true;
        leftPressedLastFrame = false;
    }
    
    
    updateControls();
    
    
    bool leftPressed = isLeftPressed();
    bool leftJustPressed = leftPressed && !leftPressedLastFrame;
    leftPressedLastFrame = leftPressed;
    
    if (isButtonReleased() || leftJustPressed) {
        initialized = false;
        needsRedraw = false;
        
        if (leftJustPressed) {
            while (true) {
                updateControls();
                int xValue = analogRead(JOYSTICK_X_PIN);
                if (xValue >= JOYSTICK_LOW_THRESHOLD && xValue <= JOYSTICK_HIGH_THRESHOLD) {
                    break; 
                }
                delay(10);
            }
            delay(50); 
        }
        return AppState::EXIT;
    }
    
    
    bool scrolled = false;
    if (isUpPressed() && tvScrollOffset > 0) {
        tvScrollOffset--;
        scrolled = true;
    }
    if (isDownPressed()) {
        int visibleLines = (display->height() / CHAR_HEIGHT) - 1;
        
        int totalLines = 0;
        if (tvContent) {
            const char* ptr = tvContent;
            while (*ptr) {
                if (*ptr == '\n') totalLines++;
                ptr++;
            }
            if (ptr > tvContent && *(ptr-1) != '\n') totalLines++;
        }
        if (tvScrollOffset < totalLines - visibleLines) {
            tvScrollOffset++;
            scrolled = true;
        }
    }
    
    
    if (needsRedraw || scrolled) {
        needsRedraw = false;
        
        acquireDisplayLock();
        display->clearDisplay();
        resetCursor();
        
        
        if (tvTitle) {
            customPrintln(tvTitle, true);
        }
        
        
        if (tvContent) {
            const char* ptr = tvContent;
            int lineNum = 0;
            int visibleLines = (display->height() / CHAR_HEIGHT) - 1; 
            
            char lineBuffer[64];
            int bufIdx = 0;
            
            while (*ptr) {
                if (*ptr == '\n' || bufIdx >= 63) {
                    lineBuffer[bufIdx] = '\0';
                    
                    if (lineNum >= tvScrollOffset && lineNum < tvScrollOffset + visibleLines) {
                        customPrintln(lineBuffer, false);
                    }
                    
                    lineNum++;
                    bufIdx = 0;
                } else {
                    lineBuffer[bufIdx++] = *ptr;
                }
                ptr++;
            }
            
            
            if (bufIdx > 0) {
                lineBuffer[bufIdx] = '\0';
                if (lineNum >= tvScrollOffset && lineNum < tvScrollOffset + visibleLines) {
                    customPrintln(lineBuffer, false);
                }
            }
        }
        
        releaseDisplayLock();
        requestDisplayRefresh();
    }
    
    delay(20); 
    
    return AppState::RUNNING;
}





static const char* aboutText = 
    "Flipper Zero UI Clone\n"
    "Version: 1.0.0\n"
    "\n"
    "Built with PlatformIO\n"
    "for ESP32\n"
    "\n"
    "Displays:\n"
    "- SSD1306 OLED\n"
    "- Waveshare e-Paper\n"
    "\n"
    "Features:\n"
    "- File browser UI\n"
    "- Custom icons\n"
    "- Lua scripting\n"
    "\n"
    "Press button to exit";

AppState aboutApp() {
    static bool initialized = false;
    static String loadedAboutText = "";
    
    if (!initialized) {
        
        loadedAboutText = loadAboutText();
        if (loadedAboutText.length() == 0) {
            loadedAboutText = aboutText;
        }
        setTextViewerContent("About", loadedAboutText.c_str());
        initialized = true;
    }
    
    AppState state = textViewerApp();
    
    if (state == AppState::EXIT) {
        initialized = false;
        loadedAboutText = ""; 
    }
    
    return state;
}

