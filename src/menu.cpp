#include "menu.h"
#include "utils.h"
#include "app_runner.h"
#include <FlipperDisplay.h>
#include <string>
#include <vector>

int selectedIndex = 0;
std::string currentPath = "/";
bool menuNeedsRedraw = true;


std::vector<FSEntry> currentEntries;


static std::string lastRenderedPath = "";
static int lastSelectedIndex = -1;

void initMenu() {
    currentPath = "/";
    selectedIndex = 0;
    menuNeedsRedraw = true;
    lastRenderedPath = "";
    lastSelectedIndex = -1;
    currentEntries.clear();
}

void initMenuFromString(const std::string& fileData) {
    initFileSystem();
    initIcons();
    loadFromString(fileData);
    initMenu();
}

static void updateCurrentEntries() {
    currentEntries = getEntriesInDir(currentPath);
}

void renderMenu(bool forceRedraw) {
    extern FlipperDisplay* display;
    
    if (!display) return;
    
    
    if (currentPath != lastRenderedPath) {
        updateCurrentEntries();
        menuNeedsRedraw = true;
    }
    
    
    if (!forceRedraw && !menuNeedsRedraw && selectedIndex == lastSelectedIndex) {
        return;
    }
    
    display->clearDisplay();
    resetCursor();
    
    
    int startIndex = 0;
    if (selectedIndex >= MAX_VISIBLE_LINES) {
        startIndex = selectedIndex - MAX_VISIBLE_LINES + 1;
    }
    
    int itemsToShow = currentEntries.size();
    bool showBackEntry = (currentPath != "/");
    if (showBackEntry) {
        itemsToShow++; 
    }
    
    int displayIndex = 0;
    int actualIndex = 0;
    
    
    uint8_t scale = display->getScale();
    
    
    if (showBackEntry) {
        if (actualIndex >= startIndex && displayIndex < MAX_VISIBLE_LINES) {
            bool isSelected = (selectedIndex == actualIndex);
            
            
            if (isSelected) {
                display->fillRect(0, cursorY, display->width(), CHAR_HEIGHT, COLOR_WHITE);
            }
            
            
            const Icon* backIcon = getBackIcon();
            if (backIcon) {
                display->drawScaledBitmap(0, cursorY, backIcon->data, 
                                          backIcon->width, backIcon->height,
                                          isSelected ? COLOR_BLACK : COLOR_WHITE, scale);
            }
            
            
            cursorX = CHAR_WIDTH * 2;
            display->setCursor(cursorX, cursorY);
            display->setTextColor(isSelected ? COLOR_BLACK : COLOR_WHITE, isSelected ? COLOR_WHITE : COLOR_BLACK);
            display->print("..");
            cursorY += CHAR_HEIGHT;
            cursorX = 0;
            displayIndex++;
        }
        actualIndex++;
    }
    
    
    for (size_t i = 0; i < currentEntries.size() && displayIndex < MAX_VISIBLE_LINES; i++) {
        if (actualIndex < startIndex) {
            actualIndex++;
            continue;
        }
        
        const FSEntry& entry = currentEntries[i];
        bool isSelected = (selectedIndex == actualIndex);
        
        
        if (isSelected) {
            display->fillRect(0, cursorY, display->width(), CHAR_HEIGHT, COLOR_WHITE);
        }
        
        
        const Icon* icon = getIcon(entry.iconId.c_str());
        if (!icon) {
            
            switch (entry.type) {
                case EntryType::FOLDER:
                    icon = getFolderIcon();
                    break;
                case EntryType::APP:
                    icon = getAppIcon();
                    break;
                default:
                    icon = getFileIcon();
                    break;
            }
        }
        
        
        if (icon) {
            display->drawScaledBitmap(0, cursorY, icon->data,
                                      icon->width, icon->height,
                                      isSelected ? COLOR_BLACK : COLOR_WHITE, scale);
        }
        
        
        cursorX = CHAR_WIDTH * 2;
        display->setCursor(cursorX, cursorY);
        display->setTextColor(isSelected ? COLOR_BLACK : COLOR_WHITE, isSelected ? COLOR_WHITE : COLOR_BLACK);
        
        
        int availableWidth = display->width() - cursorX;
        int maxChars = availableWidth / CHAR_WIDTH;
        std::string name = entry.name;
        if ((int)name.length() > maxChars && maxChars > 3) {
            name = name.substr(0, maxChars - 3) + "...";
        }
        display->print(name.c_str());
        
        cursorY += CHAR_HEIGHT;
        cursorX = 0;
        displayIndex++;
        actualIndex++;
    }
    
    
    int totalItems = showBackEntry ? currentEntries.size() + 1 : currentEntries.size();
    if (totalItems > MAX_VISIBLE_LINES) {
        int w = display->width();
        int h = display->height();
        
        
        if (startIndex > 0) {
            display->drawPixel(w - 4, 2, COLOR_WHITE);
            display->drawPixel(w - 5, 3, COLOR_WHITE);
            display->drawPixel(w - 3, 3, COLOR_WHITE);
        }
        
        
        if (startIndex + MAX_VISIBLE_LINES < totalItems) {
            int y = h - 4;
            display->drawPixel(w - 4, y + 2, COLOR_WHITE);
            display->drawPixel(w - 5, y + 1, COLOR_WHITE);
            display->drawPixel(w - 3, y + 1, COLOR_WHITE);
        }
    }
    
    display->display();
    
    menuNeedsRedraw = false;
    lastRenderedPath = currentPath;
    lastSelectedIndex = selectedIndex;
}

