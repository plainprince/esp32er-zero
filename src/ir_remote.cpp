#include "ir_remote.h"
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRutils.h>
#include "filesystem.h"
#include <LittleFS.h>
#include <vector>
#include <string>

static IRsend irsend(IR_LED_PIN);
static IRrecv irrecv(IR_RECEIVER_PIN);
static std::vector<TVCode> tvCodes;
static std::vector<const char*> protocolNames;
static char discoveredProtocols[32][64]; // Fixed array for protocol names (max 32 protocols, 64 chars each)
static int discoveredProtocolCount = 0;
static std::vector<const char*> protocolPtrs; // C-style array for return
static std::vector<TVCode> customCodes; // Custom scanned codes

// Forward declarations
uint64_t parseHex(const char* str);

// Case-insensitive string comparison helper (no String allocation)
bool strcasecmp_eq(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) {
        char ca = (*a >= 'a' && *a <= 'z') ? (*a - 32) : *a;
        char cb = (*b >= 'a' && *b <= 'z') ? (*b - 32) : *b;
        if (ca != cb) return false;
        a++;
        b++;
    }
    return *a == *b;
}

// Helper function to convert protocol name string to decode_type_t
// Optimized to avoid String allocations - prevents memory fragmentation when called rapidly
decode_type_t protocolNameToType(const char* protocol) {
    if (!protocol) return UNKNOWN;
    
    // Map protocol names to decode_type_t enum values (case-insensitive, no String allocation)
    // Common TV remote protocols
    if (strcasecmp_eq(protocol, "NEC")) return NEC;
    if (strcasecmp_eq(protocol, "SONY")) return SONY;
    if (strcasecmp_eq(protocol, "RC5")) return RC5;
    if (strcasecmp_eq(protocol, "RC6")) return RC6;
    if (strcasecmp_eq(protocol, "SAMSUNG")) return SAMSUNG;
    if (strcasecmp_eq(protocol, "LG")) return LG;
    if (strcasecmp_eq(protocol, "PANASONIC")) return PANASONIC;
    if (strcasecmp_eq(protocol, "SHARP")) return SHARP;
    if (strcasecmp_eq(protocol, "JVC")) return JVC;
    if (strcasecmp_eq(protocol, "DENON")) return DENON;
    if (strcasecmp_eq(protocol, "DISH")) return DISH;
    if (strcasecmp_eq(protocol, "PIONEER")) return PIONEER;
    if (strcasecmp_eq(protocol, "LG2")) return LG2;
    if (strcasecmp_eq(protocol, "SAMSUNG36")) return SAMSUNG36;
    if (strcasecmp_eq(protocol, "NEC_LIKE")) return NEC_LIKE;
    
    // AC protocols (less common for TV remotes) - only include if available
    #ifdef DECODE_MITSUBISHI
    if (strcasecmp_eq(protocol, "MITSUBISHI")) return MITSUBISHI;
    #endif
    #ifdef DECODE_COOLIX
    if (strcasecmp_eq(protocol, "COOLIX")) return COOLIX;
    #endif
    #ifdef DECODE_DAIKIN
    if (strcasecmp_eq(protocol, "DAIKIN")) return DAIKIN;
    #endif
    #ifdef DECODE_GREE
    if (strcasecmp_eq(protocol, "GREE")) return GREE;
    #endif
    #ifdef DECODE_TOSHIBA_AC
    if (strcasecmp_eq(protocol, "TOSHIBA_AC")) return TOSHIBA_AC;
    #endif
    
    return UNKNOWN;
}

