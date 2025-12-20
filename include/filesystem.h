#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <Arduino.h>
#include <vector>
#include "icons.h"





enum class EntryType : uint8_t {
    FOLDER,     
    FILE,       
    APP         
};






typedef void (*AppCallback)(const char* path, const char* name);





struct FSEntry {
    String path;            
    String name;            
    EntryType type;         
    String iconId;          
    AppCallback callback;   
    
    FSEntry() : type(EntryType::FILE), iconId(ICON_FILE), callback(nullptr) {}
};






void initFileSystem();


void clearFileSystem();


FSEntry* addFolder(const char* path, const char* iconId = ICON_FOLDER);


FSEntry* addFile(const char* path, const char* iconId = ICON_FILE);


FSEntry* addApp(const char* path, AppCallback callback, const char* iconId = ICON_APP);


std::vector<FSEntry*> getEntriesInDir(const String& dirPath);


FSEntry* getEntry(const String& path);


String getParentDir(const String& path);


int getEntryCount();






void loadFromString(const String& data);






void registerAppCallback(const char* name, AppCallback callback);


AppCallback getAppCallback(const char* name);


#define MAX_APP_CALLBACKS 32

#endif 

