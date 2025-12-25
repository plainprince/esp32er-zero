#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <Arduino.h>
#include <vector>
#include <string>
#include "icons.h"

enum class EntryType : uint8_t {
    FOLDER,     
    FILE,       
    APP         
};

typedef void (*AppCallback)(const char* path, const char* name);

struct FSEntry {
    std::string path;            
    std::string name;            
    EntryType type;         
    std::string iconId;          
    AppCallback callback;   
    std::string targetPath; // Real FS path (optional)
    
    FSEntry() : type(EntryType::FILE), iconId(ICON_FILE), callback(nullptr) {}
};

void initFileSystem();

void clearFileSystem();

FSEntry* addFolder(const char* path, const char* iconId = ICON_FOLDER);

FSEntry* addFile(const char* path, const char* iconId = ICON_FILE);

FSEntry* addApp(const char* path, AppCallback callback, const char* iconId = ICON_APP);

// Registers a dynamic folder mapping (e.g. "/Applications" -> "/apps/Applications")
void registerDynamicFolder(const char* uiPath, const char* fsPath);

// Scan filesystem and cache folder structure (folders and file names, not contents)
void scanAndCacheFolderStructure();

// Returns entries by value now, as they might be generated dynamically
std::vector<FSEntry> getEntriesInDir(const std::string& dirPath);

// Only searches static entries
FSEntry* getEntry(const std::string& path);

std::string getParentDir(const std::string& path);

int getEntryCount();

void loadFromString(const std::string& data);

void registerAppCallback(const char* name, AppCallback callback);

AppCallback getAppCallback(const char* name);

#define MAX_APP_CALLBACKS 32

#endif 
