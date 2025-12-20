#ifndef EPAPER_DISPLAY_H
#define EPAPER_DISPLAY_H

#include "FlipperDisplay.h"


#ifndef EPAPER_BW
#define EPAPER_BW 1
#endif
#ifndef EPAPER_3C
#define EPAPER_3C 2
#endif


#ifndef EPAPER_COLOR_MODE
#define EPAPER_COLOR_MODE EPAPER_3C
#endif


#ifndef EPAPER_WIDTH
#define EPAPER_WIDTH 296
#endif
#ifndef EPAPER_HEIGHT
#define EPAPER_HEIGHT 128
#endif

#if EPAPER_COLOR_MODE == EPAPER_BW
    #include <GxEPD2_BW.h>
#else
    #include <GxEPD2_3C.h>
#endif




#define EPAPER_CS_PIN   5
#define EPAPER_DC_PIN   17
#define EPAPER_RST_PIN  16
#define EPAPER_BUSY_PIN 4


#if EPAPER_COLOR_MODE == EPAPER_BW
    
    typedef GxEPD2_BW<GxEPD2_290_T94_V2, GxEPD2_290_T94_V2::HEIGHT> EPaperDisplayType;
    #define EPAPER_DRIVER GxEPD2_290_T94_V2
#else
    
    typedef GxEPD2_3C<GxEPD2_290_C90c, GxEPD2_290_C90c::HEIGHT> EPaperDisplayType;
    #define EPAPER_DRIVER GxEPD2_290_C90c
#endif

class EPaperDisplay : public FlipperDisplay {
public:
    EPaperDisplay(int8_t csPin = EPAPER_CS_PIN, 
                  int8_t dcPin = EPAPER_DC_PIN, 
                  int8_t rstPin = EPAPER_RST_PIN, 
                  int8_t busyPin = EPAPER_BUSY_PIN)
        : _display(EPAPER_DRIVER(csPin, dcPin, rstPin, busyPin))
        , _cursorX(0)
        , _cursorY(0)
        , _textColorFg(GxEPD_WHITE)
        , _textColorBg(GxEPD_BLACK)
        , _wasCleared(false)
        , _changedPixels(0) {}
    
    bool begin() override {
        #if EPAPER_COLOR_MODE == EPAPER_BW
            _display.init(115200, true, 2, false);   
        #else
            _display.init(115200, true, 50, false);  
        #endif
        _display.setRotation(1);  
        _display.setTextWrap(false);
        _display.setFullWindow();
        clearDisplay();
        _display.display(false);  
        _wasCleared = false;
        _changedPixels = 0;
        return true;
    }
    
    void clearDisplay() override {
        _display.fillScreen(GxEPD_BLACK);
        _wasCleared = true;
        _changedPixels = width() * height(); 
    }
    
    void display() override {
        #if EPAPER_COLOR_MODE == EPAPER_BW
            #if EPAPER_USE_FULL_UPDATE
                _display.display(false);
            #else
                _display.display(true);
            #endif
            _wasCleared = false;
            _changedPixels = 0;
        #else
            _display.display(false);
        #endif
    }
    
    void drawPixel(int16_t x, int16_t y, uint16_t color) override {
        _display.drawPixel(x, y, color ? GxEPD_WHITE : GxEPD_BLACK);
        _changedPixels++;
    }
    
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
        _display.fillRect(x, y, w, h, color ? GxEPD_WHITE : GxEPD_BLACK);
        _changedPixels += w * h;
    }
    
    void drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, uint16_t color) override {
        _display.drawBitmap(x, y, bitmap, w, h, color ? GxEPD_WHITE : GxEPD_BLACK);
        _changedPixels += w * h;
    }
    
    void setCursor(int16_t x, int16_t y) override {
        _cursorX = x;
        _cursorY = y;
        _display.setCursor(x, y);
    }
    
    void setTextColor(uint16_t fg, uint16_t bg) override {
        _textColorFg = fg ? GxEPD_WHITE : GxEPD_BLACK;
        _textColorBg = bg ? GxEPD_WHITE : GxEPD_BLACK;
        _display.setTextColor(_textColorFg, _textColorBg);
    }
    
    void setTextSize(uint8_t size) override {
        _display.setTextSize(size);
    }
    
    void print(const char* text) override {
        _display.print(text);
    }
    
    
    void displayFull() {
        _display.display(false);
    }
    
    
    EPaperDisplayType& getDisplay() { return _display; }
    
    
    int16_t width() const override { return EPAPER_WIDTH; }
    int16_t height() const override { return EPAPER_HEIGHT; }
    
    
    uint8_t getScale() const override { 
        return max(1, EPAPER_HEIGHT / BASE_DISPLAY_HEIGHT);
    }

private:
    EPaperDisplayType _display;
    int16_t _cursorX;
    int16_t _cursorY;
    uint16_t _textColorFg;
    uint16_t _textColorBg;
    bool _wasCleared;
    int _changedPixels;
};

#endif 
