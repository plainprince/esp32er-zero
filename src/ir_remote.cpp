#include "ir_remote.h"
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRutils.h>
#include "filesystem.h"
#include <LittleFS.h>
#include <vector>

static IRsend irsend(IR_LED_PIN);
static IRrecv irrecv(IR_RECEIVER_PIN);
static std::vector<TVCode> tvCodes;
static std::vector<String> protocolNames;
static std::vector<String> discoveredProtocols; // For dynamic protocol discovery
static std::vector<const char*> protocolPtrs; // C-style array for return
static std::vector<TVCode> customCodes; // Custom scanned codes

// Forward declarations
uint64_t parseHex(const String& str);

// Helper function to convert protocol name string to decode_type_t
decode_type_t protocolNameToType(const char* protocol) {
    String proto = String(protocol);
    proto.toUpperCase();
    
    // Map protocol names to decode_type_t enum values
    // Common TV remote protocols
    if (proto == "NEC") return NEC;
    if (proto == "SONY") return SONY;
    if (proto == "RC5") return RC5;
    if (proto == "RC6") return RC6;
    if (proto == "SAMSUNG") return SAMSUNG;
    if (proto == "LG") return LG;
    if (proto == "PANASONIC") return PANASONIC;
    if (proto == "SHARP") return SHARP;
    if (proto == "JVC") return JVC;
    if (proto == "DENON") return DENON;
    if (proto == "DISH") return DISH;
    if (proto == "PIONEER") return PIONEER;
    if (proto == "LG2") return LG2;
    if (proto == "SAMSUNG36") return SAMSUNG36;
    if (proto == "NEC_LIKE") return NEC_LIKE;
    
    // AC protocols (less common for TV remotes) - only include if available
    #ifdef DECODE_MITSUBISHI
    if (proto == "MITSUBISHI") return MITSUBISHI;
    #endif
    #ifdef DECODE_COOLIX
    if (proto == "COOLIX") return COOLIX;
    #endif
    #ifdef DECODE_DAIKIN
    if (proto == "DAIKIN") return DAIKIN;
    #endif
    #ifdef DECODE_GREE
    if (proto == "GREE") return GREE;
    #endif
    #ifdef DECODE_TOSHIBA_AC
    if (proto == "TOSHIBA_AC") return TOSHIBA_AC;
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
    while (file) {
        if (!file.isDirectory()) {
            String name = file.name();
            
            if (name.endsWith(".txt") && !name.endsWith(".gitkeep")) {
                String fullPath = String("/assets/custom_codes/") + name.substring(name.lastIndexOf('/') + 1);
                File codeFile = LittleFS.open(fullPath, "r");
                
                if (codeFile) {
                    while (codeFile.available()) {
                        String line = codeFile.readStringUntil('\n');
                        line.trim();
                        
                        if (line.length() == 0 || line.startsWith("#")) continue;
                        
                        int colons[5];
                        int colonCount = 0;
                        for (int i = 0; i < (int)line.length() && colonCount < 5; i++) {
                            if (line[i] == ':') {
                                colons[colonCount++] = i;
                            }
                        }
                        
                        if (colonCount == 5) {
                            TVCode code;
                            code.protocol = line.substring(0, colons[0]);
                            code.brand = line.substring(colons[0] + 1, colons[1]);
                            code.button = line.substring(colons[1] + 1, colons[2]);
                            code.address = parseHex(line.substring(colons[2] + 1, colons[3]));
                            code.command = parseHex(line.substring(colons[3] + 1, colons[4]));
                            code.nbits = line.substring(colons[4] + 1).toInt();
                            
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
    
    // Load TV codes
    loadTVCodes();
    
    // Load custom codes from filesystem
    loadCustomCodes();
    
    // Initialize IR receiver
    initIRReceiver();
}

// Parse hex string (with or without 0x prefix)
uint64_t parseHex(const String& str) {
    String s = str;
    s.trim();
    s.toUpperCase();
    if (s.startsWith("0X")) {
        s = s.substring(2);
    }
    return strtoull(s.c_str(), nullptr, 16);
}

bool loadTVCodes() {
    tvCodes.clear();
    discoveredProtocols.clear();
    protocolPtrs.clear();
    
    File file = LittleFS.open("/assets/tv_codes.txt", "r");
    if (!file) {
        Serial.println(F("Warning: Could not open /assets/tv_codes.txt"));
        return false;
    }
    
    int lineNum = 0;
    while (file.available()) {
        lineNum++;
        String line = file.readStringUntil('\n');
        line.trim();
        
        // Skip empty lines and comments
        if (line.length() == 0 || line.startsWith("#")) {
            continue;
        }
        
        // Parse format: protocol:brand:button:address:command:nbits
        int colons[5];
        int colonCount = 0;
        for (int i = 0; i < (int)line.length() && colonCount < 5; i++) {
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
        code.protocol = line.substring(0, colons[0]);
        code.brand = line.substring(colons[0] + 1, colons[1]);
        code.button = line.substring(colons[1] + 1, colons[2]);
        code.address = parseHex(line.substring(colons[2] + 1, colons[3]));
        code.command = parseHex(line.substring(colons[3] + 1, colons[4]));
        code.nbits = line.substring(colons[4] + 1).toInt();
        
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

TVCode* findTVCode(const char* protocol, const char* brand, const char* button) {
    for (size_t i = 0; i < tvCodes.size(); i++) {
        if (tvCodes[i].protocol.equalsIgnoreCase(protocol) &&
            tvCodes[i].brand.equalsIgnoreCase(brand) &&
            tvCodes[i].button.equalsIgnoreCase(button)) {
            return &tvCodes[i];
        }
    }
    return nullptr;
}

TVCode* findTVCode(const char* protocol, const char* button) {
    return findTVCode(protocol, "DEFAULT", button);
}

const TVCode* getAllTVCodes() {
    return tvCodes.data();
}

uint16_t getTVCodeCount() {
    return tvCodes.size();
}

// Dynamically discover protocols from loaded codes
const char** getIRProtocols() {
    // Always rebuild to include Custom at start and refresh folder list
    discoveredProtocols.clear();
    
    // Add "Custom" protocol at the start
    discoveredProtocols.push_back("Custom");
    
    // Extract unique protocols from loaded codes
    if (tvCodes.size() > 0) {
        for (size_t i = 0; i < tvCodes.size(); i++) {
            bool found = false;
            for (const auto& p : discoveredProtocols) {
                if (p.equalsIgnoreCase(tvCodes[i].protocol)) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                discoveredProtocols.push_back(tvCodes[i].protocol);
            }
        }
    }
    
    // Convert to C-style array (pointers to String objects that persist)
    protocolPtrs.clear();
    for (const auto& p : discoveredProtocols) {
        protocolPtrs.push_back(p.c_str());
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
    
    // Protocol-specific data formatting
    if (code->protocol.equalsIgnoreCase("NEC")) {
        // NEC: combine address (16 bits) and command (8 bits)
        data = ((uint64_t)code->address << 16) | (code->command & 0xFF);
    } else if (code->protocol.equalsIgnoreCase("PANASONIC")) {
        // Panasonic: address (16 bits) + data (32 bits), but send() takes combined
        data = ((uint64_t)code->address << 32) | code->command;
    } else if (code->protocol.equalsIgnoreCase("SHARP")) {
        // Sharp: address (5 bits) + command (8 bits)
        data = ((uint64_t)code->address << 8) | (code->command & 0xFF);
    } else if (code->protocol.equalsIgnoreCase("RC5") || code->protocol.equalsIgnoreCase("RC6")) {
        // RC5/RC6: address and command combined
        data = ((uint64_t)code->address << 6) | (code->command & 0x3F);
    } else {
        // Most other protocols (Sony, Samsung, LG, JVC) use command directly
        data = code->command;
    }
    
    return sendIR(code->protocol.c_str(), data, code->nbits, repeat);
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
            // Convert decode_type_t to string
            String protoStr = typeToString(results.decode_type);
            protoStr.toUpperCase();
            strncpy(protocolName, protoStr.c_str(), protocolNameLen - 1);
            protocolName[protocolNameLen - 1] = '\0';
            
            // Extract address and command based on protocol
            outCode->protocol = protoStr;
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
            
            outCode->brand = ""; // Will be set when saving
            outCode->button = ""; // Will be set when saving
            
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
    newCode.brand = String(folderName);
    newCode.button = String(buttonName);
    
    customCodes.push_back(newCode);
    
    // Ensure directory structure exists
    if (!LittleFS.exists("/assets")) {
        LittleFS.mkdir("/assets");
    }
    if (!LittleFS.exists("/assets/custom_codes")) {
        LittleFS.mkdir("/assets/custom_codes");
    }
    
    String filePath = String("/assets/custom_codes/") + folderName + ".txt";
    
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

std::vector<String> getCustomFolders() {
    std::vector<String> folders;
    
    // First, add folders from in-memory codes
    for (size_t i = 0; i < customCodes.size(); i++) {
        bool found = false;
        for (const auto& f : folders) {
            if (f.equalsIgnoreCase(customCodes[i].brand)) {
                found = true;
                break;
            }
        }
        if (!found && customCodes[i].brand.length() > 0) {
            folders.push_back(customCodes[i].brand);
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
    while (file) {
        if (!file.isDirectory()) {
            String name = file.name();
            
            if (!name.endsWith(".gitkeep")) {
                int lastSlash = name.lastIndexOf('/');
                int lastDot = name.lastIndexOf('.');
                
                String folderName;
                if (lastSlash >= 0 && lastDot > lastSlash) {
                    folderName = name.substring(lastSlash + 1, lastDot);
                } else if (lastDot > 0) {
                    folderName = name.substring(0, lastDot);
                }
                
                if (folderName.length() > 0) {
                    bool found = false;
                    for (const auto& f : folders) {
                        if (f.equalsIgnoreCase(folderName)) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        folders.push_back(folderName);
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
    std::vector<const TVCode*> codes;
    
    // Search in memory (codes are already loaded at startup)
    for (size_t i = 0; i < customCodes.size(); i++) {
        if (customCodes[i].brand.equalsIgnoreCase(folderName)) {
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
    
    String filePath = String("/assets/custom_codes/") + folderName + ".txt";
    
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
    
    String filePath = String("/assets/custom_codes/") + folderName + ".txt";
    
    if (LittleFS.remove(filePath)) {
        // Remove from memory
        for (size_t i = 0; i < customCodes.size(); ) {
            if (customCodes[i].brand.equalsIgnoreCase(folderName)) {
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
        if (customCodes[i].brand.equalsIgnoreCase(folderName) &&
            customCodes[i].button.equalsIgnoreCase(buttonName)) {
            customCodes.erase(customCodes.begin() + i);
        } else {
            i++;
        }
    }
    
    // Rewrite the file without the deleted code
    String filePath = String("/assets/custom_codes/") + folderName + ".txt";
    File readFile = LittleFS.open(filePath, "r");
    if (!readFile) return false;
    
    std::vector<String> lines;
    while (readFile.available()) {
        String line = readFile.readStringUntil('\n');
        line.trim();
        
        if (line.length() > 0 && !line.startsWith("#")) {
            int colons[5];
            int colonCount = 0;
            for (int i = 0; i < (int)line.length() && colonCount < 5; i++) {
                if (line[i] == ':') {
                    colons[colonCount++] = i;
                }
            }
            
            if (colonCount == 5) {
                String lineButton = line.substring(colons[1] + 1, colons[2]);
                if (!lineButton.equalsIgnoreCase(buttonName)) {
                    lines.push_back(line);
                }
            }
        } else if (line.startsWith("#")) {
            lines.push_back(line);
        }
    }
    readFile.close();
    
    File writeFile = LittleFS.open(filePath, "w");
    if (!writeFile) return false;
    
    for (const auto& line : lines) {
        writeFile.println(line);
    }
    writeFile.close();
    
    return true;
}
