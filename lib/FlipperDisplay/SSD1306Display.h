#ifndef SSD1306_DISPLAY_H
#define SSD1306_DISPLAY_H

#include "FlipperDisplay.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


class SSD1306Display : public FlipperDisplay {
public:
    SSD1306Display(uint8_t i2cAddr = 0x3C, TwoWire* wire = &Wire)
        : _i2cAddr(i2cAddr)
        , _wire(wire)
        , _display(BASE_DISPLAY_WIDTH, BASE_DISPLAY_HEIGHT, wire, -1) {}
    
    bool begin() override {
        if (!_display.begin(SSD1306_SWITCHCAPVCC, _i2cAddr)) {
            return false;
        }
        _display.clearDisplay();
        _display.display();
        return true;
    }
    
    void clearDisplay() override {
        _display.clearDisplay();
    }
    
    void display() override {
        _display.display();
    }
    
    void drawPixel(int16_t x, int16_t y, uint16_t color) override {
        _display.drawPixel(x, y, color ? SSD1306_WHITE : SSD1306_BLACK);
    }
    
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
        _display.fillRect(x, y, w, h, color ? SSD1306_WHITE : SSD1306_BLACK);
    }
    
    void drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, uint16_t color) override {
        _display.drawBitmap(x, y, bitmap, w, h, color ? SSD1306_WHITE : SSD1306_BLACK);
    }
    
    void setCursor(int16_t x, int16_t y) override {
        _display.setCursor(x, y);
    }
    
    void setTextColor(uint16_t fg, uint16_t bg) override {
        _display.setTextColor(
            fg ? SSD1306_WHITE : SSD1306_BLACK,
            bg ? SSD1306_WHITE : SSD1306_BLACK
        );
    }
    
    void setTextSize(uint8_t size) override {
        _display.setTextSize(size);
    }
    
    void print(const char* text) override {
        _display.print(text);
    }
    
    
    Adafruit_SSD1306& getDisplay() { return _display; }
    
    
    int16_t width() const override { return BASE_DISPLAY_WIDTH; }
    int16_t height() const override { return BASE_DISPLAY_HEIGHT; }
    uint8_t getScale() const override { return 1; }

private:
    uint8_t _i2cAddr;
    TwoWire* _wire;
    Adafruit_SSD1306 _display;
};

#endif 

