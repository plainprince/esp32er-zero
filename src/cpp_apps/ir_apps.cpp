
#include "cpp_app.h"
#include "ir_remote.h"
#include "keyboard.h"
#include <vector>
#include <string>

#if __has_include("security_config.h")
    #include "security_config.h"
#else
    #define ENABLE_ADVANCED_IR 0
#endif

// Universal Remote App - Improved navigation for custom codes
// Navigation structure:
// - Protocol selection
// - Brand/Folder selection
// - For Custom protocol:
//   - Inside folder: list of codes (as folders) + "New entry" + "Delete folder"
//   - Inside code: "Run" (hold) + "Delete"
// - For regular protocols:
//   - Button list with send on button press

CPP_APP(ir_remote) {
    // State variables
    enum NavLevel { PROTOCOL, BRAND, CODE_LIST, CODE_ACTIONS };
    NavLevel navLevel = PROTOCOL;
    
    int protocolIndex = 0;
    int brandIndex = 0;
    int codeIndex = 0;
    int actionIndex = 0;
    
    bool sending = false;
    bool lastButtonState = false;
    unsigned long lastSendTime = 0;
    unsigned long lastRepeatTime = 0;
    const unsigned long REPEAT_INTERVAL = 110;
    
    char currentFolder[64] = "";
    char currentCodeName[64] = "";
    const TVCode* currentCode = nullptr;
    std::vector<const TVCode*> codes;  // Cached codes list for current folder
    
    // Get available protocols
    const char** protocols = getIRProtocols();
    const int numProtocols = getIRProtocolCount();
    
    // Get TV codes from file
    const TVCode* allCodes = getAllTVCodes();
    const int numCodes = getTVCodeCount();
    
    // Helper: Check if current protocol is Custom
    auto isCustomProtocol = [&]() -> bool {
        if (protocolIndex >= numProtocols || !protocols[protocolIndex]) return false;
        return strcasecmp_eq(protocols[protocolIndex], "Custom");
    };
    
    // Store string objects for custom folders to keep pointers valid
    static std::vector<std::string> customFolderStrings;
    
    // Helper: Get brands for current protocol
    auto getBrands = [&]() -> std::vector<const char*> {
        std::vector<const char*> brands;
        if (protocolIndex >= numProtocols || !protocols[protocolIndex]) return brands;
        
        const char* protoName = protocols[protocolIndex];
        
        if (strcasecmp_eq(protoName, "Custom")) {
            // Store String objects persistently to keep pointers valid
            customFolderStrings = getCustomFolders();
            for (const auto& f : customFolderStrings) {
                brands.push_back(f.c_str());
            }
            return brands;
        }
        
        // Regular protocols - point directly to persistent char arrays in TVCode structs
        for (int i = 0; i < numCodes; i++) {
            if (strcasecmp_eq(allCodes[i].protocol, protoName)) {
                bool found = false;
                for (const auto& b : brands) {
                    if (strcasecmp_eq(b, allCodes[i].brand)) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    brands.push_back(allCodes[i].brand); // Point directly to char array
                }
            }
        }
        return brands;
    };
    
    // Helper: Get codes for brand
    auto getCodesForBrand = [&](const char* brand) -> std::vector<const TVCode*> {
        std::vector<const TVCode*> codes;
        const char* protoName = protocols[protocolIndex];
        
        if (strcasecmp_eq(protoName, "Custom")) {
            return getCustomCodes(brand);
        }
        
        for (int i = 0; i < numCodes; i++) {
            if (strcasecmp_eq(allCodes[i].protocol, protoName) &&
                strcasecmp_eq(allCodes[i].brand, brand)) {
                codes.push_back(&allCodes[i]);
            }
        }
        return codes;
    };
    
    // Render function
    auto render = [&]() {
        CppApp::clear();
        CppApp::println("IR Remote");
        CppApp::println("");
        
        char buf[80];
        
        if (navLevel == PROTOCOL) {
            // Protocol selection
            CppApp::println("Protocol:");
            if (protocolIndex < numProtocols && protocols[protocolIndex]) {
                snprintf(buf, sizeof(buf), "> %s", protocols[protocolIndex]);
                CppApp::println(buf);
            }
            CppApp::println("");
            CppApp::println("Up/Down: select");
            CppApp::println("Right: next");
            CppApp::println("Left: exit");
            
        } else if (navLevel == BRAND) {
            // Brand/Folder selection
            std::vector<const char*> brands = getBrands();
            bool isCustom = isCustomProtocol();
            int maxIndex = brands.size() + (isCustom ? 1 : 0);
            
            if (brandIndex >= maxIndex) brandIndex = maxIndex > 0 ? maxIndex - 1 : 0;
            
            CppApp::println(isCustom ? "Folders:" : "Brand:");
            
            for (int i = 0; i < (int)brands.size() && i < 5; i++) {
                if (i == brandIndex) {
                    snprintf(buf, sizeof(buf), "> %s", brands[i]);
                } else {
                    snprintf(buf, sizeof(buf), "  %s", brands[i]);
                }
                CppApp::println(buf);
            }
            
            if (isCustom && brandIndex == (int)brands.size()) {
                CppApp::println("> Create new");
            } else if (isCustom) {
                CppApp::println("  Create new");
            }
            
            if (maxIndex > 5) {
                snprintf(buf, sizeof(buf), "(%d/%d)", brandIndex + 1, maxIndex);
                CppApp::println(buf);
            }
            
            CppApp::println("");
            CppApp::println("Right: open");
            CppApp::println("Left: back");
            
        } else if (navLevel == CODE_LIST) {
            // Inside folder/brand - list codes
            std::vector<const TVCode*> codes = getCodesForBrand(currentFolder);
            bool isCustom = isCustomProtocol();
            
            // For custom: codes + "New entry" + "Delete folder"
            // For regular: codes only
            int numOptions = codes.size() + (isCustom ? 2 : 0);
            if (codeIndex >= numOptions) codeIndex = numOptions > 0 ? numOptions - 1 : 0;
            
            CppApp::println(isCustom ? "Codes:" : "Buttons:");
            
            for (int i = 0; i < (int)codes.size() && i < 5; i++) {
                if (i == codeIndex) {
                    snprintf(buf, sizeof(buf), "> %s", codes[i]->button);
                } else {
                    snprintf(buf, sizeof(buf), "  %s", codes[i]->button);
                }
                CppApp::println(buf);
            }
            
            if (isCustom) {
                if (codeIndex == (int)codes.size()) {
                    CppApp::println("> New entry");
                } else {
                    CppApp::println("  New entry");
                }
                
                if (codeIndex == (int)codes.size() + 1) {
                    CppApp::println("> Delete folder");
                } else {
                    CppApp::println("  Delete folder");
                }
            }
            
            if (numOptions > 5) {
                snprintf(buf, sizeof(buf), "(%d/%d)", codeIndex + 1, numOptions);
                CppApp::println(buf);
            }
            
            CppApp::println("");
            if (isCustom) {
                CppApp::println("Right: open/action");
            } else {
                CppApp::println("Button: send");
            }
            CppApp::println("Left: back");
            
        } else if (navLevel == CODE_ACTIONS) {
            // Inside a code - show Run, Delete, Show info
            CppApp::println("Actions:");
            CppApp::println(currentCodeName);
            CppApp::println("");
            
            if (actionIndex == 0) {
                CppApp::println("> Run (hold)");
            } else {
                CppApp::println("  Run (hold)");
            }
            
            if (actionIndex == 1) {
                CppApp::println("> Delete");
            } else {
                CppApp::println("  Delete");
            }
            
            if (actionIndex == 2) {
                CppApp::println("> Show info");
            } else {
                CppApp::println("  Show info");
            }
            
            CppApp::println("");
            CppApp::println("Button: execute");
            CppApp::println("Left: back");
        }
        
        if (sending && (millis() - lastSendTime < 200)) {
            CppApp::println("");
            CppApp::println("Sending...");
        }
        
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
            if (navLevel == PROTOCOL) {
                CppApp::exit();
                break;
            } else if (navLevel == BRAND) {
                navLevel = PROTOCOL;
                render();
            } else if (navLevel == CODE_LIST) {
                navLevel = BRAND;
                codeIndex = 0;
                render();
            } else if (navLevel == CODE_ACTIONS) {
                navLevel = CODE_LIST;
                actionIndex = 0;
                render();
            }
        }
        
        if (navLevel == PROTOCOL) {
            if (upPressed) {
                protocolIndex = (protocolIndex - 1 + numProtocols) % numProtocols;
                brandIndex = 0;
                render();
            }
            if (downPressed) {
                protocolIndex = (protocolIndex + 1) % numProtocols;
                brandIndex = 0;
                render();
            }
            if (rightPressed) {
                std::vector<const char*> brands = getBrands();
                if (brands.size() > 0 || isCustomProtocol()) {
                    navLevel = BRAND;
                    brandIndex = 0;
                    render();
                }
            }
            
        } else if (navLevel == BRAND) {
            std::vector<const char*> brands = getBrands();
            bool isCustom = isCustomProtocol();
            int maxIndex = brands.size() + (isCustom ? 1 : 0);
            
            if (upPressed && maxIndex > 0) {
                brandIndex = (brandIndex - 1 + maxIndex) % maxIndex;
                render();
            }
            if (downPressed && maxIndex > 0) {
                brandIndex = (brandIndex + 1) % maxIndex;
                render();
            }
            if (rightPressed) {
                if (isCustom && brandIndex == (int)brands.size()) {
                    // Create new folder
                    char folderName[64] = "";
                    if (showKeyboard("Folder name:", folderName, sizeof(folderName))) {
                        // Clean folder name: remove newline and trim whitespace
                        char cleanName[64] = "";
                        size_t len = strlen(folderName);
                        size_t cleanLen = 0;
                        for (size_t i = 0; i < len && i < sizeof(cleanName) - 1; i++) {
                            if (folderName[i] == '\n' || folderName[i] == '\r') break;
                            if (folderName[i] != ' ' || cleanLen > 0) { // Skip leading spaces
                                cleanName[cleanLen++] = folderName[i];
                            }
                        }
                        // Trim trailing spaces
                        while (cleanLen > 0 && cleanName[cleanLen - 1] == ' ') {
                            cleanLen--;
                        }
                        cleanName[cleanLen] = '\0';
                        
                        if (cleanLen > 0) {
                            if (createCustomFolder(cleanName)) {
                                // Reload brands and select the new folder
                                brands = getBrands();
                                for (size_t i = 0; i < brands.size(); i++) {
                                    if (strcasecmp_eq(brands[i], cleanName)) {
                                        brandIndex = i;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    render();
                } else if (brandIndex < (int)brands.size()) {
                    strncpy(currentFolder, brands[brandIndex], sizeof(currentFolder) - 1);
                    currentFolder[sizeof(currentFolder) - 1] = '\0';
                    navLevel = CODE_LIST;
                    codeIndex = 0;
                    render();
                }
            }
            
        } else if (navLevel == CODE_LIST) {
            // Refresh codes list each time we enter this section
            codes = getCodesForBrand(currentFolder);
            bool isCustom = isCustomProtocol();
            int numOptions = codes.size() + (isCustom ? 2 : 0);
            
            if (upPressed && numOptions > 0) {
                codeIndex = (codeIndex - 1 + numOptions) % numOptions;
                render();
            }
            if (downPressed && numOptions > 0) {
                codeIndex = (codeIndex + 1) % numOptions;
                render();
            }
            
            if (isCustom) {
                // Custom protocol - navigate or take action
                if (rightPressed || btnJustPressed) {
                    if (codeIndex == (int)codes.size()) {
                        // New entry - scan code
                        CppApp::clear();
                        CppApp::println("Scanning...");
                        CppApp::println("");
                        CppApp::println("Point remote");
                        CppApp::println("at receiver");
                        CppApp::refresh();
                        
                        TVCode scannedCode;
                        char protocolName[32] = "";
                        if (scanIRCode(10000, &scannedCode, protocolName, sizeof(protocolName))) {
                            char buttonName[64] = "";
                            if (showKeyboard("Button name:", buttonName, sizeof(buttonName))) {
                                // Clean button name: remove newline and trim whitespace
                                char cleanName[64] = "";
                                size_t len = strlen(buttonName);
                                size_t cleanLen = 0;
                                for (size_t i = 0; i < len && i < sizeof(cleanName) - 1; i++) {
                                    if (buttonName[i] == '\n' || buttonName[i] == '\r') break;
                                    if (buttonName[i] != ' ' || cleanLen > 0) { // Skip leading spaces
                                        cleanName[cleanLen++] = buttonName[i];
                                    }
                                }
                                // Trim trailing spaces
                                while (cleanLen > 0 && cleanName[cleanLen - 1] == ' ') {
                                    cleanLen--;
                                }
                                cleanName[cleanLen] = '\0';
                                
                                if (cleanLen > 0) {
                                    if (saveCustomCode(currentFolder, cleanName, &scannedCode)) {
                                        // Successfully saved - refresh codes list and select the new code
                                        codes = getCodesForBrand(currentFolder);
                                        codeIndex = codes.size() - 1;  // Select the newly added code
                                    }
                                }
                            }
                        } else {
                            CppApp::clear();
                            CppApp::println("Scan failed");
                            CppApp::println("");
                            CppApp::println("Timeout or");
                            CppApp::println("no signal");
                            CppApp::refresh();
                            delay(2000);
                        }
                        render();
                        
                    } else if (codeIndex == (int)codes.size() + 1) {
                        // Delete folder
                        deleteCustomFolder(currentFolder);
                        navLevel = BRAND;
                        codeIndex = 0;
                        render();
                        
                    } else if (codeIndex < (int)codes.size()) {
                        // Enter code actions
                        strncpy(currentCodeName, codes[codeIndex]->button, sizeof(currentCodeName) - 1);
                        currentCodeName[sizeof(currentCodeName) - 1] = '\0';
                        currentCode = codes[codeIndex];
                        navLevel = CODE_ACTIONS;
                        actionIndex = 0;
                        render();
                    }
                }
            } else {
                // Regular protocol - send on button press
                if (btnJustPressed && codeIndex < (int)codes.size()) {
                    sending = true;
                    lastSendTime = millis();
                    lastRepeatTime = millis();
                    sendTVCode(codes[codeIndex]);
                    render();
                }
                
                // Continue sending while held
                if (btnHeld && sending && codeIndex < (int)codes.size() &&
                    (millis() - lastRepeatTime >= REPEAT_INTERVAL)) {
                    lastRepeatTime = millis();
                    lastSendTime = millis();
                    sendTVCode(codes[codeIndex]);
                }
            }
            
        } else if (navLevel == CODE_ACTIONS) {
            if (upPressed) {
                actionIndex = (actionIndex - 1 + 3) % 3;
                render();
            }
            if (downPressed) {
                actionIndex = (actionIndex + 1) % 3;
                render();
            }
            
            if (actionIndex == 0) {
                // Run - hold to send
                if (btnJustPressed) {
                    sending = true;
                    lastSendTime = millis();
                    lastRepeatTime = millis();
                    sendTVCode(currentCode);
                    render();
                }
                
                if (btnHeld && sending && (millis() - lastRepeatTime >= REPEAT_INTERVAL)) {
                    lastRepeatTime = millis();
                    lastSendTime = millis();
                    sendTVCode(currentCode);
                }
            } else if (actionIndex == 1 && btnJustPressed) {
                // Delete
                deleteCustomCode(currentFolder, currentCodeName);
                navLevel = CODE_LIST;
                codeIndex = 0;
                actionIndex = 0;
                render();
            } else if (actionIndex == 2 && btnJustPressed) {
                // Show info - display full details
                CppApp::clear();
                
                // Code Name
                CppApp::println(currentCodeName);
                
                if (currentCode) {
                    char buf[64];
                    // Combine info to save vertical space
                    snprintf(buf, sizeof(buf), "P:%s  %dbits", currentCode->protocol, currentCode->nbits);
                    CppApp::println(buf);
                    
                    snprintf(buf, sizeof(buf), "B:%s", currentCode->brand);
                    CppApp::println(buf);
                    
                    // Use explicit casting and format for 32-bit/64-bit values
                    // Using %04llX to force at least 4 digits (pads with zeros)
                    snprintf(buf, sizeof(buf), "A:0x%04llX", (unsigned long long)currentCode->address);
                    CppApp::println(buf);
                    
                    snprintf(buf, sizeof(buf), "C:0x%04llX", (unsigned long long)currentCode->command);
                    CppApp::println(buf);
                }
                
                // Empty line for spacing
                CppApp::println("");
                CppApp::println("Click to exit");
                
                CppApp::refresh();
                
                // Wait for button release first
                while (CppApp::buttonRaw()) {
                    delay(10);
                }
                
                // Now wait for any button press
                while (!CppApp::shouldExit()) {
                    if (CppApp::buttonRaw() || CppApp::up() || CppApp::down() || 
                        CppApp::left() || CppApp::right()) {
                        delay(100); // Debounce
                        break;
                    }
                    delay(10);
                }
                
                render();
            }
        }
        
        // Stop sending when button released
        if (!btnHeld && sending) {
            sending = false;
            render();
        }
        
        lastButtonState = btnHeld;
        CppApp::waitFrame(50);
    }
}

REGISTER_CPP_APP(ir_remote, "/Applications/Infrared/Universal Remote");

// Simple IR Test App
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
    // TV-B-Gone is now implemented in Lua
#endif
}