void loadCustomCodes() {
    customCodes.clear();
    
    if (!LittleFS.exists("/assets/custom_codes")) {
        return;
    }
    
    File root = LittleFS.open("/assets/custom_codes");
    if (!root || !root.isDirectory()) {
        return;
    }
    
    File file = root.openNextFile();
    char lineBuf[512]; // Fixed buffer for line reading
    char fullPathBuf[256]; // Fixed buffer for file paths
    char nameBuf[128]; // Fixed buffer for file names
    
    while (file) {
        if (!file.isDirectory()) {
            const char* name = file.name();
            strncpy(nameBuf, name, sizeof(nameBuf) - 1);
            nameBuf[sizeof(nameBuf) - 1] = '\0';
            
            size_t nameLen = strlen(nameBuf);
            bool endsWithTxt = nameLen >= 4 && strcmp(nameBuf + nameLen - 4, ".txt") == 0;
            bool endsWithGitkeep = nameLen >= 8 && strcmp(nameBuf + nameLen - 8, ".gitkeep") == 0;
            
            if (endsWithTxt && !endsWithGitkeep) {
                // Extract filename from path
                const char* filename = strrchr(name, '/');
                if (filename) filename++;
                else filename = name;
                
                snprintf(fullPathBuf, sizeof(fullPathBuf), "/assets/custom_codes/%s", filename);
                File codeFile = LittleFS.open(fullPathBuf, "r");
                
                if (codeFile) {
                    while (codeFile.available()) {
                        size_t bytesRead = codeFile.readBytesUntil('\n', lineBuf, sizeof(lineBuf) - 1);
                        lineBuf[bytesRead] = '\0';
                        
                        // Trim whitespace
                        size_t start = 0;
                        while (start < bytesRead && (lineBuf[start] == ' ' || lineBuf[start] == '\t' || lineBuf[start] == '\r')) {
                            start++;
                        }
                        size_t end = bytesRead;
                        while (end > start && (lineBuf[end-1] == ' ' || lineBuf[end-1] == '\t' || lineBuf[end-1] == '\r')) {
                            end--;
                        }
                        lineBuf[end] = '\0';
                        const char* line = lineBuf + start;
                        
                        if (end - start == 0 || line[0] == '#') continue;
                        
                        int colons[5];
                        int colonCount = 0;
                        for (int i = 0; i < (int)(end - start) && colonCount < 5; i++) {
                            if (line[i] == ':') {
                                colons[colonCount++] = i;
                            }
                        }
                        
                        if (colonCount == 5) {
                            TVCode code;
                            // Extract substrings to fixed buffers
                            char protocolBuf[64], brandBuf[64], buttonBuf[64];
                            char addrBuf[64], cmdBuf[64], nbitsBuf[32];
                            
                            size_t protocolLen = colons[0];
                            size_t brandLen = colons[1] - colons[0] - 1;
                            size_t buttonLen = colons[2] - colons[1] - 1;
                            size_t addrLen = colons[3] - colons[2] - 1;
                            size_t cmdLen = colons[4] - colons[3] - 1;
                            size_t nbitsLen = (end - start) - colons[4] - 1;
                            
                            if (protocolLen >= sizeof(protocolBuf)) protocolLen = sizeof(protocolBuf) - 1;
                            if (brandLen >= sizeof(brandBuf)) brandLen = sizeof(brandBuf) - 1;
                            if (buttonLen >= sizeof(buttonBuf)) buttonLen = sizeof(buttonBuf) - 1;
                            if (addrLen >= sizeof(addrBuf)) addrLen = sizeof(addrBuf) - 1;
                            if (cmdLen >= sizeof(cmdBuf)) cmdLen = sizeof(cmdBuf) - 1;
                            if (nbitsLen >= sizeof(nbitsBuf)) nbitsLen = sizeof(nbitsBuf) - 1;
                            
                            strncpy(protocolBuf, line, protocolLen);
                            protocolBuf[protocolLen] = '\0';
                            strncpy(brandBuf, line + colons[0] + 1, brandLen);
                            brandBuf[brandLen] = '\0';
                            strncpy(buttonBuf, line + colons[1] + 1, buttonLen);
                            buttonBuf[buttonLen] = '\0';
                            strncpy(addrBuf, line + colons[2] + 1, addrLen);
                            addrBuf[addrLen] = '\0';
                            strncpy(cmdBuf, line + colons[3] + 1, cmdLen);
                            cmdBuf[cmdLen] = '\0';
                            strncpy(nbitsBuf, line + colons[4] + 1, nbitsLen);
                            nbitsBuf[nbitsLen] = '\0';
                            
                            strncpy(code.protocol, protocolBuf, sizeof(code.protocol) - 1);
                            code.protocol[sizeof(code.protocol) - 1] = '\0';
                            strncpy(code.brand, brandBuf, sizeof(code.brand) - 1);
                            code.brand[sizeof(code.brand) - 1] = '\0';
                            strncpy(code.button, buttonBuf, sizeof(code.button) - 1);
                            code.button[sizeof(code.button) - 1] = '\0';
                            code.address = parseHex(addrBuf);
                            code.command = parseHex(cmdBuf);
                            code.nbits = (uint16_t)atoi(nbitsBuf);
                            
                            customCodes.push_back(code);
                        }
                    }
                    codeFile.close();
                }
            }
        }
        file.close();
        file = root.openNextFile();
    }
    root.close();
    
    Serial.print(F("Loaded "));
    Serial.print(customCodes.size());
    Serial.println(F(" custom IR codes"));
}

