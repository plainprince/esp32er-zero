#include "filesystem.h"





static std::vector<FSEntry> entries;
static std::vector<std::pair<String, AppCallback>> appCallbacks;





static String extractName(const String& path) {
    int lastSlash = path.lastIndexOf('/');
    if (lastSlash == -1 || lastSlash == (int)path.length() - 1) {
        return path;
    }
    return path.substring(lastSlash + 1);
}

static String extractDir(const String& path) {
    int lastSlash = path.lastIndexOf('/');
    if (lastSlash <= 0) {
        return "/";
    }
    return path.substring(0, lastSlash + 1);
}

static String normalizePath(const String& path) {
    String result = path;
    
    if (!result.startsWith("/")) {
        result = "/" + result;
    }
    
    while (result.length() > 1 && result.endsWith("/")) {
        result = result.substring(0, result.length() - 1);
    }
    return result;
}





void initFileSystem() {
    entries.clear();
    entries.reserve(64);
}

void clearFileSystem() {
    entries.clear();
}

FSEntry* addFolder(const char* path, const char* iconId) {
    
    String normalizedPath = normalizePath(path);
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

std::vector<FSEntry*> getEntriesInDir(const String& dirPath) {
    std::vector<FSEntry*> result;
    
    String normalizedDir = dirPath;
    if (!normalizedDir.endsWith("/")) {
        normalizedDir += "/";
    }
    if (!normalizedDir.startsWith("/")) {
        normalizedDir = "/" + normalizedDir;
    }
    
    for (size_t i = 0; i < entries.size(); i++) {
        String entryDir = extractDir(entries[i].path);
        if (!entryDir.endsWith("/")) {
            entryDir += "/";
        }
        
        if (entryDir == normalizedDir) {
            result.push_back(&entries[i]);
        }
    }
    
    return result;
}

FSEntry* getEntry(const String& path) {
    String normalized = normalizePath(path);
    
    for (size_t i = 0; i < entries.size(); i++) {
        if (entries[i].path == normalized) {
            return &entries[i];
        }
    }
    
    return nullptr;
}

String getParentDir(const String& path) {
    if (path == "/" || path.isEmpty()) {
        return "/";
    }
    
    String normalized = normalizePath(path);
    int lastSlash = normalized.lastIndexOf('/');
    
    if (lastSlash <= 0) {
        return "/";
    }
    
    return normalized.substring(0, lastSlash) + "/";
}

int getEntryCount() {
    return entries.size();
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
    
    appCallbacks.push_back(std::make_pair(String(name), callback));
}

AppCallback getAppCallback(const char* name) {
    for (const auto& pair : appCallbacks) {
        if (pair.first == name) {
            return pair.second;
        }
    }
    return nullptr;
}





void loadFromString(const String& data) {
    
    
    std::vector<String> pathAtDepth;
    pathAtDepth.push_back("");  
    
    int pos = 0;
    while (pos < (int)data.length()) {
        
        int lineEnd = data.indexOf('\n', pos);
        if (lineEnd == -1) lineEnd = data.length();
        
        String line = data.substring(pos, lineEnd);
        pos = lineEnd + 1;
        
        
        if (line.length() == 0) continue;
        
        
        int depth = 0;
        while (depth < (int)line.length() && line[depth] == ' ') {
            depth++;
        }
        
        
        if (depth >= (int)line.length()) continue;
        
        
        String content = line.substring(depth);
        content.trim();
        
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
        content = content.substring(2);
        content.trim();
        
        
        const char* iconId = defaultIcon;
        if (content.startsWith("ICON:")) {
            int spacePos = content.indexOf(' ');
            if (spacePos != -1) {
                String iconSpec = content.substring(5, spacePos);
                
                static char iconIdBuffer[MAX_APP_CALLBACKS][32];
                static int iconIdIndex = 0;
                
                iconSpec.toCharArray(iconIdBuffer[iconIdIndex % MAX_APP_CALLBACKS], 32);
                iconId = iconIdBuffer[iconIdIndex % MAX_APP_CALLBACKS];
                iconIdIndex++;
                
                content = content.substring(spacePos + 1);
                content.trim();
            }
        }
        
        
        String name = content;
        if (name.length() == 0) continue;
        
        
        
        
        
        String parentPath = "";
        if (depth > 0 && depth <= (int)pathAtDepth.size()) {
            parentPath = pathAtDepth[depth - 1];
        }
        
        String fullPath = parentPath + "/" + name;
        
        
        switch (type) {
            case EntryType::FOLDER:
                addFolder(fullPath.c_str(), iconId);
                
                
                while ((int)pathAtDepth.size() <= depth) {
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

