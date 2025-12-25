#include "filesystem.h"
#include <algorithm>
#include <utility>
#include <LittleFS.h>
#include <map>

static std::vector<FSEntry> entries;
static std::vector<std::pair<std::string, AppCallback>> appCallbacks;

struct DynamicMount {
    std::string uiPath;
    std::string fsPath;
};
static std::vector<DynamicMount> dynamicMounts;

// Helper functions for string manipulation
static void toLowerCase(std::string& str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
}

static void trim(std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (std::string::npos == first) {
        str = "";
        return;
    }
    size_t last = str.find_last_not_of(" \t\r\n");
    str = str.substr(first, (last - first + 1));
}

static bool startsWith(const std::string& str, const std::string& prefix) {
    return str.rfind(prefix, 0) == 0;
}

static bool endsWith(const std::string& str, const std::string& suffix) {
    if (str.length() < suffix.length()) return false;
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

static std::string extractName(const std::string& path) {
    size_t lastSlash = path.rfind('/');
    if (lastSlash == std::string::npos || lastSlash == path.length() - 1) {
        return path;
    }
    return path.substr(lastSlash + 1);
}

static std::string extractDir(const std::string& path) {
    size_t lastSlash = path.rfind('/');
    if (lastSlash == std::string::npos || lastSlash == 0) {
        return "/";
    }
    return path.substr(0, lastSlash + 1);
}

static std::string normalizePath(const std::string& path) {
    std::string result = path;
    
    if (!startsWith(result, "/")) {
        result = "/" + result;
    }
    
    while (result.length() > 1 && endsWith(result, "/")) {
        result = result.substr(0, result.length() - 1);
    }
    return result;
}

// Helper to parse [icon]Name
static std::string parseIconPrefix(const std::string& input, std::string& outName) {
    if (startsWith(input, "[")) {
        size_t closePos = input.find(']');
        if (closePos != std::string::npos && closePos > 1) {
            std::string icon = input.substr(1, closePos - 1);
            outName = input.substr(closePos + 1);
            return icon;
        }
    }
    outName = input;
    return "";  
}

void initFileSystem() {
    entries.clear();
    entries.reserve(64);
    dynamicMounts.clear();
}

void clearFileSystem() {
    entries.clear();
    dynamicMounts.clear();
}

FSEntry* addFolder(const char* path, const char* iconId) {
    
    std::string normalizedPath = normalizePath(path);
    for (size_t i = 0; i < entries.size(); i++) {
        if (entries[i].path == normalizedPath && entries[i].type == EntryType::FOLDER) {
            
            if (iconId) {
                entries[i].iconId = iconId;
            }
            return &entries[i];
        }
    }
    
    FSEntry entry;
    entry.path = normalizedPath;
    entry.name = extractName(entry.path);
    entry.type = EntryType::FOLDER;
    entry.iconId = iconId ? iconId : ICON_FOLDER;
    entry.callback = nullptr;
    
    entries.push_back(entry);
    return &entries.back();
}

FSEntry* addFile(const char* path, const char* iconId) {
    FSEntry entry;
    entry.path = normalizePath(path);
    entry.name = extractName(entry.path);
    entry.type = EntryType::FILE;
    entry.iconId = iconId ? iconId : ICON_FILE;
    entry.callback = nullptr;
    
    entries.push_back(entry);
    return &entries.back();
}

FSEntry* addApp(const char* path, AppCallback callback, const char* iconId) {
    FSEntry entry;
    entry.path = normalizePath(path);
    entry.name = extractName(entry.path);
    entry.type = EntryType::APP;
    entry.iconId = iconId ? iconId : ICON_APP;
    entry.callback = callback;
    
    entries.push_back(entry);
    return &entries.back();
}

void registerDynamicFolder(const char* uiPath, const char* fsPath) {
    DynamicMount mount;
    mount.uiPath = normalizePath(uiPath);
    mount.fsPath = normalizePath(fsPath);
    dynamicMounts.push_back(mount);
}

// Recursively scan filesystem and cache folder structure (folders and file names, not contents)
static void scanFolderRecursive(const std::string& fsPath, const std::string& uiPath) {
    File dir = LittleFS.open(fsPath.c_str());
    if (!dir || !dir.isDirectory()) {
        return;
    }
    
    File file = dir.openNextFile();
    while (file) {
        std::string rawName = file.name();
        // Extract just the filename from path
        size_t lastSlash = rawName.rfind('/');
        if (lastSlash != std::string::npos) {
            rawName = rawName.substr(lastSlash + 1);
        }
        
        // Skip hidden files
        if (rawName.length() > 0 && rawName[0] == '.') {
            file.close();
            file = dir.openNextFile();
            continue;
        }
        
        std::string displayName;
        std::string icon = parseIconPrefix(rawName, displayName);
        if (displayName.empty()) displayName = rawName;
        
        std::string entryUiPath = uiPath + "/" + displayName;
        
        if (file.isDirectory()) {
            // Add folder to cache
            FSEntry folder;
            folder.path = normalizePath(entryUiPath);
            folder.name = displayName;
            folder.type = EntryType::FOLDER;
            folder.iconId = icon.empty() ? ICON_FOLDER : icon;
            folder.callback = nullptr;
            folder.targetPath = fsPath + "/" + rawName; // Store FS path for navigation
            
            // Check if already exists
            bool exists = false;
            for (const auto& e : entries) {
                if (e.path == folder.path && e.type == EntryType::FOLDER) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                entries.push_back(folder);
            }
            
            // Recursively scan subfolder
            scanFolderRecursive(fsPath + "/" + rawName, entryUiPath);
        } else {
            // Add file/app entry to cache (but don't load contents)
            if (endsWith(rawName, ".lua") || endsWith(rawName, ".cpp")) {
                // It's a script - add as APP entry
                std::string scriptName = displayName;
                size_t dotPos = scriptName.rfind('.');
                if (dotPos != std::string::npos) {
                    scriptName = scriptName.substr(0, dotPos);
                }
                
                FSEntry app;
                app.path = normalizePath(entryUiPath);
                app.name = scriptName;
                app.type = EntryType::APP;
                app.iconId = icon.empty() ? ICON_APP : icon;
                app.callback = endsWith(rawName, ".lua") ? getAppCallback("Lua") : nullptr;
                app.targetPath = fsPath + "/" + rawName; // Store FS path for loading
                
                // Check if already exists
                bool exists = false;
                for (const auto& e : entries) {
                    if (e.path == app.path && e.type == EntryType::APP) {
                        exists = true;
                        break;
                    }
                }
                if (!exists && app.callback) { // Only add Lua apps
                    entries.push_back(app);
                    Serial.print(F("  Cached app: "));
                    Serial.print(app.path.c_str());
                    Serial.print(F(" -> "));
                    Serial.println(app.targetPath.c_str());
                }
            } else {
                // Regular file
                FSEntry fileEntry;
                fileEntry.path = normalizePath(entryUiPath);
                fileEntry.name = displayName;
                fileEntry.type = EntryType::FILE;
                fileEntry.iconId = icon.empty() ? ICON_FILE : icon;
                fileEntry.callback = nullptr;
                fileEntry.targetPath = fsPath + "/" + rawName;
                
                // Check if already exists
                bool exists = false;
                for (const auto& e : entries) {
                    if (e.path == fileEntry.path && e.type == EntryType::FILE) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    entries.push_back(fileEntry);
                }
            }
        }
        
        file.close();
        file = dir.openNextFile();
    }
    dir.close();
}

void scanAndCacheFolderStructure() {
    Serial.println(F("Scanning filesystem structure..."));
    
    // Scan each registered dynamic folder
    for (const auto& mount : dynamicMounts) {
        Serial.print(F("Scanning: "));
        Serial.print(mount.uiPath.c_str());
        Serial.print(F(" -> "));
        Serial.println(mount.fsPath.c_str());
        
        // First, check if the mount path exists as-is
        File testDir = LittleFS.open(mount.fsPath.c_str());
        if (testDir && testDir.isDirectory()) {
            testDir.close();
            scanFolderRecursive(mount.fsPath, mount.uiPath);
        } else {
            // Try to find folder with [icon] prefix
            // For example, /apps/Applications might actually be /apps/[app]Applications
            // This is expected, so we don't log an error - just search for it
            File parentDir = LittleFS.open("/apps");
            if (parentDir && parentDir.isDirectory()) {
                File entry = parentDir.openNextFile();
                bool found = false;
                while (entry) {
                    if (entry.isDirectory()) {
                        std::string dirName = entry.name();
                        size_t lastSlash = dirName.rfind('/');
                        if (lastSlash != std::string::npos) {
                            dirName = dirName.substr(lastSlash + 1);
                        }
                        
                        // Extract display name (without [icon] prefix)
                        std::string displayName;
                        parseIconPrefix(dirName, displayName);
                        
                        // Check if this matches our mount point
                        std::string mountName = mount.fsPath;
                        size_t mountLastSlash = mountName.rfind('/');
                        if (mountLastSlash != std::string::npos) {
                            mountName = mountName.substr(mountLastSlash + 1);
                        }
                        
                        if (displayName == mountName || dirName == mountName) {
                            // Found matching folder, scan it
                            std::string actualFsPath = "/apps/" + dirName;
                            Serial.print(F("Found folder: "));
                            Serial.print(actualFsPath.c_str());
                            Serial.print(F(" -> UI: "));
                            Serial.println(mount.uiPath.c_str());
                            scanFolderRecursive(actualFsPath, mount.uiPath);
                            found = true;
                            break;
                        }
                    }
                    entry.close();
                    entry = parentDir.openNextFile();
                }
                parentDir.close();
                if (!found) {
                    Serial.print(F("Warning: Could not find folder for mount: "));
                    Serial.println(mount.fsPath.c_str());
                }
            }
        }
    }
    
    Serial.print(F("Cached "));
    Serial.print(entries.size());
    Serial.println(F(" filesystem entries"));
}

std::vector<FSEntry> getEntriesInDir(const std::string& dirPath) {
    std::vector<FSEntry> result;
    
    std::string normalizedDir = normalizePath(dirPath);
    
    // Use cached entries from startup scan
    // Match entries whose parent is normalizedDir
    for (const auto& entry : entries) {
        std::string parent = extractDir(entry.path);
        // extractDir returns path with trailing slash, normalize it for comparison
        std::string normalizedParent = normalizePath(parent);
        
        // Compare normalized paths
        if (normalizedParent == normalizedDir) {
            result.push_back(entry);
        }
    }
    
    // Sort: folders first, then alphabetically by name
    std::sort(result.begin(), result.end(), [](const FSEntry& a, const FSEntry& b) {
        // Folders before files/apps
        if (a.type == EntryType::FOLDER && b.type != EntryType::FOLDER) return true;
        if (a.type != EntryType::FOLDER && b.type == EntryType::FOLDER) return false;
        
        // Alphabetical by name (case-insensitive)
        std::string aName = a.name;
        std::string bName = b.name;
        toLowerCase(aName);
        toLowerCase(bName);
        return aName < bName;
    });
    
    return result;
}

FSEntry* getEntry(const std::string& path) {
    std::string normalized = normalizePath(path);
    
    for (size_t i = 0; i < entries.size(); i++) {
        if (entries[i].path == normalized) {
            return &entries[i];
        }
    }
    
    return nullptr;
}

std::string getParentDir(const std::string& path) {
    if (path == "/" || path.empty()) {
        return "/";
    }
    
    std::string normalized = normalizePath(path);
    size_t lastSlash = normalized.rfind('/');
    
    if (lastSlash == std::string::npos || lastSlash == 0) {
        return "/";
    }
    
    return normalized.substr(0, lastSlash);
}

int getEntryCount() {
    return entries.size(); // Only counts static entries
}

void registerAppCallback(const char* name, AppCallback callback) {
    if (appCallbacks.size() >= MAX_APP_CALLBACKS) {
        return;
    }
    
    for (auto& pair : appCallbacks) {
        if (pair.first == name) {
            pair.second = callback;
            return;
        }
    }
    
    appCallbacks.push_back(std::make_pair(std::string(name), callback));
}

AppCallback getAppCallback(const char* name) {
    for (const auto& pair : appCallbacks) {
        if (pair.first == name) {
            return pair.second;
        }
    }
    return nullptr;
}

void loadFromString(const std::string& data) {
    
    std::vector<std::string> pathAtDepth;
    pathAtDepth.push_back("");  
    
    size_t pos = 0;
    while (pos < data.length()) {
        
        size_t lineEnd = data.find('\n', pos);
        if (lineEnd == std::string::npos) lineEnd = data.length();
        
        std::string line = data.substr(pos, lineEnd - pos);
        pos = lineEnd + 1;
        
        if (line.length() == 0) continue;
        
        size_t depth = 0;
        while (depth < line.length() && line[depth] == ' ') {
            depth++;
        }
        
        if (depth >= line.length()) continue;
        
        std::string content = line.substr(depth);
        trim(content);
        
        if (content.length() == 0) continue;
        
        char typeChar = content[0];
        EntryType type;
        const char* defaultIcon;
        
        switch (typeChar) {
            case 'd':
            case 'D':
                type = EntryType::FOLDER;
                defaultIcon = ICON_FOLDER;
                break;
            case 'f':
            case 'F':
                type = EntryType::FILE;
                defaultIcon = ICON_FILE;
                break;
            case 'a':
            case 'A':
                type = EntryType::APP;
                defaultIcon = ICON_APP;
                break;
            default:
                continue;
        }
        
        if (content.length() < 2) continue;
        content = content.substr(2);
        trim(content);
        
        const char* iconId = defaultIcon;
        if (startsWith(content, "ICON:")) {
            size_t spacePos = content.find(' ');
            if (spacePos != std::string::npos) {
                std::string iconSpec = content.substr(5, spacePos - 5);
                
                static char iconIdBuffer[MAX_APP_CALLBACKS][32];
                static int iconIdIndex = 0;
                
                strncpy(iconIdBuffer[iconIdIndex % MAX_APP_CALLBACKS], iconSpec.c_str(), 31);
                iconIdBuffer[iconIdIndex % MAX_APP_CALLBACKS][31] = '\0';
                iconId = iconIdBuffer[iconIdIndex % MAX_APP_CALLBACKS];
                iconIdIndex++;
                
                content = content.substr(spacePos + 1);
                trim(content);
            }
        }
        
        std::string name = content;
        if (name.length() == 0) continue;
        
        std::string parentPath = "";
        if (depth > 0 && depth <= pathAtDepth.size()) {
            parentPath = pathAtDepth[depth - 1];
        }
        
        std::string fullPath = parentPath + "/" + name;
        
        switch (type) {
            case EntryType::FOLDER:
                addFolder(fullPath.c_str(), iconId);
                
                while (pathAtDepth.size() <= depth) {
                    pathAtDepth.push_back("");
                }
                pathAtDepth[depth] = fullPath;
                break;
                
            case EntryType::FILE:
                addFile(fullPath.c_str(), iconId);
                break;
                
            case EntryType::APP: {
                AppCallback cb = getAppCallback(name.c_str());
                addApp(fullPath.c_str(), cb, iconId);
                break;
            }
        }
    }
}
