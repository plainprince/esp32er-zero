/*
 * Flipper Zero UI Clone
 * 
 * WIRING (ESP32 DevKit V1):
 * 
 * Waveshare 2.9" e-Paper (296x128, 8-pin SPI):
 *   VCC  -> 3.3V (NOT 5V!)
 *   GND  -> GND
 *   DIN  -> GPIO 23 (MOSI)
 *   CLK  -> GPIO 18 (SCK)
 *   CS   -> GPIO 5
 *   DC   -> GPIO 17
 *   RST  -> GPIO 16
 *   BUSY -> GPIO 4
 * 
 * Analog Joystick (5-pin):
 *   VCC  -> 3.3V
 *   GND  -> GND
 *   VRx  -> GPIO 34 (horizontal)
 *   VRy  -> GPIO 35 (vertical)
 *   SW   -> GPIO 32 (button, directly to pin - uses internal pullup)
 * 
 * SSD1306 OLED (I2C, 4-pin) - alternative display:
 *   VCC  -> 3.3V
 *   GND  -> GND
 *   SCL  -> GPIO 22
 *   SDA  -> GPIO 21
 * 
 * IR LED (for universal remote):
 *   Anode  -> GPIO 25 (via current-limiting resistor ~100-220 ohm)
 *   Cathode -> GND
 *   Note: Use a transistor (e.g., 2N2222) for higher power/range
 */

#include <Arduino.h>
#include "config.h"

// Display driver includes
#if DISPLAY_TYPE == SSD1306
    #include <SSD1306Display.h>
#elif DISPLAY_TYPE == EPAPER
    #include <EPaperDisplay.h>
#elif DISPLAY_TYPE == DUAL
    #include <SSD1306Display.h>
    #include <EPaperDisplay.h>
#endif

#include "utils.h"
#include "icons.h"
#include "filesystem.h"
#include "menu.h"
#include "controls.h"
#include "app_runner.h"
#include "lua_app.h"
#include "lua_scripts.h"
#include "lua_fs.h"
#include "cpp_app.h"
#include "file_explorer.h"
#include "ir_remote.h"


// Create display instance(s)
#if DISPLAY_TYPE == SSD1306
    SSD1306Display displayDriver;
#elif DISPLAY_TYPE == EPAPER
    // Pins: CS=5, DC=17, RST=16, BUSY=4
    EPaperDisplay displayDriver(5, 17, 16, 4);
#elif DISPLAY_TYPE == DUAL
    // Primary display: OLED for instant feedback
    SSD1306Display oledDisplay;
    // Secondary display: e-Paper for persistent view
    EPaperDisplay epaperDisplay(5, 17, 16, 4);
    // Use OLED as main display driver for menu system
    #define displayDriver oledDisplay
#endif

// ============================================================================
// Custom icon example - define your own 8x8 icons
// ============================================================================

// Custom "game" icon - gamepad shape
DEFINE_ICON(gameIcon, 
    0x00, 0x7E, 0xFF, 0xDB, 0xDB, 0xFF, 0x7E, 0x00
);

// Custom "music" icon - note shape
DEFINE_ICON(musicIcon,
    0x0F, 0x0B, 0x09, 0x09, 0x79, 0xF9, 0xF0, 0x60
);

// Custom "photo" icon - camera shape
DEFINE_ICON(photoIcon,
    0x00, 0x1C, 0x7F, 0x41, 0x5D, 0x5D, 0x41, 0x7F
);

// ============================================================================
// Application callbacks - functions that run when apps are selected
// ============================================================================


void onAbout(const char* path, const char* name) {
    Serial.println("Opening About...");
    startApp(aboutApp);
}

void onGameStart(const char* path, const char* name) {
    Serial.print("Starting game: ");
    Serial.println(name);
    setTextViewerContent(name, "Game not implemented yet!\n\nThis would be where the\ngame runs.\n\nPress button to exit");
    startApp(textViewerApp);
}

void onMusicPlay(const char* path, const char* name) {
    Serial.print("Playing: ");
    Serial.println(name);
    setTextViewerContent("Music Player", "Now playing...\n\nThis is a placeholder\nfor the music player.\n\nPress button to exit");
    startApp(textViewerApp);
}