static bool tvCodesLoaded = false;
static bool customCodesLoaded = false;

static void ensureCustomCodesLoaded() {
    if (!customCodesLoaded) {
        loadCustomCodes();
        customCodesLoaded = true;
    }
}

void initIRRemote() {
    irsend.begin();
    pinMode(IR_LED_PIN, OUTPUT);
    digitalWrite(IR_LED_PIN, LOW);
    Serial.println(F("IR Remote initialized on GPIO 25"));
    
    // Ensure directory structure exists for custom codes
    if (!LittleFS.exists("/assets")) {
        LittleFS.mkdir("/assets");
        Serial.println(F("Created /assets directory"));
    }
    if (!LittleFS.exists("/assets/custom_codes")) {
        LittleFS.mkdir("/assets/custom_codes");
        Serial.println(F("Created /assets/custom_codes directory"));
    }
    
    // Initialize protocol names list (common protocols)
    protocolNames.clear();
    protocolNames.push_back("NEC");
    protocolNames.push_back("SONY");
    protocolNames.push_back("RC5");
    protocolNames.push_back("RC6");
    protocolNames.push_back("SAMSUNG");
    protocolNames.push_back("LG");
    protocolNames.push_back("PANASONIC");
    protocolNames.push_back("SHARP");
    protocolNames.push_back("JVC");
    
    // NOTE: TV codes and custom codes are now loaded lazily when first accessed
    // This saves significant RAM at startup (~50KB+ depending on code count)
    
    // Initialize IR receiver
    initIRReceiver();
}

// Parse hex string (with or without 0x prefix) - uses fixed buffer to avoid String allocation
uint64_t parseHex(const char* str) {
    if (!str) return 0;
    
    char buf[64]; // Fixed buffer for hex parsing
    size_t len = strlen(str);
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    
    // Trim and copy
    size_t start = 0;
    while (start < len && (str[start] == ' ' || str[start] == '\t' || str[start] == '\r' || str[start] == '\n')) {
        start++;
    }
    size_t end = len;
    while (end > start && (str[end-1] == ' ' || str[end-1] == '\t' || str[end-1] == '\r' || str[end-1] == '\n')) {
        end--;
    }
    
    size_t copyLen = end - start;
    if (copyLen >= sizeof(buf)) copyLen = sizeof(buf) - 1;
    strncpy(buf, str + start, copyLen);
    buf[copyLen] = '\0';
    
    // Convert to uppercase
    for (size_t i = 0; i < copyLen; i++) {
        if (buf[i] >= 'a' && buf[i] <= 'z') {
            buf[i] = buf[i] - 32;
        }
    }
    
    // Skip 0x prefix
    const char* hexStr = buf;
    if (copyLen >= 2 && buf[0] == '0' && (buf[1] == 'x' || buf[1] == 'X')) {
        hexStr = buf + 2;
    }
    
    return strtoull(hexStr, nullptr, 16);
}

