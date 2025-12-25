#ifndef IR_REMOTE_H
#define IR_REMOTE_H

#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <vector>
#include <string>

// IR LED pin - GPIO 25
#define IR_LED_PIN 25

// IR Receiver pin - GPIO 26
#define IR_RECEIVER_PIN 26

// Initialize IR remote functionality
void initIRRemote();

// TV Code structure - uses fixed-size char arrays to avoid String allocations
struct TVCode {
    char protocol[64];    // Protocol name (e.g., "NEC", "SONY", "SAMSUNG")
    char brand[64];      // Brand name (e.g., "LG", "Samsung") or "DEFAULT"
    char button[64];     // Button name (e.g., "Power", "Vol+")
    uint64_t address;    // Protocol-specific address
    uint64_t command;    // Button command code
    uint16_t nbits;      // Number of bits
};

// Load TV codes from file
bool loadTVCodes();
TVCode* findTVCode(const char* protocol, const char* brand, const char* button);
TVCode* findTVCode(const char* protocol, const char* button); // Uses DEFAULT brand
const TVCode* getAllTVCodes();
uint16_t getTVCodeCount();

// Get list of available protocols (discovered from library)
const char** getIRProtocols();
uint8_t getIRProtocolCount();

// Case-insensitive string comparison helper (no String allocation)
bool strcasecmp_eq(const char* a, const char* b);

// Convert protocol name string to decode_type_t
decode_type_t protocolNameToType(const char* protocol);

// Universal send function using protocol name string
bool sendIR(const char* protocol, uint64_t data, uint16_t nbits, uint16_t repeat = 0);

// Send TV code
bool sendTVCode(const TVCode* code, uint16_t repeat = 0);

// Individual protocol send functions (for direct use - legacy)
void sendNEC(uint32_t address, uint32_t command);
void sendRC5(uint8_t address, uint8_t command);
void sendSony(uint32_t data, uint8_t nbits = 12);

// Send raw IR signal (for custom protocols)
void sendRaw(uint16_t* rawData, uint16_t length, uint32_t frequency = 38000);

// Send a simple on/off pulse (for testing)
void sendIRPulse(uint16_t onTime, uint16_t offTime, uint32_t frequency = 38000);

// IR Receiver functions
bool initIRReceiver();
bool scanIRCode(uint32_t timeoutMs, TVCode* outCode, char* protocolName, size_t protocolNameLen);

// Custom code management
bool saveCustomCode(const char* folderName, const char* buttonName, const TVCode* code);
bool createCustomFolder(const char* folderName);
bool deleteCustomFolder(const char* folderName);
bool deleteCustomCode(const char* folderName, const char* buttonName);
std::vector<std::string> getCustomFolders();
std::vector<const TVCode*> getCustomCodes(const char* folderName);

#endif
