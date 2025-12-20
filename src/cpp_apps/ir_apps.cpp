
#include "cpp_app.h"
#include "ir_remote.h"

#if __has_include("security_config.h")
    #include "security_config.h"
#else
    #define ENABLE_ADVANCED_IR 0
#endif

// Universal Remote App
// Supports common IR protocols: NEC, RC5, Sony
// Includes preset codes for common TV brands

CPP_APP(ir_remote) {
    int menuIndex = 0;
    int protocolIndex = 0;
    int deviceIndex = 0;
    bool sending = false;
    bool lastButtonState = false;
    unsigned long lastSendTime = 0;
    unsigned long lastRepeatTime = 0;
    const unsigned long REPEAT_INTERVAL = 80; // Time between IR repeats in ms (faster repeat for better responsiveness)
    
    // Protocol names
    const char* protocols[] = {"NEC", "RC5", "Sony"};
    const int numProtocols = 3;
    
    // Common TV remote codes (NEC format: address, command)
    struct TVCode {
        const char* name;
        uint32_t address;
        uint32_t command;
    };
    
    TVCode tvCodes[] = {
        {"Power", 0x00FF, 0x00FF},
        {"Vol+", 0x00FF, 0x00FE},
        {"Vol-", 0x00FF, 0x00FD},
        {"Ch+", 0x00FF, 0x00FC},
        {"Ch-", 0x00FF, 0x00FB},
        {"Mute", 0x00FF, 0x00FA},
        {"Input", 0x00FF, 0x00F9},
        {"Menu", 0x00FF, 0x00F8},
        {"Up", 0x00FF, 0x00F7},
        {"Down", 0x00FF, 0x00F6},
        {"Left", 0x00FF, 0x00F5},
        {"Right", 0x00FF, 0x00F4},
        {"OK", 0x00FF, 0x00F3},
        {"Back", 0x00FF, 0x00F2},
        {"1", 0x00FF, 0x00E1},
        {"2", 0x00FF, 0x00E2},
        {"3", 0x00FF, 0x00E3},
        {"4", 0x00FF, 0x00E4},
        {"5", 0x00FF, 0x00E5},
        {"6", 0x00FF, 0x00E6},
        {"7", 0x00FF, 0x00E7},
        {"8", 0x00FF, 0x00E8},
        {"9", 0x00FF, 0x00E9},
        {"0", 0x00FF, 0x00EA},
    };
    const int numTVCodes = sizeof(tvCodes) / sizeof(tvCodes[0]);
    
    auto render = [&]() {
        CppApp::clear();
        CppApp::println("IR Remote");
        CppApp::println("");
        
        char buf[50];
        
        if (menuIndex == 0) {
            // Protocol selection
            CppApp::println("Protocol:");
            snprintf(buf, sizeof(buf), "> %s", protocols[protocolIndex]);
            CppApp::println(buf);
            CppApp::println("");
            CppApp::println("Up/Down: select");
            CppApp::println("Right: next");
            CppApp::println("Button: send test");
        } else if (menuIndex == 1) {
            // TV remote codes
            CppApp::println("TV Remote:");
            int startIdx = deviceIndex;
            int endIdx = startIdx + 5;
            if (endIdx > numTVCodes) endIdx = numTVCodes;
            
            for (int i = startIdx; i < endIdx; i++) {
                if (i == deviceIndex) {
                    snprintf(buf, sizeof(buf), "> %s", tvCodes[i].name);
                } else {
                    snprintf(buf, sizeof(buf), "  %s", tvCodes[i].name);
                }
                CppApp::println(buf);
            }
            
            if (numTVCodes > 5) {
                snprintf(buf, sizeof(buf), "(%d/%d)", deviceIndex + 1, numTVCodes);
                CppApp::println(buf);
            }
            
            CppApp::println("");
            CppApp::println("Up/Down: navigate");
            CppApp::println("Button: send");
        } else if (menuIndex == 2) {
            // Custom code entry
            CppApp::println("Custom Code:");
            CppApp::println("(Not implemented)");
            CppApp::println("");
            CppApp::println("Use TV Remote");
            CppApp::println("for presets");
        }
        
        if (sending && (millis() - lastSendTime < 200)) {
            CppApp::println("");
            CppApp::println("Sending...");
        }
        
        CppApp::println("");
        CppApp::println("Left: back/exit");
        
        CppApp::refresh();
    };
    
    render();
    
    while (!CppApp::shouldExit()) {
        bool upPressed = CppApp::up();
        bool downPressed = CppApp::down();
        bool leftPressed = CppApp::left();
        bool rightPressed = CppApp::right();
        bool btnHeld = CppApp::buttonRaw();
        bool btnJustPressed = btnHeld && !lastButtonState;
        
        if (leftPressed) {
            if (menuIndex > 0) {
                menuIndex--;
                render();
            } else {
                CppApp::exit();
                break;
            }
        }
        
        if (menuIndex == 0) {
            // Protocol selection
            if (upPressed) {
                protocolIndex = (protocolIndex - 1 + numProtocols) % numProtocols;
                render();
            }
            if (downPressed) {
                protocolIndex = (protocolIndex + 1) % numProtocols;
                render();
            }
            if (rightPressed) {
                menuIndex = 1;
                render();
            }
            if (btnJustPressed) {
                // Send test code on initial press
                sending = true;
                lastSendTime = millis();
                lastRepeatTime = millis();
                switch (protocolIndex) {
                    case 0: // NEC
                        sendNEC(0x00FF, 0x00FF);
                        break;
                    case 1: // RC5
                        sendRC5(0x00, 0x0C);
                        break;
                    case 2: // Sony
                        sendSony(0x1CE1, 12);
                        break;
                }
                render();
            }
            // Continue sending while button is held
            if (btnHeld && sending && (millis() - lastRepeatTime >= REPEAT_INTERVAL)) {
                lastRepeatTime = millis();
                lastSendTime = millis();
                switch (protocolIndex) {
                    case 0: // NEC
                        sendNEC(0x00FF, 0x00FF);
                        break;
                    case 1: // RC5
                        sendRC5(0x00, 0x0C);
                        break;
                    case 2: // Sony
                        sendSony(0x1CE1, 12);
                        break;
                }
            }
        } else if (menuIndex == 1) {
            // TV remote codes
            if (upPressed) {
                deviceIndex = (deviceIndex - 1 + numTVCodes) % numTVCodes;
                render();
            }
            if (downPressed) {
                deviceIndex = (deviceIndex + 1) % numTVCodes;
                render();
            }
            if (btnJustPressed) {
                // Send selected code on initial press
                sending = true;
                lastSendTime = millis();
                lastRepeatTime = millis();
                sendNEC(tvCodes[deviceIndex].address, tvCodes[deviceIndex].command);
                render();
            }
            // Continue sending while button is held
            if (btnHeld && sending && (millis() - lastRepeatTime >= REPEAT_INTERVAL)) {
                lastRepeatTime = millis();
                lastSendTime = millis();
                sendNEC(tvCodes[deviceIndex].address, tvCodes[deviceIndex].command);
            }
        }
        
        // Stop sending when button is released
        if (!btnHeld && sending) {
            sending = false;
            render();
        }
        
        lastButtonState = btnHeld;
        CppApp::waitFrame(50);
    }
}

