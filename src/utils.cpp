#include "utils.h"
#include "config.h"


const int BASE_CHAR_WIDTH = 6;
const int BASE_CHAR_HEIGHT = 8;


int CHAR_WIDTH = 6;
int CHAR_HEIGHT = 8;


int cursorX = 0;
int cursorY = 0;


FlipperDisplay* display = nullptr;


static uint8_t displayScale = 1;

int getDisplayScale() {
    return displayScale;
}


static uint8_t textScale = 1;

int getTextScale() {
    return textScale;
}

int getTextHeight() {
    return BASE_CHAR_HEIGHT;
}

void initUtils(FlipperDisplay* disp) {
    display = disp;
    
    
    int w = disp->width();
    int h = disp->height();
    int scaleX = max(1, w / 128);
    int scaleY = max(1, h / 64);
    
    
    displayScale = min(scaleX, scaleY);
    textScale = displayScale;
    
    
    CHAR_WIDTH = BASE_CHAR_WIDTH * textScale;
    CHAR_HEIGHT = BASE_CHAR_HEIGHT * textScale;
    
    
    disp->setTextSize(textScale);
    
    resetCursor();
}


void customPrintUnlocked(const char* text, bool invert) {
    if (!display) return;
    
    int textWidth = strlen(text) * CHAR_WIDTH;
    
    if (invert) {
        
        display->fillRect(cursorX, cursorY, textWidth, CHAR_HEIGHT, COLOR_WHITE);
        display->setTextColor(COLOR_BLACK, COLOR_WHITE);
    } else {
        display->setTextColor(COLOR_WHITE, COLOR_BLACK);
    }
    
    
    display->setCursor(cursorX, cursorY);
    display->print(text);
    
    
    cursorX += textWidth;
    
    
    display->setTextColor(COLOR_WHITE, COLOR_BLACK);
}

void customPrint(const char* text, bool invert) {
    if (!display) return;
    
    acquireDisplayLock();
    customPrintUnlocked(text, invert);
    releaseDisplayLock();
}


void customPrintlnUnlocked(const char* text, bool invert) {
    if (!display) return;
    
    if (invert) {
        
        display->fillRect(0, cursorY, display->width(), CHAR_HEIGHT, COLOR_WHITE);
        display->setTextColor(COLOR_BLACK, COLOR_WHITE);
    } else {
        display->setTextColor(COLOR_WHITE, COLOR_BLACK);
    }
    
    
    int availableWidth = display->width() - cursorX;
    int maxChars = availableWidth / CHAR_WIDTH;
    
    
    static char truncatedText[64];
    int textLen = strlen(text);
    
    if (textLen > maxChars && maxChars > 3) {
        
        int copyLen = maxChars - 3;
        strncpy(truncatedText, text, copyLen);
        truncatedText[copyLen] = '.';
        truncatedText[copyLen + 1] = '.';
        truncatedText[copyLen + 2] = '.';
        truncatedText[copyLen + 3] = '\0';
        text = truncatedText;
    } else if (textLen > maxChars) {
        
        strncpy(truncatedText, text, maxChars);
        truncatedText[maxChars] = '\0';
        text = truncatedText;
    }
    
    
    display->setCursor(cursorX, cursorY);
    display->print(text);
    
    
    cursorX = 0;
    cursorY += CHAR_HEIGHT;
    
    
    display->setTextColor(COLOR_WHITE, COLOR_BLACK);
}

void customPrintln(const char* text, bool invert) {
    if (!display) return;
    
    acquireDisplayLock();
    customPrintlnUnlocked(text, invert);
    releaseDisplayLock();
}

void resetCursor() {
    cursorX = 0;
    cursorY = 0;
}