bool loadTVCodes() {
    tvCodes.clear();
    discoveredProtocolCount = 0;
    protocolPtrs.clear();
    
    File file = LittleFS.open("/assets/tv_codes.txt", "r");
    if (!file) {
        Serial.println(F("Warning: Could not open /assets/tv_codes.txt"));
        return false;
    }
    
    int lineNum = 0;
    char lineBuf[512]; // Fixed buffer for line reading
    
    while (file.available()) {
        lineNum++;
        size_t bytesRead = file.readBytesUntil('\n', lineBuf, sizeof(lineBuf) - 1);
        lineBuf[bytesRead] = '\0';
        
        // Trim whitespace
        size_t start = 0;
        while (start < bytesRead && (lineBuf[start] == ' ' || lineBuf[start] == '\t' || lineBuf[start] == '\r')) {
            start++;
        }
        size_t end = bytesRead;
        while (end > start && (lineBuf[end-1] == ' ' || lineBuf[end-1] == '\t' || lineBuf[end-1] == '\r')) {
            end--;
        }
        lineBuf[end] = '\0';
        const char* line = lineBuf + start;
        
        // Skip empty lines and comments
        if (end - start == 0 || line[0] == '#') {
            continue;
        }
        
        // Parse format: protocol:brand:button:address:command:nbits
        int colons[5];
        int colonCount = 0;
        for (int i = 0; i < (int)(end - start) && colonCount < 5; i++) {
            if (line[i] == ':') {
                colons[colonCount++] = i;
            }
        }
        
        if (colonCount != 5) {
            Serial.print(F("Warning: Invalid format at line "));
            Serial.print(lineNum);
            Serial.print(F(": "));
            Serial.println(line);
            continue;
        }
        
        TVCode code;
        // Extract substrings to fixed buffers to avoid String allocations
        char protocolBuf[64], brandBuf[64], buttonBuf[64];
        char addrBuf[64], cmdBuf[64], nbitsBuf[32];
        
        size_t protocolLen = colons[0];
        size_t brandLen = colons[1] - colons[0] - 1;
        size_t buttonLen = colons[2] - colons[1] - 1;
        size_t addrLen = colons[3] - colons[2] - 1;
        size_t cmdLen = colons[4] - colons[3] - 1;
        size_t nbitsLen = (end - start) - colons[4] - 1;
        
        if (protocolLen >= sizeof(protocolBuf)) protocolLen = sizeof(protocolBuf) - 1;
        if (brandLen >= sizeof(brandBuf)) brandLen = sizeof(brandBuf) - 1;
        if (buttonLen >= sizeof(buttonBuf)) buttonLen = sizeof(buttonBuf) - 1;
        if (addrLen >= sizeof(addrBuf)) addrLen = sizeof(addrBuf) - 1;
        if (cmdLen >= sizeof(cmdBuf)) cmdLen = sizeof(cmdBuf) - 1;
        if (nbitsLen >= sizeof(nbitsBuf)) nbitsLen = sizeof(nbitsBuf) - 1;
        
        strncpy(protocolBuf, line, protocolLen);
        protocolBuf[protocolLen] = '\0';
        strncpy(brandBuf, line + colons[0] + 1, brandLen);
        brandBuf[brandLen] = '\0';
        strncpy(buttonBuf, line + colons[1] + 1, buttonLen);
        buttonBuf[buttonLen] = '\0';
        strncpy(addrBuf, line + colons[2] + 1, addrLen);
        addrBuf[addrLen] = '\0';
        strncpy(cmdBuf, line + colons[3] + 1, cmdLen);
        cmdBuf[cmdLen] = '\0';
        strncpy(nbitsBuf, line + colons[4] + 1, nbitsLen);
        nbitsBuf[nbitsLen] = '\0';
        
        strncpy(code.protocol, protocolBuf, sizeof(code.protocol) - 1);
        code.protocol[sizeof(code.protocol) - 1] = '\0';
        strncpy(code.brand, brandBuf, sizeof(code.brand) - 1);
        code.brand[sizeof(code.brand) - 1] = '\0';
        strncpy(code.button, buttonBuf, sizeof(code.button) - 1);
        code.button[sizeof(code.button) - 1] = '\0';
        code.address = parseHex(addrBuf);
        code.command = parseHex(cmdBuf);
        code.nbits = (uint16_t)atoi(nbitsBuf);
        
        if (code.nbits == 0) {
            Serial.print(F("Warning: Invalid nbits at line "));
            Serial.print(lineNum);
            Serial.print(F(": "));
            Serial.println(line);
            continue;
        }
        
        tvCodes.push_back(code);
    }
    
    file.close();
    Serial.print(F("Loaded "));
    Serial.print(tvCodes.size());
    Serial.println(F(" TV codes"));
    
    return true;
}

static void ensureTVCodesLoaded() {
    if (!tvCodesLoaded) {
        loadTVCodes();
        tvCodesLoaded = true;
    }
}