// Generic Lua script launcher - loads script from filesystem based on UI path
void onLuaScript(const char* uiPath, const char* name) {
    // Look up the actual filesystem path from the registered mapping
    String fsPath = getScriptFsPath(uiPath);
    
    if (fsPath.length() == 0) {
        Serial.print("ERROR: No filesystem path registered for: ");
        Serial.println(uiPath);
        return;
    }
    
    Serial.print("Starting Lua script: ");
    Serial.print(name);
    Serial.print(" from ");
    Serial.println(fsPath);
    
    setLuaScriptFromFile(fsPath.c_str());
    startApp(luaApp);
}

// Generic C++ app launcher - runs registered C++ app by name
void onCppApp(const char* uiPath, const char* name) {
    Serial.print("Starting C++ app at: ");
    Serial.println(uiPath);
    
    runCppAppByPath(uiPath);
}

// File explorer launcher
void onFileExplorer(const char* uiPath, const char* name) {
    Serial.println("Starting File Explorer...");
    setFileExplorerRoot("/storage");
    startApp(fileExplorerApp);
}

// ============================================================================
// File structure definition using the new string format
// ============================================================================

// Format: [TYPE] [ICON:icon_id] NAME
// Types: d=folder, f=file, a=application
// ICON: prefix is optional, uses default icon if omitted

// Minimal file structure - apps are loaded from data/apps folder
const String fileStructure = R"(
d ICON:settings Settings
 d ICON:info Documentation
  f ICON:info About
  f ICON:info Lua Docs
a ICON:sd Storage
)";

// ============================================================================
// Setup and Loop
// ============================================================================

// ESP32 built-in LED pin
#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

// Render task system - separate thread for rendering
static volatile bool renderRequested = false;
// Render task uses polling instead of semaphore for simplicity
static TaskHandle_t renderTaskHandle = NULL;

// Forward declarations
void renderTask(void* parameter);
void requestRender();

