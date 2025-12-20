#include "keyboard.h"
#include "utils.h"
#include "controls.h"
#include <FlipperDisplay.h>

extern FlipperDisplay* display;
extern int CHAR_WIDTH;
extern int CHAR_HEIGHT;






static const char* KEY_LAYOUT_LOWER = 
    "1234567890"
    "qwertyuiop"
    "asdfghjkl_"
    "zxcvbnm.,-";

static const char* KEY_LAYOUT_UPPER = 
    "!@#$%^&*()"
    "QWERTYUIOP"
    "ASDFGHJKL_"
    "ZXCVBNM<>?";

#define K_COLS 12
#define K_ROWS 4



static const uint8_t icon_del[] = { 
    0b00000000,
    0b01000010,
    0b00100100,
    0b00011000,
    0b00011000,
    0b00100100,
    0b01000010,
    0b00000000
};


static const uint8_t icon_shift[] = { 
    0b00011000,
    0b00111100,
    0b01111110,
    0b00011000,
    0b00011000,
    0b01111110,
    0b00000000,
    0b00000000
};


static const uint8_t icon_shift_active[] = { 
    0b00011000,
    0b00111100,
    0b01111110,
    0b11111111,
    0b01111110,
    0b00111100,
    0b01111110,
    0b00000000
};


static const uint8_t icon_ok[] = { 
    0b00000000,
    0b00000001,
    0b00000011,
    0b10000110,
    0b11001100,
    0b01111000,
    0b00110000,
    0b00000000
};


static const uint8_t icon_newline[] = { 
    0b00000000,
    0b00000000,
    0b01111110,
    0b01000000,
    0b01000000,
    0b01000110,
    0b01001100,
    0b00111000
};



static const uint8_t icon_arrow_up[] = { 
    0b00011000,
    0b00111100,
    0b01111110,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00000000,
    0b00000000
};

static const uint8_t icon_arrow_left[] = { 
    0b00010000,
    0b00110000,
    0b01111110,
    0b00110000,
    0b00010000,
    0b00000000,
    0b00000000,
    0b00000000
};

static const uint8_t icon_arrow_down[] = { 
    0b00011000,
    0b00011000,
    0b01111110,
    0b00111100,
    0b00011000,
    0b00000000,
    0b00000000,
    0b00000000
};

static const uint8_t icon_arrow_right[] = { 
    0b00001000,
    0b00001100,
    0b01111110,
    0b00001100,
    0b00001000,
    0b00000000,
    0b00000000,
    0b00000000
};


static int k_sel_x = 0;
static int k_sel_y = 0;
static bool k_shift = false;
static int cursor_pos = 0;

static char getKeyChar(int x, int y) {
    if (x >= 10) return 0;
    int idx = y * 10 + x;
    return k_shift ? KEY_LAYOUT_UPPER[idx] : KEY_LAYOUT_LOWER[idx];
}

static void drawRect(int x, int y, int w, int h, int color) {
    if (!display) return;
    display->fillRect(x, y, w, 1, color);         
    display->fillRect(x, y + h - 1, w, 1, color); 
    display->fillRect(x, y, 1, h, color);         
    display->fillRect(x + w - 1, y, 1, h, color); 
}