void showLoading(const char* title, const char* subtitle, bool showSpinner) {
    if (!display) return;
    
    acquireDisplayLock();
    
    display->clearDisplay();
    
    int w = display->width();
    int h = display->height();
    int scale = display->getScale();
    
    
    int titleLen = strlen(title);
    int titleWidth = titleLen * CHAR_WIDTH;
    int titleX = (w - titleWidth) / 2;
    int titleY = h / 2 - CHAR_HEIGHT;
    
    
    if (titleX < 0) titleX = 0;
    
    
    display->setTextColor(COLOR_WHITE, COLOR_BLACK);
    display->setCursor(titleX, titleY);
    display->print(title);
    
    
    if (subtitle && strlen(subtitle) > 0) {
        int subLen = strlen(subtitle);
        int subWidth = subLen * CHAR_WIDTH;
        int subX = (w - subWidth) / 2;
        int subY = titleY + CHAR_HEIGHT + 4;
        
        if (subX < 0) subX = 0;
        
        display->setCursor(subX, subY);
        display->print(subtitle);
    }
    
    
    if (showSpinner) {
        static int spinFrame = 0;
        int spinX = w / 2;
        int spinY = titleY - CHAR_HEIGHT - 12;
        int dotSize = 3 * scale;
        int spacing = dotSize * 2;
        
        
        for (int i = 0; i < 3; i++) {
            int dx = (i - 1) * spacing;
            if (i == (spinFrame % 3)) {
                display->fillRect(spinX + dx - dotSize/2, spinY, dotSize, dotSize, COLOR_WHITE);
            } else {
                
                display->fillRect(spinX + dx - 1, spinY + 1, 2, 2, COLOR_WHITE);
            }
        }
        spinFrame++;
    }
    
    
    int borderWidth = 2;
    display->fillRect(8, 8, w - 16, borderWidth, COLOR_WHITE);                    
    display->fillRect(8, h - 8 - borderWidth, w - 16, borderWidth, COLOR_WHITE);  
    display->fillRect(8, 8, borderWidth, h - 16, COLOR_WHITE);                    
    display->fillRect(w - 8 - borderWidth, 8, borderWidth, h - 16, COLOR_WHITE);  
    
    display->display();
    
    releaseDisplayLock();
}

void showLoadingMessage(const char* message) {
    showLoading(message, nullptr, false);
}






#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

static bool ledInitialized = false;

void initStatusLED() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);  
    ledInitialized = true;
}

void setLEDBusy() {
    if (ledInitialized) {
        digitalWrite(LED_BUILTIN, HIGH);  
    }
}

void setLEDReady() {
    if (ledInitialized) {
        digitalWrite(LED_BUILTIN, LOW);  
    }
}





#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <freertos/task.h>

static SemaphoreHandle_t displayMutex = NULL;

void acquireDisplayLock() {
    if (!displayMutex) {
        displayMutex = xSemaphoreCreateRecursiveMutex();
    }
    if (displayMutex) {
        xSemaphoreTakeRecursive(displayMutex, portMAX_DELAY);
    }
}

void releaseDisplayLock() {
    if (displayMutex) {
        xSemaphoreGiveRecursive(displayMutex);
    }
}





static QueueHandle_t renderQueue = NULL;
static TaskHandle_t renderTaskHandle = NULL;
static volatile bool renderQueueInitialized = false;
static volatile bool renderBusy = false;
static SemaphoreHandle_t initMutex = NULL;



static void renderQueueTask(void* parameter) {
    uint8_t dummy;
    
    while (true) {
        
        if (xQueueReceive(renderQueue, &dummy, portMAX_DELAY) == pdTRUE) {
            
            
            
            int queuedCount = 1;  
            while (xQueueReceive(renderQueue, &dummy, 0) == pdTRUE) {
                queuedCount++;
            }
            
            
            
            
            renderBusy = true;
            acquireDisplayLock();
            
            #if (DISPLAY_TYPE == EPAPER || DISPLAY_TYPE == DUAL) && EPAPER_COLOR_MODE == EPAPER_BW && !EPAPER_USE_FULL_UPDATE
            
            
            vTaskDelay(pdMS_TO_TICKS(RENDER_QUEUE_SETTLE_MS));
            #endif
            
            
            
            
            if (display) {
                setLEDBusy();
                display->display();  
                setLEDReady();
            }
            
            releaseDisplayLock();
            renderBusy = false;
        }
    }
}

