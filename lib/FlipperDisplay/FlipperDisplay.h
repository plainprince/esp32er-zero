#ifndef FLIPPER_DISPLAY_H
#define FLIPPER_DISPLAY_H

#include <Arduino.h>


#define BASE_DISPLAY_WIDTH 128
#define BASE_DISPLAY_HEIGHT 64


#define COLOR_BLACK 0
#define COLOR_WHITE 1


class FlipperDisplay {
public:
    virtual ~FlipperDisplay() = default;
    
    
    virtual bool begin() = 0;
    
    
    virtual void clearDisplay() = 0;
    virtual void display() = 0;
    
    
    virtual void drawPixel(int16_t x, int16_t y, uint16_t color) = 0;
    virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) = 0;
    virtual void drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, uint16_t color) = 0;
    
    
    virtual void drawScaledBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, uint16_t color, uint8_t scale) {
        
        for (int16_t j = 0; j < h; j++) {
            for (int16_t i = 0; i < w; i++) {
                
                uint8_t byte = pgm_read_byte(&bitmap[i]);
                if (byte & (1 << j)) {
                    fillRect(x + i * scale, y + j * scale, scale, scale, color);
                }
            }
        }
    }
    
    
    virtual void setCursor(int16_t x, int16_t y) = 0;
    virtual void setTextColor(uint16_t fg, uint16_t bg) = 0;
    virtual void setTextSize(uint8_t size) = 0;
    virtual void print(const char* text) = 0;
    
    
    virtual int16_t width() const { return BASE_DISPLAY_WIDTH; }
    virtual int16_t height() const { return BASE_DISPLAY_HEIGHT; }
    
    
    virtual uint8_t getScale() const { 
        
        return max(1, height() / BASE_DISPLAY_HEIGHT);
    }
};

#endif 