void setup() {
    Serial.begin(115200);
    Serial.println(F("Flipper Zero UI Starting..."));
    
    // Initialize status LED (LED ON = ready, LED OFF = busy)
    initStatusLED();
    setLEDBusy();  // Start busy during initialization
    Serial.println(F("Status LED initialized"));
    
    // Initialize display(s)
    #if DISPLAY_TYPE == DUAL
        // Initialize OLED first (fast)
        if (!oledDisplay.begin()) {
            Serial.println(F("OLED initialization failed!"));
            while (true) { delay(1000); }
        }
        Serial.println(F("OLED initialized"));
        
        // Initialize e-Paper (slow)
        Serial.println(F("Initializing e-Paper (this takes a while)..."));
        if (!epaperDisplay.begin()) {
            Serial.println(F("e-Paper initialization failed!"));
            while (true) { delay(1000); }
        }
        Serial.println(F("e-Paper initialized"));
    #else
        if (!displayDriver.begin()) {
            Serial.println(F("Display initialization failed!"));
            while (true) { delay(1000); }
        }
        Serial.println(F("Display initialized"));
    #endif
    
    // Initialize core modules (uses OLED in dual mode)
    initUtils(&displayDriver);
    initControls();
    initIcons();
    initFileSystem();
    
    // Register custom icons
    REGISTER_ICON("game", gameIcon);
    REGISTER_ICON("music", musicIcon);
    REGISTER_ICON("photo", photoIcon);
    
    // Register application callbacks for built-in apps
    registerAppCallback("About", onAbout);
    registerAppCallback("Storage", onFileExplorer);
    
    // Initialize Lua filesystem and interpreter
    initLuaFS();
    initLua();
    initCppApps();
    
    // Initialize IR remote
    initIRRemote();
    
    // Load assets (loading screen, etc.)
    loadLoadingScreen();
    
    // Load file structure from string
    loadFromString(fileStructure);
    
    // Dynamically add Lua scripts from filesystem
    // Folder structure in /apps/ maps to UI paths:
    // /apps/Applications/*.lua -> /Applications/*
    // /apps/Games/*.lua -> /Games/*
    // /apps/Tools/*.lua -> /Tools/*
    //
    // NOTE: You must upload the filesystem separately with: pio run -t uploadfs
    
    // First, create UI folders from filesystem folders
    // Folder names can have [icon] prefix: "[game]Games" -> icon=game, name=Games
    std::vector<ScriptFolder> folders = listScriptFolders();
    for (const auto& folder : folders) {
        addFolder(folder.uiPath.c_str(), folder.icon.c_str());
        Serial.print(F("Created UI folder: "));
        Serial.print(folder.uiPath);
        Serial.print(F(" ["));
        Serial.print(folder.icon);
        Serial.println(F("]"));
    }
    
    // Now add scripts to their folders
    // Script names can have [icon] prefix: "[game]snake.lua" -> icon=game, name=snake
    std::vector<ScriptInfo> scripts = listAllScripts();
    Serial.print(F("Found "));
    Serial.print(scripts.size());
    Serial.println(F(" scripts on filesystem"));
    
    for (const auto& script : scripts) {
        // Only handle Lua scripts (C++ apps are registered separately)
        if (script.type != ScriptType::LUA) {
            continue;
        }
        
        // Register the UI path -> filesystem path mapping
        registerScriptPath(script.uiPath, script.path);
        
        // Add script as an app with its icon
        addApp(script.uiPath.c_str(), onLuaScript, script.icon.c_str());
        
        Serial.print(F("Added Lua app: "));
        Serial.print(script.name);
        Serial.print(F(" ["));
        Serial.print(script.icon);
        Serial.print(F("] at "));
        Serial.println(script.uiPath);
    }
    
    // Add registered C++ apps to UI
    std::vector<CppAppInfo>& cppApps = getCppApps();
    Serial.print(F("Processing "));
    Serial.print(cppApps.size());
    Serial.println(F(" C++ apps"));
    
    for (const auto& app : cppApps) {
        // Create all parent folders in the path
        String uiPath = app.uiPath;
        
        // Build path progressively and create missing folders
        String currentPath = "";
        int startIdx = 1; // Skip leading /
        
        while (startIdx < (int)uiPath.length()) {
            int nextSlash = uiPath.indexOf('/', startIdx);
            if (nextSlash == -1) {
                // This is the app name, not a folder
                break;
            }
            
            currentPath = uiPath.substring(0, nextSlash);
            
            // Check if folder exists
            FSEntry* folder = getEntry(currentPath);
            if (!folder) {
                // Determine icon based on folder name
                String folderName = uiPath.substring(startIdx, nextSlash);
                const char* icon = "folder";
                if (folderName == "WiFi") icon = "wifi";
                else if (folderName == "Applications") icon = "app";
                else if (folderName == "Games") icon = "game";
                else if (folderName == "Tools") icon = "settings";
                
                Serial.print(F("Creating folder: "));
                Serial.println(currentPath);
                addFolder(currentPath.c_str(), icon);
            }
            
            startIdx = nextSlash + 1;
        }
        
        // Add C++ app
        addApp(app.uiPath.c_str(), onCppApp, "app");
        
        Serial.print(F("Added C++ app: "));
        Serial.print(app.name);
        Serial.print(F(" at "));
        Serial.println(app.uiPath);
    }
    
    // Initialize menu
    initMenu();
    
    Serial.println(F("All modules initialized"));
    Serial.print(F("Total entries: "));
    Serial.println(getEntryCount());
    
    // Initialize the centralized render queue (for apps)
    initRenderQueue();
    
    // Create render task semaphore and task (for menu)
    // Render task uses polling - no semaphore needed
    xTaskCreatePinnedToCore(
        renderTask,       // Task function
        "RenderTask",     // Name
        4096,             // Stack size
        NULL,             // Parameters
        1,                // Priority (low, input handling is more important)
        &renderTaskHandle,// Task handle
        0                 // Run on Core 0 (main loop runs on Core 1)
    );
    Serial.println(F("Render task created on Core 0"));
    
    // Initial render (direct, not via task)
    #if DISPLAY_TYPE == DUAL
        // Render OLED first (instant)
        Serial.println(F("Initial OLED render..."));
        renderMenu(true);
        
        // Then render e-Paper (slow)
        Serial.println(F("Initial e-Paper render..."));
        setLEDBusy();
        
        extern FlipperDisplay* display;
        FlipperDisplay* savedDisplay = display;
        display = &epaperDisplay;
        initUtils(&epaperDisplay);
        renderMenu(true);
        display = savedDisplay;
        initUtils(&oledDisplay);
        
        Serial.println(F("Initial render complete"));
    #elif DISPLAY_TYPE == EPAPER
        // e-Paper only - render with LED indication
        setLEDBusy();
        renderMenu(true);
    #else
        // OLED only
        renderMenu(true);
    #endif
    
    // System ready - LED ON
    setLEDReady();
    Serial.println(F("System ready"));
}

