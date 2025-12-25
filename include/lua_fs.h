#ifndef LUA_FS_H
#define LUA_FS_H

#include <Arduino.h>
#include <vector>
#include <string>


bool initLuaFS();


std::string loadScript(const char* path);


bool saveScript(const char* path, const char* content);


bool deleteScript(const char* path);


bool scriptExists(const char* path);


std::string getScriptDataFolder(const char* scriptPath);


std::string readDataFile(const char* scriptPath, const char* filename);


bool writeDataFile(const char* scriptPath, const char* filename, const char* content);


size_t getFSFreeSpace();


size_t getFSTotalSpace();


#define loadLuaScript(path) loadScript(path)
#define saveLuaScript(path, content) saveScript(path, content)
#define deleteLuaScript(path) deleteScript(path)
#define luaScriptExists(path) scriptExists(path)
#define getLuaFSFreeSpace() getFSFreeSpace()
#define getLuaFSTotalSpace() getFSTotalSpace()

#endif 
