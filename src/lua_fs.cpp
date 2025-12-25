#include "lua_fs.h"
#include <LittleFS.h>
#include <map>
#include <algorithm>

#define LUA_SCRIPTS_DIR "/apps"
#define STORAGE_DIR "/storage"


bool initLuaFS() {
    if (!LittleFS.begin(true)) {  
        Serial.println(F("LittleFS mount failed!"));
        return false;
    }
    
    Serial.println(F("LittleFS mounted"));
    Serial.print(F("Total: "));
    Serial.print(LittleFS.totalBytes());
    Serial.print(F(" bytes, Used: "));
    Serial.print(LittleFS.usedBytes());
    Serial.println(F(" bytes"));
    
    
    if (!LittleFS.exists(LUA_SCRIPTS_DIR)) {
        LittleFS.mkdir(LUA_SCRIPTS_DIR);
        Serial.println(F("Created /apps directory"));
    }
    if (!LittleFS.exists(STORAGE_DIR)) {
        LittleFS.mkdir(STORAGE_DIR);
        Serial.println(F("Created /storage directory"));
    }
    
    return true;
}

std::string loadScript(const char* path) {
    File file = LittleFS.open(path, "r");
    if (!file) {
        Serial.print(F("Failed to open script: "));
        Serial.println(path);
        return "";
    }
    
    // Read file in chunks to std::string
    std::string content = "";
    content.reserve(file.size());
    
    char buffer[256];
    while(file.available()) {
        size_t read = file.readBytes(buffer, sizeof(buffer)-1);
        buffer[read] = '\0';
        content += buffer;
    }
    
    file.close();
    
    Serial.print(F("Loaded script: "));
    Serial.print(path);
    Serial.print(F(" ("));
    Serial.print(content.length());
    Serial.println(F(" bytes)"));
    
    return content;
}

bool saveScript(const char* path, const char* content) {
    
    std::string pathStr = path;
    size_t lastSlash = pathStr.rfind('/');
    if (lastSlash != std::string::npos && lastSlash > 0) {
        std::string dir = pathStr.substr(0, lastSlash);
        if (!LittleFS.exists(dir.c_str())) {
            LittleFS.mkdir(dir.c_str());
        }
    }
    
    File file = LittleFS.open(path, "w");
    if (!file) {
        Serial.print(F("Failed to create script: "));
        Serial.println(path);
        return false;
    }
    
    size_t written = file.print(content);
    file.close();
    
    Serial.print(F("Saved script: "));
    Serial.print(path);
    Serial.print(F(" ("));
    Serial.print(written);
    Serial.println(F(" bytes)"));
    
    return written > 0;
}

bool deleteScript(const char* path) {
    if (LittleFS.remove(path)) {
        Serial.print(F("Deleted script: "));
        Serial.println(path);
        return true;
    }
    Serial.print(F("Failed to delete script: "));
    Serial.println(path);
    return false;
}

bool scriptExists(const char* path) {
    return LittleFS.exists(path);
}

std::string getScriptDataFolder(const char* scriptPath) {
    std::string path = scriptPath;
    
    
    size_t dotPos = path.rfind('.');
    if (dotPos != std::string::npos && dotPos > 0) {
        path = path.substr(0, dotPos);
    }
    
    
    if (LittleFS.exists(path.c_str())) {
        File f = LittleFS.open(path.c_str());
        if (f && f.isDirectory()) {
            f.close();
            return path + "/";
        }
    }
    
    return "";
}

std::string readDataFile(const char* scriptPath, const char* filename) {
    std::string dataFolder = getScriptDataFolder(scriptPath);
    if (dataFolder.length() == 0) {
        return "";
    }
    
    std::string filePath = dataFolder + filename;
    File file = LittleFS.open(filePath.c_str(), "r");
    if (!file) {
        return "";
    }
    
    // Read to std::string
    std::string content = "";
    char buffer[256];
    while(file.available()) {
        size_t read = file.readBytes(buffer, sizeof(buffer)-1);
        buffer[read] = '\0';
        content += buffer;
    }
    file.close();
    return content;
}

bool writeDataFile(const char* scriptPath, const char* filename, const char* content) {
    std::string dataFolder = getScriptDataFolder(scriptPath);
    if (dataFolder.length() == 0) {
        
        std::string path = scriptPath;
        size_t dotPos = path.rfind('.');
        if (dotPos != std::string::npos && dotPos > 0) {
            path = path.substr(0, dotPos);
        }
        LittleFS.mkdir(path.c_str());
        dataFolder = path + "/";
    }
    
    std::string filePath = dataFolder + filename;
    File file = LittleFS.open(filePath.c_str(), "w");
    if (!file) {
        return false;
    }
    
    size_t written = file.print(content);
    file.close();
    return written > 0;
}

size_t getFSFreeSpace() {
    return LittleFS.totalBytes() - LittleFS.usedBytes();
}

size_t getFSTotalSpace() {
    return LittleFS.totalBytes();
}
