#include "file_explorer.h"
#include "utils.h"
#include "controls.h"
#include "icons.h"
#include <LittleFS.h>
#include <FlipperDisplay.h>
#include <vector>
#include <string>
#include <algorithm>

extern FlipperDisplay* display;


static void drawEntryIcon(const Icon* icon, bool selected) {
    if (!icon || !display) return;
    
    int scale = display->getScale();
    uint16_t color = selected ? COLOR_BLACK : COLOR_WHITE;
    
    
    display->drawScaledBitmap(2, cursorY, icon->data, icon->width, icon->height, color, scale);
}


static std::string rootPath = "/storage";
static std::string currentPath = "/storage";
static int selectedIndex = 0;
static int scrollOffset = 0;

struct FileEntry {
    std::string name;
    bool isDir;
    size_t size;
};

static std::vector<FileEntry> entries;

void setFileExplorerRoot(const char* path) {
    rootPath = path;
    currentPath = path;
    selectedIndex = 0;
    scrollOffset = 0;
}

std::string getFileExplorerPath() {
    return currentPath;
}

static void loadDirectory() {
    entries.clear();
    
    File dir = LittleFS.open(currentPath.c_str());
    if (!dir || !dir.isDirectory()) {
        Serial.print(F("Failed to open directory: "));
        Serial.println(currentPath.c_str());
        return;
    }
    
    File entry = dir.openNextFile();
    while (entry) {
        FileEntry fe;
        fe.name = entry.name();
        
        // Remove path prefix if present
        size_t lastSlash = fe.name.rfind('/');
        if (lastSlash != std::string::npos) {
            fe.name = fe.name.substr(lastSlash + 1);
        }
        
        fe.isDir = entry.isDirectory();
        fe.size = entry.size();
        entries.push_back(fe);
        entry = dir.openNextFile();
    }
    
    
    std::sort(entries.begin(), entries.end(), [](const FileEntry& a, const FileEntry& b) {
        if (a.isDir != b.isDir) return a.isDir > b.isDir;
        return a.name < b.name;
    });
    
    Serial.print(F("Loaded "));
    Serial.print(entries.size());
    Serial.print(F(" entries from "));
    Serial.println(currentPath.c_str());
}

static void renderExplorer() {
    if (!display) return;
    
    acquireDisplayLock();
    
    display->clearDisplay();
    resetCursor();
    
    
    std::string header = currentPath;
    if (header.length() > 20) {
        header = "..." + header.substr(header.length() - 17);
    }
    customPrintln(header.c_str(), false);
    
    
    int maxVisible = (display->height() - CHAR_HEIGHT) / CHAR_HEIGHT;
    bool inSubDir = (currentPath != rootPath);
    
    int totalItems = entries.size() + 1;  
    
    
    if (selectedIndex < scrollOffset) {
        scrollOffset = selectedIndex;
    }
    if (selectedIndex >= scrollOffset + maxVisible) {
        scrollOffset = selectedIndex - maxVisible + 1;
    }
    
    int displayIndex = 0;
    int itemIndex = 0;
    int scale = display->getScale();
    int iconSize = ICON_WIDTH * scale;
    int textOffset = iconSize + 4;  
    
    
    if (itemIndex >= scrollOffset && displayIndex < maxVisible) {
        bool selected = (selectedIndex == itemIndex);
        if (selected) {
            display->fillRect(0, cursorY, display->width(), CHAR_HEIGHT, COLOR_WHITE);
        }
        
        
        const Icon* backIcon = getBackIcon();
        if (backIcon) {
            drawEntryIcon(backIcon, selected);
        }
        
        
        int savedCursorX = cursorX;
        cursorX = textOffset;
        customPrint(inSubDir ? "Parent" : "Exit", selected);
        cursorX = savedCursorX;
        cursorY += CHAR_HEIGHT;
        displayIndex++;
    }
    itemIndex++;
    
    
    for (const auto& entry : entries) {
        if (itemIndex >= scrollOffset && displayIndex < maxVisible) {
            bool selected = (selectedIndex == itemIndex);
            if (selected) {
                display->fillRect(0, cursorY, display->width(), CHAR_HEIGHT, COLOR_WHITE);
            }
            
            
            const Icon* icon = entry.isDir ? getFolderIcon() : getFileIcon();
            if (icon) {
                drawEntryIcon(icon, selected);
            }
            
            
            std::string line = entry.name;
            if (!entry.isDir) {
                char sizeBuf[32];
                if (entry.size < 1024) {
                    snprintf(sizeBuf, sizeof(sizeBuf), " (%uB)", (unsigned int)entry.size);
                } else {
                    snprintf(sizeBuf, sizeof(sizeBuf), " (%uK)", (unsigned int)(entry.size / 1024));
                }
                line += sizeBuf;
            }
            
            
            int savedCursorX = cursorX;
            cursorX = textOffset;
            customPrint(line.c_str(), selected);
            cursorX = savedCursorX;
            cursorY += CHAR_HEIGHT;
            displayIndex++;
        }
        itemIndex++;
    }
    
    
    if (scrollOffset > 0) {
        display->fillRect(display->width() - 4, CHAR_HEIGHT, 3, 3, COLOR_WHITE);
    }
    if (scrollOffset + maxVisible < totalItems) {
        display->fillRect(display->width() - 4, display->height() - 4, 3, 3, COLOR_WHITE);
    }
    
    releaseDisplayLock();
    
    requestDisplayRefresh();
}

