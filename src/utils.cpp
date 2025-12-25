#include "utils.h"
#include "config.h"
#include <string.h>
#include <string>
#include <algorithm>
#include <memory>


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

bool ledInitialized = false;

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

// Loading screen state (declared early so requestDisplayRefresh can access it)
static bool loadingScreenVisible = false;
static bool loadingScreenNeedsFlush = false;



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
    // If loading screen is visible and was just drawn, flush it once and skip queue
    if (loadingScreenVisible && loadingScreenNeedsFlush) {
        // Flush the loading screen once, then clear the flag
        if (display) {
            acquireDisplayLock();
            setLEDBusy();
            display->display();
            setLEDReady();
            releaseDisplayLock();
        }
        loadingScreenNeedsFlush = false;
        return;
    }
    
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



String loadAboutText() { // Keep String for now as caller might expect it, or update header
    File file = LittleFS.open("/assets/about.txt", "r");
    if (!file) {
        Serial.println(F("Failed to open /assets/about.txt"));
        return "";
    }
    
    // Read to String buffer (Arduino String is still convenient for file reading if we convert immediately)
    // But better to use loop
    String content = file.readString();
    file.close();
    
    Serial.print(F("Loaded about text: "));
    Serial.print(content.length());
    Serial.println(F(" bytes"));
    
    return content;
}

// Structure to hold loading screen data - returned by loadLoadingScreen()
struct LoadingScreenData {
    std::unique_ptr<uint8_t[]> data;
    int width;
    int height;
    size_t size;
    
    LoadingScreenData() : data(nullptr), width(0), height(0), size(0) {}
    
    bool isValid() const {
        return data != nullptr && width > 0 && height > 0;
    }
    
    // Clear the data - smart pointer will automatically free memory
    void clear() {
        data.reset();
        width = 0;
        height = 0;
        size = 0;
    }
};

// Static cache for loading screen data - REMOVED to save memory
// Data is now loaded on demand and freed immediately
// static LoadingScreenData cachedLoadingScreenData;

// Load loading screen data - returns a struct with smart pointer for automatic cleanup
static LoadingScreenData loadLoadingScreen() {
    LoadingScreenData result;
    
    File file = LittleFS.open("/assets/loading_screen.txt", "r");
    if (!file) {
        Serial.println(F("Failed to open /assets/loading_screen.txt (optional)"));
        return result;
    }
    
    // Read header without using String to avoid heap fragmentation
    char headerBuf[64];
    size_t headerIdx = 0;
    while (file.available() && headerIdx < sizeof(headerBuf) - 1) {
        char c = file.read();
        if (c == '\n') break;
        headerBuf[headerIdx++] = c;
    }
    headerBuf[headerIdx] = '\0';
    
    // Parse dimensions
    char* spacePtr = strchr(headerBuf, ' ');
    if (!spacePtr) {
        file.close();
        Serial.println(F("Invalid loading screen format"));
        return result;
    }
    *spacePtr = '\0'; // Split string at space
    
    result.width = atoi(headerBuf);
    result.height = atoi(spacePtr + 1);
    
    if (result.width <= 0 || result.height <= 0 || 
        result.width > 512 || result.height > 512) {
        file.close();
        Serial.println(F("Invalid loading screen dimensions"));
        return result;
    }
    
    result.size = result.width * result.height;
    
    // Allocate using smart pointer - automatically freed when out of scope
    result.data = std::unique_ptr<uint8_t[]>(new uint8_t[result.size]);
    if (!result.data) {
        file.close();
        Serial.println(F("Failed to allocate memory for loading screen"));
        return result;
    }
    
    // Read directly into buffer
    int pixelsRead = 0;
    int targetPixels = result.size;
    
    // Use a small buffer for reading to improve performance over single-byte reads
    const int BUF_SIZE = 64;
    uint8_t buf[BUF_SIZE];
    
    while (file.available() && pixelsRead < targetPixels) {
        int n = file.read(buf, BUF_SIZE);
        for (int i = 0; i < n && pixelsRead < targetPixels; i++) {
            char c = (char)buf[i];
            // Skip whitespace
            if (c != '\n' && c != '\r' && c != ' ' && c != '\t') {
                result.data[pixelsRead++] = (c == '1') ? 1 : 0;
            }
        }
    }
    
    file.close();
    
    
    if (pixelsRead < targetPixels) {
        Serial.print(F("Warning: Loading screen file has insufficient data: "));
        Serial.print(pixelsRead);
        Serial.print(F(" pixels, expected "));
        Serial.println(targetPixels);
        
        // Fill remaining with 0
        while (pixelsRead < targetPixels) {
            result.data[pixelsRead++] = 0;
        }
    }
    
    Serial.print(F("Loaded loading screen: "));
    Serial.print(result.width);
    Serial.print(F("x"));
    Serial.print(result.height);
    Serial.print(F(" ("));
    Serial.print(result.size);
    Serial.println(F(" bytes)"));
    
    return result;
}

void freeLoadingScreen() {
    // Clear visibility flags only
    // Data is automatically freed immediately after use in showLoadingScreenOverlay
    loadingScreenVisible = false;
    loadingScreenNeedsFlush = false;
}

void showLoadingScreenOverlay(const char* text) {
    static char lastLoadingText[64] = "";
    
    // Check if already visible with same text to avoid redraw
    if (loadingScreenVisible) {
        if ((text == nullptr && lastLoadingText[0] == '\0') || 
            (text != nullptr && strncmp(text, lastLoadingText, sizeof(lastLoadingText)) == 0)) {
            // Already showing the same loading screen, no need to redraw
            return;
        }
    }

    // Load loading screen data immediately on stack - freed when function returns
    LoadingScreenData screenData = loadLoadingScreen();
    
    if (!screenData.isValid()) {
        return; // Failed to load
    }
    
    if (!display) {
        return;
    }
    
    acquireDisplayLock();
    
    // Update text tracking before drawing
    if (text) {
        strncpy(lastLoadingText, text, sizeof(lastLoadingText) - 1);
        lastLoadingText[sizeof(lastLoadingText) - 1] = '\0';
    } else {
        lastLoadingText[0] = '\0';
    }
    
    // Set visible flag and mark that we need to flush
    loadingScreenVisible = true;
    loadingScreenNeedsFlush = true;
    
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
    
    
    float scaleX = (float)displayWidth / screenData.width;
    float scaleY = (float)displayHeight / screenData.height;
    float scale = max(scaleX, scaleY);  
    
    int scaledWidth = (int)(screenData.width * scale);
    int scaledHeight = (int)(screenData.height * scale);
    
    
    int offsetX = (displayWidth - scaledWidth) / 2;
    int offsetY = (displayHeight - scaledHeight) / 2;
    
    
    for (int y = 0; y < scaledHeight; y++) {
        int srcY = (int)(y / scale);
        if (srcY >= screenData.height) break;
        
        for (int x = 0; x < scaledWidth; x++) {
            int srcX = (int)(x / scale);
            if (srcX >= screenData.width) break;
            
            int pixelIdx = srcY * screenData.width + srcX;
            if (pixelIdx < screenData.size && screenData.data[pixelIdx] == 1) {
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
    
    // Don't flush here - let display.refresh() handle it to avoid multiple renders
    // The loading screen will be flushed when display.refresh() is called
    
    releaseDisplayLock();
}

void hideLoadingScreenOverlay() {
    loadingScreenVisible = false;
    loadingScreenNeedsFlush = false;
    // Don't free the loading screen here to avoid memory fragmentation
    // It will be freed when the app exits via freeLoadingScreen()
}

bool isLoadingScreenVisible() {
    return loadingScreenVisible;
}