static void drawIcon(int x, int y, const uint8_t* icon, int color) {
    if (!display) return;
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


static void getLineInfo(const char* buffer, int cursorPos, int* lineStart, int* lineLen) {
    int start = 0;
    int pos = 0;
    
    while (pos < cursorPos) {
        if (buffer[pos] == '\n') {
            start = pos + 1;
        }
        pos++;
    }
    *lineStart = start;
    
    
    int len = 0;
    pos = start;
    while (buffer[pos] != 0 && buffer[pos] != '\n') {
        len++;
        pos++;
            }
    *lineLen = len;
}

static void drawKeyboard(const char* title, const char* buffer) {
    if (!display) return;
    
    acquireDisplayLock();
    display->clearDisplay();
    
    int w = display->width();
    int h = display->height();
    int textScale = getTextScale(); 
    
    
    int inputH = CHAR_HEIGHT + 6;
    drawRect(0, 0, w, inputH, COLOR_WHITE);
    
    
    int lineStart, lineLen;
    getLineInfo(buffer, cursor_pos, &lineStart, &lineLen);
    
    int cursorInLine = cursor_pos - lineStart;
    
    
    
    int maxVisibleChars = (w - 4) / CHAR_WIDTH;
    int displayOffset = 0;
    if (cursorInLine >= maxVisibleChars) {
        displayOffset = cursorInLine - maxVisibleChars + 1;
    }
    
    int xPos = 2;
    int yPos = 3;
    
    for (int i = 0; i < maxVisibleChars; i++) {
        int idx = lineStart + displayOffset + i;
        if (buffer[idx] == 0 || buffer[idx] == '\n') break;
        
        char c[2] = {buffer[idx], 0};
        display->setCursor(xPos + i * CHAR_WIDTH, yPos);
        display->setTextColor(COLOR_WHITE, COLOR_BLACK);
        display->print(c);
    }
    
    
    int cursorScreenX = xPos + (cursorInLine - displayOffset) * CHAR_WIDTH;
    if (cursorScreenX < w - 2) {
        display->fillRect(cursorScreenX, yPos + CHAR_HEIGHT, CHAR_WIDTH, 1, COLOR_WHITE);
    }

    
    int kStartY = inputH + 2;
    int kH = h - kStartY;
    int keyW = w / K_COLS;
    int keyH = kH / K_ROWS;
    
    
    int btnW = keyW - 1;
    int btnH = keyH - 1;
    
    for (int y = 0; y < K_ROWS; y++) {
        for (int x = 0; x < K_COLS; x++) {
            int kx = x * keyW;
            int ky = kStartY + y * keyH;
            
            bool selected = (x == k_sel_x && y == k_sel_y);
            
            
            if (selected) {
                display->fillRect(kx, ky, btnW, btnH, COLOR_WHITE);
                display->setTextColor(COLOR_BLACK, COLOR_WHITE);
            } else {
                drawRect(kx, ky, btnW, btnH, COLOR_WHITE);
                display->setTextColor(COLOR_WHITE, COLOR_BLACK);
            }
            
            
            
            int cx = kx + (btnW - CHAR_WIDTH + 1) / 2;
            int cy = ky + (btnH - CHAR_HEIGHT + 1) / 2;
            
            if (x < 10) {
                
                char c = getKeyChar(x, y);
                char str[2] = {c == '_' ? ' ' : c, 0};
                display->setCursor(cx, cy);
                display->print(str);
            } else if (x == 10) {
                
                const uint8_t* icon = NULL;
                if (y == 0) icon = k_shift ? icon_shift_active : icon_shift; 
                else if (y == 1) icon = icon_del; 
                else if (y == 2) icon = icon_ok; 
                else if (y == 3) icon = icon_newline; 
                
                if (icon) {
                    int scale = display->getScale();
                    int iconSize = 8 * scale;
                    
                    int ix = kx + (btnW - iconSize + 1) / 2;
                    int iy = ky + (btnH - iconSize + 1) / 2;
                    drawIcon(ix, iy, icon, selected ? COLOR_BLACK : COLOR_WHITE);
                }
            } else if (x == 11) {
                
                const uint8_t* icon = NULL;
                if (y == 0) icon = icon_arrow_up;
                else if (y == 1) icon = icon_arrow_left;
                else if (y == 2) icon = icon_arrow_down;
                else if (y == 3) icon = icon_arrow_right;
                
                if (icon) {
                    int scale = display->getScale();
                    int iconSize = 8 * scale;
                    
                    int ix = kx + (btnW - iconSize + 1) / 2;
                    int iy = ky + (btnH - iconSize + 1) / 2;
                    drawIcon(ix, iy, icon, selected ? COLOR_BLACK : COLOR_WHITE);
                }
            }
        }
    }
    
    releaseDisplayLock();
    requestDisplayRefresh();
}

bool showKeyboard(const char* title, char* buffer, size_t maxLen) {
    k_sel_x = 0;
    k_sel_y = 0;
    k_shift = false;
    cursor_pos = strlen(buffer);
    
    bool done = false;
    bool confirm = false;
    
    drawKeyboard(title, buffer);
    
    while (!done) {
        updateControls();
        bool changed = false;
        
        
        if (isUpPressed()) {
            if (k_sel_y > 0) k_sel_y--;
            else k_sel_y = K_ROWS - 1; 
            changed = true;
        }
        if (isDownPressed()) {
            if (k_sel_y < K_ROWS - 1) k_sel_y++;
            else k_sel_y = 0; 
            changed = true;
        }
        if (isLeftPressed()) {
            if (k_sel_x > 0) k_sel_x--;
            else k_sel_x = K_COLS - 1; 
            changed = true;
        }
        if (isRightPressed()) {
            if (k_sel_x < K_COLS - 1) k_sel_x++;
            else k_sel_x = 0; 
            changed = true;
            }
        
        
        if (isButtonReleased()) {
            changed = true;
            size_t len = strlen(buffer);
            
            if (k_sel_x < 10) {
                
                if (len < maxLen - 1) {
                    
                    for (int i = len; i >= cursor_pos; i--) {
                        buffer[i + 1] = buffer[i];
                    }
                    char c = getKeyChar(k_sel_x, k_sel_y);
                    buffer[cursor_pos] = (c == '_') ? ' ' : c;
                    cursor_pos++;
                }
            } else if (k_sel_x == 10) {
                
                if (k_sel_y == 0) { 
                    k_shift = !k_shift;
                } else if (k_sel_y == 1) { 
                    if (cursor_pos > 0) {
                        for (int i = cursor_pos - 1; i < len; i++) {
                            buffer[i] = buffer[i + 1];
                        }
                        cursor_pos--;
                    }
                } else if (k_sel_y == 2) { 
                    done = true;
                    confirm = true;
                } else if (k_sel_y == 3) { 
                    if (len < maxLen - 1) {
                         for (int i = len; i >= cursor_pos; i--) {
                            buffer[i + 1] = buffer[i];
                        }
                        buffer[cursor_pos] = '\n';
                        cursor_pos++;
                    }
                }
            } else if (k_sel_x == 11) {
                
                if (k_sel_y == 1) { 
                    if (cursor_pos > 0) cursor_pos--;
                } else if (k_sel_y == 3) { 
                    if (cursor_pos < len) cursor_pos++;
                } else if (k_sel_y == 0) { 
                    
                    int curLineStart = 0;
                    for (int i = cursor_pos - 1; i >= 0; i--) {
                        if (buffer[i] == '\n') {
                            curLineStart = i + 1;
                            break;
                        }
                    }
                    int col = cursor_pos - curLineStart;
                    
                    if (curLineStart > 0) {
                        
                        int prevLineStart = 0;
                        for (int i = curLineStart - 2; i >= 0; i--) {
                            if (buffer[i] == '\n') {
                                prevLineStart = i + 1;
                                break;
                            }
                        }
                        
                        int prevLineLen = curLineStart - 1 - prevLineStart;
                        int newCol = (col > prevLineLen) ? prevLineLen : col;
                        cursor_pos = prevLineStart + newCol;
                    }
                } else if (k_sel_y == 2) { 
                     
                    int curLineStart = 0;
                    for (int i = cursor_pos - 1; i >= 0; i--) {
                        if (buffer[i] == '\n') {
                            curLineStart = i + 1;
                            break;
                        }
                    }
                    int col = cursor_pos - curLineStart;
                    
                    
                    int nextLineStart = -1;
                    for (int i = cursor_pos; i < len; i++) {
                        if (buffer[i] == '\n') {
                            nextLineStart = i + 1;
                            break;
                }
            }
                    
                    if (nextLineStart != -1) {
                         int nextLineLen = 0;
                         for (int i = nextLineStart; i < len; i++) {
                             if (buffer[i] == '\n') break;
                             nextLineLen++;
                         }
                         int newCol = (col > nextLineLen) ? nextLineLen : col;
                         cursor_pos = nextLineStart + newCol;
                    }
                }
            }
        }
        
        if (changed) {
            drawKeyboard(title, buffer);
        }
        delay(20);
    }
    
    return confirm;
}
