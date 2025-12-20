#include "lua_fs.h"
#include <LittleFS.h>
#include <map>

#define LUA_SCRIPTS_DIR "/apps"
#define STORAGE_DIR "/storage"


static std::map<String, String> scriptPathMap;

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



static String parseIconPrefix(const String& input, String& outName) {
    if (input.startsWith("[")) {
        int closePos = input.indexOf(']');
        if (closePos > 1) {
            String icon = input.substring(1, closePos);
            outName = input.substring(closePos + 1);
            return icon;
        }
    }
    outName = input;
    return "";  
}


static void scanFoldersRecursive(const String& fsPath, const String& uiPath, std::vector<ScriptFolder>& folders) {
    File dir = LittleFS.open(fsPath);
    if (!dir || !dir.isDirectory()) {
        return;
    }
    
    File entry = dir.openNextFile();
    while (entry) {
        if (entry.isDirectory()) {
            String rawName = entry.name();
            
            
            int lastSlash = rawName.lastIndexOf('/');
            if (lastSlash >= 0) {
                rawName = rawName.substring(lastSlash + 1);
            }
            
            Serial.print(F("  Raw folder name: '"));
            Serial.print(rawName);
            Serial.println(F("'"));
            
            
            String displayName;
            String icon = parseIconPrefix(rawName, displayName);
            
            Serial.print(F("  Parsed: icon='"));
            Serial.print(icon);
            Serial.print(F("' displayName='"));
            Serial.print(displayName);
            Serial.println(F("'"));
            
            ScriptFolder folder;
            folder.name = displayName.length() > 0 ? displayName : rawName;
            folder.icon = icon.length() > 0 ? icon : "folder";
            folder.fsPath = fsPath + "/" + rawName;
            folder.uiPath = uiPath + "/" + folder.name;
            folders.push_back(folder);
            
            Serial.print(F("Found script folder: "));
            Serial.print(folder.name);
            Serial.print(F(" ["));
            Serial.print(folder.icon);
            Serial.print(F("] -> UI: "));
            Serial.println(folder.uiPath);
            
            
            scanFoldersRecursive(folder.fsPath, folder.uiPath, folders);
        }
        entry = dir.openNextFile();
    }
}

std::vector<ScriptFolder> listScriptFolders() {
    std::vector<ScriptFolder> folders;
    scanFoldersRecursive(LUA_SCRIPTS_DIR, "", folders);
    return folders;
}

static bool isScriptFile(const String& name) {
    return name.endsWith(".lua") || name.endsWith(".cpp");
}

static ScriptType getScriptType(const String& name) {
    if (name.endsWith(".cpp")) return ScriptType::CPP;
    return ScriptType::LUA;
}

static String getScriptName(const String& filename) {
    
    String name = filename;
    int dotPos = name.lastIndexOf('.');
    if (dotPos > 0) {
        name = name.substring(0, dotPos);
    }
    
    
    String displayName;
    parseIconPrefix(name, displayName);
    return displayName;
}

static String getScriptIcon(const String& filename) {
    
    String name = filename;
    int dotPos = name.lastIndexOf('.');
    if (dotPos > 0) {
        name = name.substring(0, dotPos);
    }
    
    
    String displayName;
    String icon = parseIconPrefix(name, displayName);
    return icon.length() > 0 ? icon : "app";  
}

std::vector<ScriptInfo> listScriptsInFolder(const char* folderPath) {
    std::vector<ScriptInfo> scripts;
    
    File folder = LittleFS.open(folderPath);
    if (!folder || !folder.isDirectory()) {
        return scripts;
    }
    
    
    File entry = folder.openNextFile();
    while (entry) {
        if (!entry.isDirectory()) {
            String filename = entry.name();
            
            
            int lastSlash = filename.lastIndexOf('/');
            if (lastSlash >= 0) {
                filename = filename.substring(lastSlash + 1);
            }
            
            if (isScriptFile(filename)) {
                ScriptInfo info;
                info.name = getScriptName(filename);
                info.icon = getScriptIcon(filename);
                info.path = String(folderPath) + "/" + filename;
                info.type = getScriptType(filename);
                info.size = entry.size();
                info.hasDataFolder = false;
                info.dataFolder = "";
                
                Serial.print(F("  Script file: '"));
                Serial.print(filename);
                Serial.print(F("' -> name='"));
                Serial.print(info.name);
                Serial.print(F("' icon='"));
                Serial.print(info.icon);
                Serial.println(F("'"));
                
                scripts.push_back(info);
            }
        }
        entry = folder.openNextFile();
    }
    
    
    folder = LittleFS.open(folderPath);
    entry = folder.openNextFile();
    while (entry) {
        if (entry.isDirectory()) {
            String dirName = entry.name();
            
            for (auto& script : scripts) {
                if (script.name == dirName) {
                    script.hasDataFolder = true;
                    script.dataFolder = String(folderPath) + "/" + dirName + "/";
                    Serial.print(F("Script '"));
                    Serial.print(script.name);
                    Serial.print(F("' has data folder: "));
                    Serial.println(script.dataFolder);
                }
            }
        }
        entry = folder.openNextFile();
    }
    
    return scripts;
}

