#ifndef CONFIG_H
    #define CONFIG_H

    
    #define SSD1306 1
    #define EPAPER  2
    #define DUAL    3
    #define EPAPER_BW 1
    #define EPAPER_3C 2

    // ! ===== DISPLAY_TYPE (selects display driver: SSD1306 for OLED, EPAPER for e-paper, DUAL for both) =====
    #ifndef DISPLAY_TYPE
        #define DISPLAY_TYPE EPAPER
    #endif

    // ! ===== EPAPER_COLOR_MODE (selects e-paper color mode: EPAPER_BW for black/white, EPAPER_3C for 3-color) (when DISPLAY_TYPE is EPAPER or DUAL) =====
    #ifndef EPAPER_COLOR_MODE
        #define EPAPER_COLOR_MODE EPAPER_BW
    #endif

    // ! ===== EPAPER_WIDTH (e-paper display width in pixels) (when DISPLAY_TYPE is EPAPER or DUAL) =====
    #ifndef EPAPER_WIDTH
        #define EPAPER_WIDTH  296
    #endif

    // ! ===== EPAPER_HEIGHT (e-paper display height in pixels) (when DISPLAY_TYPE is EPAPER or DUAL) =====
    #ifndef EPAPER_HEIGHT
        #define EPAPER_HEIGHT 128
    #endif

    // ! ===== EPAPER_USE_FULL_UPDATE (controls e-paper update mode: 0=partial update, 1=full update) (when DISPLAY_TYPE is EPAPER or DUAL with EPAPER_BW mode) =====
    #ifndef EPAPER_USE_FULL_UPDATE
        #define EPAPER_USE_FULL_UPDATE 0
    #endif

    // ! ===== RENDER_QUEUE_SETTLE_MS (delay in milliseconds for render queue to settle before refresh) (when EPAPER with partial updates) =====
    #ifndef RENDER_QUEUE_SETTLE_MS
        #define RENDER_QUEUE_SETTLE_MS 200
    #endif

    // ! ===== JOYSTICK_INVERT_X (inverts joystick X axis direction: 0=normal, 1=inverted) =====
    #ifndef JOYSTICK_INVERT_X
        #define JOYSTICK_INVERT_X 0
    #endif

    // ! ===== JOYSTICK_INVERT_Y (inverts joystick Y axis direction: 0=normal, 1=inverted) =====
    #ifndef JOYSTICK_INVERT_Y
        #define JOYSTICK_INVERT_Y 0
    #endif

#endif
