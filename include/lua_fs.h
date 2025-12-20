#ifndef LUA_FS_H
#define LUA_FS_H

#include <Arduino.h>
#include <vector>


enum class ScriptType {
    LUA,    
    CPP     
};


struct ScriptInfo {
    String name;        
    String path;        
    String uiPath;      
    String dataFolder;  
    String icon;        
    ScriptType type;    
    size_t size;        
    bool hasDataFolder; 
};


struct ScriptFolder {
    String fsPath;      
    String uiPath;      
    String name;        
    String icon;        
};


bool initLuaFS();


std::vector<ScriptFolder> listScriptFolders();


std::vector<ScriptInfo> listScriptsInFolder(const char* folderPath);


std::vector<ScriptInfo> listAllScripts();


void registerScriptPath(const String& uiPath, const String& fsPath);


String getScriptFsPath(const String& uiPath);


String loadScript(const char* path);


bool saveScript(const char* path, const char* content);


bool deleteScript(const char* path);


bool scriptExists(const char* path);


String getScriptDataFolder(const char* scriptPath);


String readDataFile(const char* scriptPath, const char* filename);


bool writeDataFile(const char* scriptPath, const char* filename, const char* content);


size_t getFSFreeSpace();


size_t getFSTotalSpace();


#define listLuaScripts() listAllScripts()
#define loadLuaScript(path) loadScript(path)
#define saveLuaScript(path, content) saveScript(path, content)
#define deleteLuaScript(path) deleteScript(path)
#define luaScriptExists(path) scriptExists(path)
#define getLuaFSFreeSpace() getFSFreeSpace()
#define getLuaFSTotalSpace() getFSTotalSpace()

#endif 