TVCode* findTVCode(const char* protocol, const char* brand, const char* button) {
    ensureTVCodesLoaded();
    for (size_t i = 0; i < tvCodes.size(); i++) {
        if (strcasecmp_eq(tvCodes[i].protocol, protocol) &&
            strcasecmp_eq(tvCodes[i].brand, brand) &&
            strcasecmp_eq(tvCodes[i].button, button)) {
            return &tvCodes[i];
        }
    }
    return nullptr;
}

TVCode* findTVCode(const char* protocol, const char* button) {
    return findTVCode(protocol, "DEFAULT", button);
}

const TVCode* getAllTVCodes() {
    ensureTVCodesLoaded();
    return tvCodes.data();
}

uint16_t getTVCodeCount() {
    ensureTVCodesLoaded();
    return tvCodes.size();
}

// Dynamically discover protocols from loaded codes
const char** getIRProtocols() {
    ensureTVCodesLoaded();
    // Always rebuild to include Custom at start and refresh folder list
    discoveredProtocolCount = 0;
    
    // Add "Custom" protocol at the start
    strncpy(discoveredProtocols[discoveredProtocolCount], "Custom", sizeof(discoveredProtocols[0]) - 1);
    discoveredProtocols[discoveredProtocolCount][sizeof(discoveredProtocols[0]) - 1] = '\0';
    discoveredProtocolCount++;
    
    // Extract unique protocols from loaded codes
    if (tvCodes.size() > 0 && discoveredProtocolCount < (int)(sizeof(discoveredProtocols) / sizeof(discoveredProtocols[0]))) {
        for (size_t i = 0; i < tvCodes.size(); i++) {
            bool found = false;
            for (int j = 0; j < discoveredProtocolCount; j++) {
                if (strcasecmp_eq(discoveredProtocols[j], tvCodes[i].protocol)) {
                    found = true;
                    break;
                }
            }
            if (!found && discoveredProtocolCount < (int)(sizeof(discoveredProtocols) / sizeof(discoveredProtocols[0]))) {
                strncpy(discoveredProtocols[discoveredProtocolCount], tvCodes[i].protocol, sizeof(discoveredProtocols[0]) - 1);
                discoveredProtocols[discoveredProtocolCount][sizeof(discoveredProtocols[0]) - 1] = '\0';
                discoveredProtocolCount++;
            }
        }
    }
    
    // Convert to C-style array (pointers to char arrays that persist)
    protocolPtrs.clear();
    for (int i = 0; i < discoveredProtocolCount; i++) {
        protocolPtrs.push_back(discoveredProtocols[i]);
    }
    protocolPtrs.push_back(nullptr); // null terminator
    
    return protocolPtrs.data();
}

uint8_t getIRProtocolCount() {
    const char** protocols = getIRProtocols();
    uint8_t count = 0;
    while (protocols[count] != nullptr) {
        count++;
    }
    return count;
}

bool sendIR(const char* protocol, uint64_t data, uint16_t nbits, uint16_t repeat) {
    decode_type_t protoType = protocolNameToType(protocol);
    if (protoType == UNKNOWN) {
        Serial.print(F("Unknown protocol: "));
        Serial.println(protocol);
        return false;
    }
    
    irsend.send(protoType, data, nbits, repeat);
    return true;
}

bool sendTVCode(const TVCode* code, uint16_t repeat) {
    if (!code) return false;
    
    uint64_t data;
    // Protocol-specific data formatting - use strcasecmp_eq to avoid String allocations
    if (strcasecmp_eq(code->protocol, "NEC")) {
        // NEC: combine address (16 bits) and command (8 bits)
        data = ((uint64_t)code->address << 16) | (code->command & 0xFF);
    } else if (strcasecmp_eq(code->protocol, "PANASONIC")) {
        // Panasonic: address (16 bits) + data (32 bits), but send() takes combined
        data = ((uint64_t)code->address << 32) | code->command;
    } else if (strcasecmp_eq(code->protocol, "SHARP")) {
        // Sharp: address (5 bits) + command (8 bits)
        data = ((uint64_t)code->address << 8) | (code->command & 0xFF);
    } else if (strcasecmp_eq(code->protocol, "RC5") || strcasecmp_eq(code->protocol, "RC6")) {
        // RC5/RC6: address and command combined
        data = ((uint64_t)code->address << 6) | (code->command & 0x3F);
    } else {
        // Most other protocols (Sony, Samsung, LG, JVC) use command directly
        data = code->command;
    }
    
    return sendIR(code->protocol, data, code->nbits, repeat);
}