// Track e-Paper update state (for DUAL mode)
#if DISPLAY_TYPE == DUAL
static bool epaperNeedsUpdate = false;  // false because we render in setup()
static unsigned long lastInputTime = 0;
static bool epaperIsUpdating = false;
#define EPAPER_DEBOUNCE_MS 500  // Wait 500ms after last input before updating e-Paper
#endif

// Render task runs on Core 0 (app logic runs on Core 1)
void renderTask(void* parameter) {
    while (true) {
        // Check if render is requested
        if (renderRequested && !isAppRunning()) {
            renderRequested = false;
            acquireDisplayLock();
            setLEDBusy();
            
            #if DISPLAY_TYPE == DUAL
                // Render to OLED (fast)
                renderMenu(true);
            #elif DISPLAY_TYPE == EPAPER
                // e-Paper only mode
                renderMenu(true);
            #else
                // SSD1306 only
                renderMenu(true);
            #endif
            
            setLEDReady();
            releaseDisplayLock();
            // Loop immediately to check for more render requests
        } else {
            // Nothing to render, small yield
            vTaskDelay(1);
        }
    }
}

// Request a render (non-blocking)
void requestRender() {
    renderRequested = true;
    // No semaphore needed - render task polls renderRequested
}

void loop() {
    // Check if an app is running
    if (isAppRunning()) {
        // Run the app's frame
        if (!runAppFrame()) {
            // App exited, redraw menu
            Serial.println(F("App exited, returning to menu"));
            
            // Wait for button to be released before accepting new input
            // This prevents "hold button to exit" from immediately re-entering
            waitForButtonRelease();
            
            #if DISPLAY_TYPE == DUAL
                requestRender();
                epaperNeedsUpdate = true;
                lastInputTime = millis();
            #else
                requestRender();
            #endif
        }
        return;
    }
    
    // Normal menu mode
    // Update control inputs
    updateControls();
    
    bool hadInput = false;
    
    // Handle navigation
    if (isUpPressed()) {
        setLEDBusy();  // LED on while processing input
        menuUp();
        hadInput = true;
    }
    if (isDownPressed()) {
        setLEDBusy();  // LED on while processing input
        menuDown();
        hadInput = true;
    }
    
    // Handle selection on button release
    if (isButtonReleased()) {
        setLEDBusy();  // LED on while processing input
        handleMenuSelection();
        hadInput = true;
    }
    
    // Request render on input (only if no app started)
    if (hadInput && !isAppRunning()) {
        #if DISPLAY_TYPE == DUAL
            // Request OLED render (handled by render task)
            requestRender();
            // Mark e-Paper as needing update, reset debounce timer
            epaperNeedsUpdate = true;
            lastInputTime = millis();
        #elif DISPLAY_TYPE == EPAPER
            // e-Paper only mode
            requestRender();
        #else
            // SSD1306 only
            requestRender();
        #endif
    }
    
    // In DUAL mode: update e-Paper only after user stops navigating
    #if DISPLAY_TYPE == DUAL
        if (epaperNeedsUpdate && !epaperIsUpdating && !isAppRunning()) {
            // Check if enough time has passed since last input
            if (millis() - lastInputTime >= EPAPER_DEBOUNCE_MS) {
                epaperIsUpdating = true;
                acquireDisplayLock();
                setLEDBusy();
                
                Serial.print(F("["));
                Serial.print(millis());
                Serial.println(F("ms] Rendering e-Paper..."));
                
                // Render to e-Paper (this is blocking)
                extern FlipperDisplay* display;
                FlipperDisplay* savedDisplay = display;
                display = &epaperDisplay;
                
                // Re-init utils for e-Paper scale
                initUtils(&epaperDisplay);
                renderMenu(true);
                
                // Restore OLED as primary
                display = savedDisplay;
                initUtils(&oledDisplay);
                
                Serial.print(F("["));
                Serial.print(millis());
                Serial.println(F("ms] e-Paper update complete"));
                
                setLEDReady();
                releaseDisplayLock();
                
                epaperNeedsUpdate = false;
                epaperIsUpdating = false;
            }
        }
    #endif
    
    // No delay needed - ESP32 handles task scheduling
}