static bool startsWith(const std::string& str, const std::string& prefix) {
    return str.rfind(prefix, 0) == 0;
}

static std::string readFileContent(const std::string& path) {
    
    std::string actualPath = path;
    if (path == "/Settings/Documentation/About") {
        actualPath = "/assets/about.txt";
    } else if (path == "/Settings/Documentation/Lua Docs") {
        actualPath = "/assets/lua_docs.md";
    } else if (startsWith(path, "/Settings/Documentation/")) {
        std::string fileName = path.substr(26);
        actualPath = "/assets/documentation/" + fileName;
    }
    
    File file = LittleFS.open(actualPath.c_str(), "r");
    if (!file) {
        return "(Cannot read file: " + actualPath + ")";
    }
    
    
    std::string content;
    size_t maxSize = (path.find("Documentation") != std::string::npos) ? 4096 : 1024;
    content.reserve(maxSize); // Pre-allocate
    
    char buffer[128];
    while (file.available() && content.length() < maxSize) {
        size_t n = file.readBytes(buffer, sizeof(buffer));
        content.append(buffer, n);
    }
    if (file.available()) {
        content += "\n...(truncated)";
    }
    file.close();
    
    return content;
}


static std::string viewerTitle;
static std::string viewerContent;
static int viewerScroll = 0;
static std::vector<std::string> viewerLines;

static void splitIntoLines(const std::string& text) {
    viewerLines.clear();
    size_t start = 0;
    int maxChars = display->width() / CHAR_WIDTH;
    
    for (size_t i = 0; i <= text.length(); i++) {
        if (i == text.length() || text[i] == '\n') {
            std::string line = text.substr(start, i - start);
            
            while ((int)line.length() > maxChars) {
                viewerLines.push_back(line.substr(0, maxChars));
                line = line.substr(maxChars);
            }
            viewerLines.push_back(line);
            start = i + 1;
        }
    }
}

static bool viewFile(const std::string& path, const std::string& name) {
    
    std::string cleanName = name;
    for (int i = cleanName.length() - 1; i >= 0; i--) {
        char c = cleanName[i];
        if (c < 32 || c > 126) {
            cleanName.erase(i, 1);
        }
    }
    
    
    setLEDBusy();
    showLoadingScreenOverlay("(opening...)");
    
    viewerTitle = cleanName;
    viewerContent = readFileContent(path);
    viewerScroll = 0;
    splitIntoLines(viewerContent);
    setLEDReady();
    
    int maxVisible = display->height() / CHAR_HEIGHT - 1;  
    bool needsRender = true;  
    bool leftPressedLastFrame = false;
    
    while (true) {
        updateControls();
        
        
        bool leftPressed = isLeftPressed();
        bool leftJustPressed = leftPressed && !leftPressedLastFrame;
        leftPressedLastFrame = leftPressed;
        
        if (isButtonReleased() || leftJustPressed) {
            
            if (leftJustPressed) {
                while (true) {
                    updateControls();
                    int xValue = analogRead(JOYSTICK_X_PIN);
                    if (xValue >= JOYSTICK_LOW_THRESHOLD && xValue <= JOYSTICK_HIGH_THRESHOLD) {
                        break; 
                    }
                    delay(10);
                }
                delay(50); 
            }
            return true; 
        }
        
        
        bool scrolled = false;
        if (isUpPressed() && viewerScroll > 0) {
            viewerScroll--;
            scrolled = true;
        }
        if (isDownPressed() && viewerScroll < (int)viewerLines.size() - maxVisible) {
            viewerScroll++;
            scrolled = true;
        }
        
        
        if (needsRender || scrolled) {
            needsRender = false;
            
            acquireDisplayLock();
            
            display->clearDisplay();
            resetCursor();
            
            
            display->fillRect(0, 0, display->width(), CHAR_HEIGHT, COLOR_WHITE);
            customPrintln(viewerTitle.c_str(), true);
            
            
            for (int i = 0; i < maxVisible && (viewerScroll + i) < (int)viewerLines.size(); i++) {
                customPrintln(viewerLines[viewerScroll + i].c_str(), false);
            }
            
            
            if (viewerScroll > 0) {
                display->fillRect(display->width() - 4, CHAR_HEIGHT, 3, 3, COLOR_WHITE);
            }
            if (viewerScroll + maxVisible < (int)viewerLines.size()) {
                display->fillRect(display->width() - 4, display->height() - 4, 3, 3, COLOR_WHITE);
            }
            
            releaseDisplayLock();
            
            requestDisplayRefresh();
        }
        
        delay(20); 
    }
}