void initRenderQueue() {
    
    if (!initMutex) {
        initMutex = xSemaphoreCreateMutex();
    }
    
    if (initMutex) {
        xSemaphoreTake(initMutex, portMAX_DELAY);
    }
    
    
    if (renderQueueInitialized) {
        if (initMutex) {
            xSemaphoreGive(initMutex);
        }
        return;
    }
    
    
    
    renderQueue = xQueueCreate(256, sizeof(uint8_t));
    
    if (renderQueue) {
        
        xTaskCreatePinnedToCore(
            renderQueueTask,
            "RenderQueue",
            4096,
            NULL,
            1,  
            &renderTaskHandle,
            0   
        );
        
        renderQueueInitialized = true;
        Serial.println(F("Render queue initialized"));
    }
    
    if (initMutex) {
        xSemaphoreGive(initMutex);
    }
}

void requestDisplayRefresh() {
    
    if (!renderQueueInitialized) {
        initRenderQueue();
    }
    
    if (!renderQueue) {
        
        if (display) {
            acquireDisplayLock();
            setLEDBusy();
            display->display();
            setLEDReady();
            releaseDisplayLock();
        }
        return;
    }
    
    
    
    
    
    
    
    
    uint8_t dummy = 1;
    xQueueSend(renderQueue, &dummy, 0);  
}

bool isRenderBusy() {
    return renderBusy;
}

bool isRenderPending() {
    if (!renderQueue) return false;
    return uxQueueMessagesWaiting(renderQueue) > 0;
}





#include <LittleFS.h>


static uint8_t* loadingScreenData = nullptr;
static int loadingScreenWidth = 0;
static int loadingScreenHeight = 0;
static bool loadingScreenVisible = false;

String loadAboutText() {
    File file = LittleFS.open("/assets/about.txt", "r");
    if (!file) {
        Serial.println(F("Failed to open /assets/about.txt"));
        return "";
    }
    
    String content = file.readString();
    file.close();
    
    Serial.print(F("Loaded about text: "));
    Serial.print(content.length());
    Serial.println(F(" bytes"));
    
    return content;
}

bool loadLoadingScreen() {
    File file = LittleFS.open("/assets/loading_screen.txt", "r");
    if (!file) {
        Serial.println(F("Failed to open /assets/loading_screen.txt (optional)"));
        return false;
    }
    
    
    String line = file.readStringUntil('\n');
    int spacePos = line.indexOf(' ');
    if (spacePos <= 0) {
        file.close();
        Serial.println(F("Invalid loading screen format"));
        return false;
    }
    
    loadingScreenWidth = line.substring(0, spacePos).toInt();
    loadingScreenHeight = line.substring(spacePos + 1).toInt();
    
    if (loadingScreenWidth <= 0 || loadingScreenHeight <= 0 || 
        loadingScreenWidth > 512 || loadingScreenHeight > 512) {
        file.close();
        Serial.println(F("Invalid loading screen dimensions"));
        return false;
    }
    
    
    size_t dataSize = loadingScreenWidth * loadingScreenHeight;
    if (loadingScreenData) {
        free(loadingScreenData);
    }
    loadingScreenData = (uint8_t*)malloc(dataSize);
    
    if (!loadingScreenData) {
        file.close();
        Serial.println(F("Failed to allocate memory for loading screen"));
        return false;
    }
    
    
    
    String allData = file.readString();
    file.close();
    
    
    allData.trim();
    
    
    int expectedPixels = loadingScreenWidth * loadingScreenHeight;
    if (allData.length() < (unsigned int)expectedPixels) {
        Serial.print(F("Error: Loading screen file has insufficient data: "));
        Serial.print(allData.length());
        Serial.print(F(" bytes, expected at least "));
        Serial.print(expectedPixels);
        Serial.println(F(" bytes"));
        free(loadingScreenData);
        loadingScreenData = nullptr;
        loadingScreenWidth = 0;
        loadingScreenHeight = 0;
        return false;
    }
    
    
    int dataIdx = 0;
    for (int y = 0; y < loadingScreenHeight; y++) {
        for (int x = 0; x < loadingScreenWidth; x++) {
            if (dataIdx < (int)allData.length()) {
                char pixel = allData[dataIdx];
                
                while (dataIdx < (int)allData.length() && (pixel == '\n' || pixel == '\r' || pixel == ' ' || pixel == '\t')) {
                    dataIdx++;
                    if (dataIdx < (int)allData.length()) {
                        pixel = allData[dataIdx];
                    }
                }
                if (dataIdx < (int)allData.length()) {
                    loadingScreenData[y * loadingScreenWidth + x] = (pixel == '1') ? 1 : 0;
                    dataIdx++;
                } else {
                    loadingScreenData[y * loadingScreenWidth + x] = 0;
                }
            } else {
                loadingScreenData[y * loadingScreenWidth + x] = 0;
            }
        }
    }
    
    
    Serial.print(F("Loaded loading screen: "));
    Serial.print(loadingScreenWidth);
    Serial.print(F("x"));
    Serial.print(loadingScreenHeight);
    Serial.print(F(" ("));
    Serial.print(dataSize);
    Serial.println(F(" bytes)"));
    
    return true;
}