std::vector<ScriptInfo> listAllScripts() {
    std::vector<ScriptInfo> allScripts;
    
    
    std::vector<ScriptFolder> folders = listScriptFolders();
    
    
    std::vector<ScriptInfo> rootScripts = listScriptsInFolder(LUA_SCRIPTS_DIR);
    for (auto& script : rootScripts) {
        script.uiPath = "/" + script.name;
        allScripts.push_back(script);
    }
    
    
    for (const auto& folder : folders) {
        std::vector<ScriptInfo> scripts = listScriptsInFolder(folder.fsPath.c_str());
        for (auto& script : scripts) {
            script.uiPath = folder.uiPath + "/" + script.name;
            allScripts.push_back(script);
            
            Serial.print(F("Found script: "));
            Serial.print(script.name);
            Serial.print(F(" ("));
            Serial.print(script.type == ScriptType::LUA ? "Lua" : "C++");
            Serial.print(F(") -> UI: "));
            Serial.println(script.uiPath);
        }
    }
    
    return allScripts;
}

void registerScriptPath(const String& uiPath, const String& fsPath) {
    scriptPathMap[uiPath] = fsPath;
    Serial.print(F("Registered script path: "));
    Serial.print(uiPath);
    Serial.print(F(" -> "));
    Serial.println(fsPath);
}

String getScriptFsPath(const String& uiPath) {
    auto it = scriptPathMap.find(uiPath);
    if (it != scriptPathMap.end()) {
        return it->second;
    }
    return "";
}

String loadScript(const char* path) {
    File file = LittleFS.open(path, "r");
    if (!file) {
        Serial.print(F("Failed to open script: "));
        Serial.println(path);
        return "";
    }
    
    String content = file.readString();
    file.close();
    
    Serial.print(F("Loaded script: "));
    Serial.print(path);
    Serial.print(F(" ("));
    Serial.print(content.length());
    Serial.println(F(" bytes)"));
    
    return content;
}

bool saveScript(const char* path, const char* content) {
    
    String pathStr = path;
    int lastSlash = pathStr.lastIndexOf('/');
    if (lastSlash > 0) {
        String dir = pathStr.substring(0, lastSlash);
        if (!LittleFS.exists(dir)) {
            LittleFS.mkdir(dir);
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

String getScriptDataFolder(const char* scriptPath) {
    String path = scriptPath;
    
    
    int dotPos = path.lastIndexOf('.');
    if (dotPos > 0) {
        path = path.substring(0, dotPos);
    }
    
    
    if (LittleFS.exists(path)) {
        File f = LittleFS.open(path);
        if (f && f.isDirectory()) {
            f.close();
            return path + "/";
        }
    }
    
    return "";
}

String readDataFile(const char* scriptPath, const char* filename) {
    String dataFolder = getScriptDataFolder(scriptPath);
    if (dataFolder.length() == 0) {
        return "";
    }
    
    String filePath = dataFolder + filename;
    File file = LittleFS.open(filePath, "r");
    if (!file) {
        return "";
    }
    
    String content = file.readString();
    file.close();
    return content;
}

bool writeDataFile(const char* scriptPath, const char* filename, const char* content) {
    String dataFolder = getScriptDataFolder(scriptPath);
    if (dataFolder.length() == 0) {
        
        String path = scriptPath;
        int dotPos = path.lastIndexOf('.');
        if (dotPos > 0) {
            path = path.substring(0, dotPos);
        }
        LittleFS.mkdir(path);
        dataFolder = path + "/";
    }
    
    String filePath = dataFolder + filename;
    File file = LittleFS.open(filePath, "w");
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