REGISTER_CPP_APP(ir_remote, "/Applications/Infrared/Universal Remote");

// Simple IR Test App - sends a test pulse
CPP_APP(ir_test) {
    int state = 0;
    unsigned long lastAction = 0;
    
    auto render = [&]() {
        CppApp::clear();
        CppApp::println("IR Test");
        CppApp::println("");
        
        switch (state) {
            case 0:
                CppApp::println("Ready");
                CppApp::println("");
                CppApp::println("Button: send");
                CppApp::println("pulse");
                break;
            case 1:
                CppApp::println("Sending...");
                CppApp::println("");
                CppApp::println("IR pulse");
                CppApp::println("sent!");
                break;
        }
        
        CppApp::println("");
        CppApp::println("Left: exit");
        
        CppApp::refresh();
    };
    
    render();
    
    while (!CppApp::shouldExit()) {
        bool btnPressed = CppApp::button();
        bool leftPressed = CppApp::left();
        
        if (leftPressed) {
            CppApp::exit();
            break;
        }
        
        if (btnPressed && state == 0) {
            state = 1;
            lastAction = millis();
            // Send a simple test pulse
            sendIRPulse(9000, 4500, 38000);
            render();
        }
        
        if (state == 1 && (millis() - lastAction > 1000)) {
            state = 0;
            render();
        }
        
        CppApp::waitFrame(50);
    }
}

REGISTER_CPP_APP(ir_test, "/Applications/Infrared/IR Test");

void registerIRApps() {
#if ENABLE_ADVANCED_IR
    registerCppApp("Universal Remote", "/Applications/Infrared/Universal Remote", cppapp_ir_remote);
    registerCppApp("IR Test", "/Applications/Infrared/IR Test", cppapp_ir_test);
#endif
}