// Legacy functions for backward compatibility
void sendNEC(uint32_t address, uint32_t command) {
    uint64_t data = ((uint64_t)address << 16) | (command & 0xFF);
    irsend.send(NEC, data, 32);
}

void sendRC5(uint8_t address, uint8_t command) {
    uint64_t data = ((uint64_t)address << 6) | (command & 0x3F);
    irsend.send(RC5, data, 12);
}

void sendSony(uint32_t data, uint8_t nbits) {
    irsend.send(SONY, data, nbits);
}

void sendRaw(uint16_t* rawData, uint16_t length, uint32_t frequency) {
    irsend.sendRaw(rawData, length, frequency);
    delay(50);
}

void sendIRPulse(uint16_t onTime, uint16_t offTime, uint32_t frequency) {
    uint16_t rawData[2] = {onTime, offTime};
    irsend.sendRaw(rawData, 2, frequency);
    delay(50);
}

// IR Receiver functions
bool initIRReceiver() {
    irrecv.enableIRIn();
    pinMode(IR_RECEIVER_PIN, INPUT);
    Serial.println(F("IR Receiver initialized on GPIO 26"));
    return true;
}

bool scanIRCode(uint32_t timeoutMs, TVCode* outCode, char* protocolName, size_t protocolNameLen) {
    if (!outCode || !protocolName) return false;
    
    uint32_t startTime = millis();
    decode_results results;
    
    // Clear any pending results
    irrecv.resume();
    
    while (millis() - startTime < timeoutMs) {
        if (irrecv.decode(&results)) {
            // Convert decode_type_t to string - use fixed buffer to avoid String allocation
            // typeToString returns a String, so we need to extract it to a char buffer
            String protoStr = typeToString(results.decode_type);
            const char* protoCStr = protoStr.c_str();
            
            // Copy and convert to uppercase in fixed buffer
            size_t len = strlen(protoCStr);
            if (len >= protocolNameLen) len = protocolNameLen - 1;
            
            for (size_t i = 0; i < len; i++) {
                char c = protoCStr[i];
                protocolName[i] = (c >= 'a' && c <= 'z') ? (c - 32) : c;
            }
            protocolName[len] = '\0';
            
            // Store in TVCode (fixed-size char array copy)
            strncpy(outCode->protocol, protocolName, sizeof(outCode->protocol) - 1);
            outCode->protocol[sizeof(outCode->protocol) - 1] = '\0';
            
            outCode->nbits = results.bits;
            
            if (results.decode_type == NEC) {
                // NEC: address is upper 16 bits, command is bits 15-8 (middle 8 bits)
                // Lower 8 bits are inverted command (auto-generated by library)
                outCode->address = (results.value >> 16) & 0xFFFF;
                outCode->command = (results.value >> 8) & 0xFF;
            } else if (results.decode_type == SONY) {
                // Sony: just use the value
                outCode->address = 0;
                outCode->command = results.value;
            } else if (results.decode_type == RC5 || results.decode_type == RC6) {
                // RC5/RC6: address and command combined
                outCode->address = (results.value >> 6) & 0x1F;
                outCode->command = results.value & 0x3F;
            } else if (results.decode_type == SAMSUNG) {
                // Samsung: full value
                outCode->address = 0;
                outCode->command = results.value;
            } else {
                // Generic: use value as command
                outCode->address = 0;
                outCode->command = results.value;
            }
            
            outCode->brand[0] = '\0'; // Will be set when saving
            outCode->button[0] = '\0'; // Will be set when saving
            
            irrecv.resume();
            return true;
        }
        delay(10);
    }
    
    return false;
}