AppState fileExplorerApp() {
    static bool initialized = false;
    static bool needsRender = true;
    
    if (!initialized) {
        initialized = true;
        loadDirectory();
        needsRender = true;
    }
    
    updateControls();
    
    
    if (isButtonReleased()) {
        bool inSubDir = (currentPath != rootPath);
        int actualIndex = selectedIndex - 1;  
        
        if (selectedIndex == 0) {
            
            if (inSubDir) {
                
                size_t lastSlash = currentPath.rfind('/');
                if (lastSlash != std::string::npos && lastSlash > 0) {
                    currentPath = currentPath.substr(0, lastSlash);
                } else {
                    currentPath = rootPath;
                }
                selectedIndex = 0;
                scrollOffset = 0;
                loadDirectory();
                needsRender = true;
            } else {
                
                initialized = false;
                currentPath = rootPath;
                selectedIndex = 0;
                scrollOffset = 0;
                return AppState::EXIT;
            }
        } else if (actualIndex >= 0 && actualIndex < (int)entries.size()) {
            FileEntry& entry = entries[actualIndex];
            if (entry.isDir) {
                
                showLoadingScreenOverlay("(opening...)");
                
                
                currentPath = currentPath + "/" + entry.name;
                selectedIndex = 0;
                scrollOffset = 0;
                loadDirectory();
                needsRender = true;
            } else {
                
                std::string filePath = currentPath + "/" + entry.name;
                bool exitedViaLeft = false;
                
                
                updateControls();
                if (isLeftPressed()) {
                    exitedViaLeft = true;
                }
                
                viewFile(filePath, entry.name);
                
                
                
                if (exitedViaLeft) {
                    
                    while (true) {
                        updateControls();
                        int xValue = analogRead(JOYSTICK_X_PIN);
                        if (xValue >= JOYSTICK_LOW_THRESHOLD && xValue <= JOYSTICK_HIGH_THRESHOLD) {
                            break; 
                        }
                        delay(10);
                    }
                    
                    delay(50);
                    
                    updateControls();
                }
                
                
                needsRender = true;
            }
        }
    }
    
    
    
    
    
    static bool leftPressedLastFrame = false;
    updateControls();
    bool leftPressed = isLeftPressed();
    bool leftJustPressed = leftPressed && !leftPressedLastFrame;
    leftPressedLastFrame = leftPressed;
    
    if (leftJustPressed) {
        
        if (currentPath != rootPath) {
            size_t lastSlash = currentPath.rfind('/');
            if (lastSlash != std::string::npos && lastSlash > 0) {
                currentPath = currentPath.substr(0, lastSlash);
            } else {
                currentPath = rootPath;
            }
            selectedIndex = 0;
            scrollOffset = 0;
            loadDirectory();
            needsRender = true;
        } else {
            
            initialized = false;
            currentPath = rootPath;
            selectedIndex = 0;
            scrollOffset = 0;
            return AppState::EXIT;
        }
    }
    
    
    int totalItems = entries.size() + 1;  
    
    if (isUpPressed() && selectedIndex > 0) {
        selectedIndex--;
        needsRender = true;
    }
    if (isDownPressed() && selectedIndex < totalItems - 1) {
        selectedIndex++;
        needsRender = true;
    }
    
    if (needsRender) {
        renderExplorer();
        needsRender = false;
    }
    
    return AppState::RUNNING;
}