// Helper for string replacement
static void stringRemove(std::string& str, size_t pos, size_t len) {
    if (pos < str.length()) {
        str.erase(pos, len);
    }
}

static bool startsWith(const std::string& str, const std::string& prefix) {
    return str.rfind(prefix, 0) == 0;
}

void handleMenuSelection() {
    int totalItems = currentEntries.size();
    bool hasBackEntry = (currentPath != "/");
    if (hasBackEntry) totalItems++;
    
    if (totalItems == 0) return;
    
    
    if (hasBackEntry && selectedIndex == 0) {
        goBack();
        return;
    }
    
    
    int entryIndex = hasBackEntry ? selectedIndex - 1 : selectedIndex;
    
    if (entryIndex < 0 || entryIndex >= (int)currentEntries.size()) {
        return;
    }
    
    const FSEntry& entry = currentEntries[entryIndex];
    
    switch (entry.type) {
        case EntryType::FOLDER:
            // Use the stored targetPath if available (for dynamic folders), otherwise use UI path
            if (!entry.targetPath.empty()) {
                // For dynamic folders, we need to register a temporary mount or handle navigation differently
                // For now, use the UI path which should work for nested navigation
                navigateTo(entry.path + "/");
            } else {
                navigateTo(entry.path + "/");
            }
            break;
            
        case EntryType::APP:
            if (entry.callback) {
                const char* pathArg = entry.targetPath.empty() ? entry.path.c_str() : entry.targetPath.c_str();
                entry.callback(pathArg, entry.name.c_str());
                invalidateMenu(); 
            }
            break;
            
        case EntryType::FILE:
            
            {
                std::string filePath = entry.path;
                std::string fileName = entry.name;
                
                
                for (int i = fileName.length() - 1; i >= 0; i--) {
                    char c = fileName[i];
                    if (c < 32 || c > 126) {
                        fileName.erase(i, 1);
                    }
                }
                
                
                std::string actualPath = filePath;
                if (filePath == "/Settings/Documentation/About") {
                    actualPath = "/assets/about.txt";
                } else if (filePath == "/Settings/Documentation/Lua Docs") {
                    actualPath = "/assets/lua_docs.md";
                } else if (startsWith(filePath, "/Settings/Documentation/")) {
                    std::string fName = filePath.substr(26);
                    actualPath = "/assets/documentation/" + fName;
                }
                
                
                setTextViewerFile(fileName.c_str(), actualPath.c_str());
                startApp(textViewerApp);
            }
            break;
    }
}

void menuUp() {
    int totalItems = currentEntries.size();
    bool hasBackEntry = (currentPath != "/");
    if (hasBackEntry) totalItems++;
    
    if (totalItems == 0) return;
    
    selectedIndex--;
    if (selectedIndex < 0) {
        selectedIndex = totalItems - 1;
    }
    menuNeedsRedraw = true;
}

void menuDown() {
    int totalItems = currentEntries.size();
    bool hasBackEntry = (currentPath != "/");
    if (hasBackEntry) totalItems++;
    
    if (totalItems == 0) return;
    
    selectedIndex++;
    if (selectedIndex >= totalItems) {
        selectedIndex = 0;
    }
    menuNeedsRedraw = true;
}

void navigateTo(const std::string& path) {
    currentPath = path;
    if (currentPath.back() != '/') { // std::string::back() requires C++11
        currentPath += "/";
    }
    // Alternatively check size > 0
    
    if (currentPath.rfind("/", 0) != 0) { // startsWith
        currentPath = "/" + currentPath;
    }
    selectedIndex = 0;
    menuNeedsRedraw = true;
    updateCurrentEntries();
}

void goBack() {
    if (currentPath == "/") return;
    
    currentPath = getParentDir(currentPath);
    selectedIndex = 0;
    menuNeedsRedraw = true;
    updateCurrentEntries();
}

FSEntry* getSelectedEntry() {
    bool hasBackEntry = (currentPath != "/");
    
    if (hasBackEntry && selectedIndex == 0) {
        return nullptr; 
    }
    
    int entryIndex = hasBackEntry ? selectedIndex - 1 : selectedIndex;
    
    if (entryIndex < 0 || entryIndex >= (int)currentEntries.size()) {
        return nullptr;
    }
    
    return &currentEntries[entryIndex];
}

int getVisibleItemCount() {
    int count = currentEntries.size();
    if (currentPath != "/") count++; 
    return count;
}

void invalidateMenu() {
    menuNeedsRedraw = true;
}