void showLoadingScreenOverlay(const char* text) {
    if (!display || !loadingScreenData || loadingScreenWidth == 0 || loadingScreenHeight == 0) {
        return;
    }
    
    acquireDisplayLock();
    
    loadingScreenVisible = true;
    
    
    if (!display) {
        releaseDisplayLock();
        return;
    }
    
    
    display->clearDisplay();
    
    
    int displayWidth = display->width();
    int displayHeight = display->height();
    
    
    if (displayWidth <= 0 || displayHeight <= 0) {
        releaseDisplayLock();
        return;
    }
    
    
    float scaleX = (float)displayWidth / loadingScreenWidth;
    float scaleY = (float)displayHeight / loadingScreenHeight;
    float scale = max(scaleX, scaleY);  
    
    int scaledWidth = (int)(loadingScreenWidth * scale);
    int scaledHeight = (int)(loadingScreenHeight * scale);
    
    
    int offsetX = (displayWidth - scaledWidth) / 2;
    int offsetY = (displayHeight - scaledHeight) / 2;
    
    
    for (int y = 0; y < scaledHeight; y++) {
        int srcY = (int)(y / scale);
        if (srcY >= loadingScreenHeight) break;
        
        for (int x = 0; x < scaledWidth; x++) {
            int srcX = (int)(x / scale);
            if (srcX >= loadingScreenWidth) break;
            
            int pixelIdx = srcY * loadingScreenWidth + srcX;
            if (pixelIdx < loadingScreenWidth * loadingScreenHeight && loadingScreenData[pixelIdx] == 1) {
                int drawX = offsetX + x;
                int drawY = offsetY + y;
                
                
                if (drawX >= 0 && drawX < displayWidth && drawY >= 0 && drawY < displayHeight) {
                    display->drawPixel(drawX, drawY, COLOR_WHITE);
                }
            }
        }
    }
    
    
    if (text && strlen(text) > 0) {
        int textLen = strlen(text);
        int textWidth = textLen * CHAR_WIDTH;
        int textX = (displayWidth - textWidth) / 2;
        int textY = 8;  
        
        if (textX < 0) textX = 0;
        
        display->setTextColor(COLOR_WHITE, COLOR_BLACK);
        display->setCursor(textX, textY);
        display->print(text);
    }
    
    releaseDisplayLock();
}

void hideLoadingScreenOverlay() {
    loadingScreenVisible = false;
    
    
}

bool isLoadingScreenVisible() {
    return loadingScreenVisible;
}