bool saveCustomCode(const char* folderName, const char* buttonName, const TVCode* code) {
    if (!folderName || !buttonName || !code) return false;
    
    TVCode newCode = *code;
    strncpy(newCode.brand, folderName, sizeof(newCode.brand) - 1);
    newCode.brand[sizeof(newCode.brand) - 1] = '\0';
    strncpy(newCode.button, buttonName, sizeof(newCode.button) - 1);
    newCode.button[sizeof(newCode.button) - 1] = '\0';
    
    customCodes.push_back(newCode);
    
    // Ensure directory structure exists
    if (!LittleFS.exists("/assets")) {
        LittleFS.mkdir("/assets");
    }
    if (!LittleFS.exists("/assets/custom_codes")) {
        LittleFS.mkdir("/assets/custom_codes");
    }
    
    char filePath[256];
    snprintf(filePath, sizeof(filePath), "/assets/custom_codes/%s.txt", folderName);
    
    // Always use write mode if file doesn't exist, append mode if it does
    File file;
    if (LittleFS.exists(filePath)) {
        file = LittleFS.open(filePath, "a");
    } else {
        file = LittleFS.open(filePath, "w");
    }
    
    if (file) {
        file.print(newCode.protocol);
        file.print(":");
        file.print(newCode.brand);
        file.print(":");
        file.print(newCode.button);
        file.print(":");
        file.print("0x");
        file.print(newCode.address, HEX);
        file.print(":");
        file.print("0x");
        file.print(newCode.command, HEX);
        file.print(":");
        file.println(newCode.nbits);
        file.flush();  // Force flush to disk
        file.close();
        return true;
    }
    
    return false;
}

std::vector<std::string> getCustomFolders() {
    ensureCustomCodesLoaded();
    std::vector<std::string> folders;
    
    // First, add folders from in-memory codes
    for (size_t i = 0; i < customCodes.size(); i++) {
        bool found = false;
        for (const auto& f : folders) {
            if (strcasecmp_eq(f.c_str(), customCodes[i].brand)) {
                found = true;
                break;
            }
        }
        if (!found && strlen(customCodes[i].brand) > 0) {
            folders.push_back(std::string(customCodes[i].brand));
        }
    }
    
    // Then, check filesystem for custom code folders
    if (!LittleFS.exists("/assets/custom_codes")) {
        return folders;
    }
    
    File root = LittleFS.open("/assets/custom_codes");
    if (!root || !root.isDirectory()) {
        return folders;
    }
    
    File file = root.openNextFile();
    char nameBuf[256];
    char folderNameBuf[128];
    
    while (file) {
        if (!file.isDirectory()) {
            const char* name = file.name();
            strncpy(nameBuf, name, sizeof(nameBuf) - 1);
            nameBuf[sizeof(nameBuf) - 1] = '\0';
            
            size_t nameLen = strlen(nameBuf);
            bool endsWithGitkeep = nameLen >= 8 && strcmp(nameBuf + nameLen - 8, ".gitkeep") == 0;
            
            if (!endsWithGitkeep) {
                const char* lastSlash = strrchr(nameBuf, '/');
                const char* lastDot = strrchr(nameBuf, '.');
                
                if (lastDot && (!lastSlash || lastDot > lastSlash)) {
                    size_t folderNameLen = lastDot - (lastSlash ? (lastSlash + 1) : nameBuf);
                    if (folderNameLen > 0 && folderNameLen < sizeof(folderNameBuf)) {
                        strncpy(folderNameBuf, lastSlash ? (lastSlash + 1) : nameBuf, folderNameLen);
                        folderNameBuf[folderNameLen] = '\0';
                        
                        bool found = false;
                        for (const auto& f : folders) {
                            if (strcasecmp_eq(f.c_str(), folderNameBuf)) {
                                found = true;
                                break;
                            }
                        }
                        if (!found && strlen(folderNameBuf) > 0) {
                            folders.push_back(std::string(folderNameBuf));
                        }
                    }
                }
            }
        }
        file.close();
        file = root.openNextFile();
    }
    root.close();
    
    return folders;
}

std::vector<const TVCode*> getCustomCodes(const char* folderName) {
    ensureCustomCodesLoaded();
    std::vector<const TVCode*> codes;
    
    // Search in memory (codes are loaded lazily when first accessed)
    for (size_t i = 0; i < customCodes.size(); i++) {
        if (strcasecmp_eq(customCodes[i].brand, folderName)) {
            codes.push_back(&customCodes[i]);
        }
    }
    
    return codes;
}

bool createCustomFolder(const char* folderName) {
    if (!folderName || strlen(folderName) == 0) return false;
    
    // Ensure directory structure exists
    if (!LittleFS.exists("/assets")) {
        if (!LittleFS.mkdir("/assets")) return false;
    }
    
    if (!LittleFS.exists("/assets/custom_codes")) {
        if (!LittleFS.mkdir("/assets/custom_codes")) return false;
    }
    
    char filePath[256];
    snprintf(filePath, sizeof(filePath), "/assets/custom_codes/%s.txt", folderName);
    
    if (LittleFS.exists(filePath)) {
        return true;
    }
    
    // Create file with minimal content to ensure it persists
    File file = LittleFS.open(filePath, "w");
    if (file) {
        // Write newline to ensure file is non-empty and gets flushed to disk
        file.println();
        file.flush();  // Force flush to ensure write is committed
        file.close();
        
        // Verify file was created
        if (LittleFS.exists(filePath)) {
            return true;
        }
    }
    
    return false;
}

bool deleteCustomFolder(const char* folderName) {
    if (!folderName) return false;
    
    char filePath[256];
    snprintf(filePath, sizeof(filePath), "/assets/custom_codes/%s.txt", folderName);
    
    if (LittleFS.remove(filePath)) {
        // Remove from memory
        for (size_t i = 0; i < customCodes.size(); ) {
            if (strcasecmp_eq(customCodes[i].brand, folderName)) {
                customCodes.erase(customCodes.begin() + i);
            } else {
                i++;
            }
        }
        return true;
    }
    
    return false;
}

bool deleteCustomCode(const char* folderName, const char* buttonName) {
    if (!folderName || !buttonName) return false;
    
    // Remove from memory
    for (size_t i = 0; i < customCodes.size(); ) {
        if (strcasecmp_eq(customCodes[i].brand, folderName) &&
            strcasecmp_eq(customCodes[i].button, buttonName)) {
            customCodes.erase(customCodes.begin() + i);
        } else {
            i++;
        }
    }
    
    // Rewrite the file without the deleted code
    char filePath[256];
    snprintf(filePath, sizeof(filePath), "/assets/custom_codes/%s.txt", folderName);
    File readFile = LittleFS.open(filePath, "r");
    if (!readFile) return false;
    
    std::vector<std::string> lines;
    char lineBuf[512];
    char lineButtonBuf[128];
    
    while (readFile.available()) {
        size_t bytesRead = readFile.readBytesUntil('\n', lineBuf, sizeof(lineBuf) - 1);
        lineBuf[bytesRead] = '\0';
        
        // Trim whitespace
        size_t start = 0;
        while (start < bytesRead && (lineBuf[start] == ' ' || lineBuf[start] == '\t' || lineBuf[start] == '\r')) {
            start++;
        }
        size_t end = bytesRead;
        while (end > start && (lineBuf[end-1] == ' ' || lineBuf[end-1] == '\t' || lineBuf[end-1] == '\r')) {
            end--;
        }
        lineBuf[end] = '\0';
        const char* line = lineBuf + start;
        
        if (end - start > 0 && line[0] == '#') {
            lines.push_back(std::string(line)); // Comments - keep as-is
        } else if (end - start > 0) {
            int colons[5];
            int colonCount = 0;
            for (int i = 0; i < (int)(end - start) && colonCount < 5; i++) {
                if (line[i] == ':') {
                    colons[colonCount++] = i;
                }
            }
            
            if (colonCount == 5) {
                // Extract button name to fixed buffer for comparison
                size_t buttonLen = colons[2] - colons[1] - 1;
                if (buttonLen >= sizeof(lineButtonBuf)) buttonLen = sizeof(lineButtonBuf) - 1;
                strncpy(lineButtonBuf, line + colons[1] + 1, buttonLen);
                lineButtonBuf[buttonLen] = '\0';
                
                if (!strcasecmp_eq(lineButtonBuf, buttonName)) {
                    lines.push_back(std::string(line)); // Keep line if button doesn't match
                }
            }
        }
    }
    readFile.close();
    
    File writeFile = LittleFS.open(filePath, "w");
    if (!writeFile) return false;
    
    for (const auto& line : lines) {
        writeFile.println(line.c_str());
    }
    writeFile.close();
    
    return true;
}
