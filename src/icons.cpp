#include "icons.h"






const uint8_t ICON_DATA_FOLDER[] PROGMEM = {
    0x00, 0x70, 0x48, 0x7E, 0x42, 0x42, 0x7E, 0x00
};


const uint8_t ICON_DATA_FILE[] PROGMEM = {
    0x1C, 0x14, 0x1C, 0x10, 0x1C, 0x10, 0x1C, 0x00
};


const uint8_t ICON_DATA_APP[] PROGMEM = {
    0x18, 0x3C, 0xDB, 0xFF, 0xFF, 0xDB, 0x3C, 0x18
};


const uint8_t ICON_DATA_BACK[] PROGMEM = {
    0x10, 0x30, 0x7F, 0xFF, 0xFF, 0x7F, 0x30, 0x10
};


const uint8_t ICON_DATA_SETTINGS[] PROGMEM = {
    0x44, 0x44, 0x7C, 0x44, 0x22, 0x3E, 0x22, 0x22
};


const uint8_t ICON_DATA_INFO[] PROGMEM = {
    0x3C, 0x42, 0x99, 0x81, 0x99, 0x99, 0x42, 0x3C
};


const uint8_t ICON_DATA_WIFI[] PROGMEM = {
    0x00, 0x7E, 0x81, 0x3C, 0x42, 0x18, 0x24, 0x18
};


const uint8_t ICON_DATA_BLUETOOTH[] PROGMEM = {
    0x08, 0x4A, 0x2C, 0x18, 0x18, 0x2C, 0x4A, 0x08
};


const uint8_t ICON_DATA_SD[] PROGMEM = {
    0x7E, 0x7E, 0x42, 0x42, 0x7E, 0x7E, 0x7E, 0x7E
};


const uint8_t ICON_DATA_USB[] PROGMEM = {
    0x18, 0x18, 0x18, 0x5A, 0x5A, 0x3C, 0x18, 0x18
};


const uint8_t ICON_DATA_NFC[] PROGMEM = {
    0x20, 0x50, 0x88, 0x50, 0x28, 0x44, 0x28, 0x10
};


const uint8_t ICON_DATA_RFID[] PROGMEM = {
    0x08, 0x14, 0x22, 0x7F, 0x7F, 0x22, 0x14, 0x08
};


const uint8_t ICON_DATA_IR[] PROGMEM = {
    0x00, 0x08, 0x1C, 0x3E, 0x3E, 0x1C, 0x08, 0x00
};


const uint8_t ICON_DATA_GPIO[] PROGMEM = {
    0x55, 0x55, 0x55, 0x7F, 0x7F, 0x55, 0x55, 0x55
};


const uint8_t ICON_DATA_SUBGHZ[] PROGMEM = {
    0x04, 0x0A, 0x12, 0x24, 0x48, 0x90, 0xA0, 0x40
};


const uint8_t ICON_DATA_GAME[] PROGMEM = {
    0x00, 0x7E, 0xFF, 0xDB, 0xDB, 0xFF, 0x7E, 0x00
};


const uint8_t ICON_DATA_MUSIC[] PROGMEM = {
    0x0F, 0x09, 0x09, 0x09, 0x79, 0xF9, 0xF0, 0x60
};


const uint8_t ICON_DATA_PHOTO[] PROGMEM = {
    0x00, 0x7E, 0x81, 0xA5, 0xA5, 0x81, 0x7E, 0x00
};





static IconEntry iconRegistry[MAX_CUSTOM_ICONS];
static int registeredIconCount = 0;

void initIcons() {
    registeredIconCount = 0;
    
    
    registerIcon(ICON_FOLDER, ICON_DATA_FOLDER);
    registerIcon(ICON_FILE, ICON_DATA_FILE);
    registerIcon(ICON_APP, ICON_DATA_APP);
    registerIcon(ICON_BACK, ICON_DATA_BACK);
    registerIcon(ICON_SETTINGS, ICON_DATA_SETTINGS);
    registerIcon(ICON_INFO, ICON_DATA_INFO);
    registerIcon(ICON_WIFI, ICON_DATA_WIFI);
    registerIcon(ICON_BLUETOOTH, ICON_DATA_BLUETOOTH);
    registerIcon(ICON_SD, ICON_DATA_SD);
    registerIcon(ICON_USB, ICON_DATA_USB);
    registerIcon(ICON_NFC, ICON_DATA_NFC);
    registerIcon(ICON_RFID, ICON_DATA_RFID);
    registerIcon(ICON_IR, ICON_DATA_IR);
    registerIcon(ICON_GPIO, ICON_DATA_GPIO);
    registerIcon(ICON_SUBGHZ, ICON_DATA_SUBGHZ);
    registerIcon(ICON_GAME, ICON_DATA_GAME);
    registerIcon(ICON_MUSIC, ICON_DATA_MUSIC);
    registerIcon(ICON_PHOTO, ICON_DATA_PHOTO);
}

bool registerIcon(const char* id, const uint8_t* data, uint8_t width, uint8_t height) {
    if (registeredIconCount >= MAX_CUSTOM_ICONS) {
        return false;
    }
    
    
    for (int i = 0; i < registeredIconCount; i++) {
        if (strcmp(iconRegistry[i].id, id) == 0) {
            iconRegistry[i].icon.data = data;
            iconRegistry[i].icon.width = width;
            iconRegistry[i].icon.height = height;
            return true;
        }
    }
    
    
    iconRegistry[registeredIconCount].id = id;
    iconRegistry[registeredIconCount].icon.data = data;
    iconRegistry[registeredIconCount].icon.width = width;
    iconRegistry[registeredIconCount].icon.height = height;
    registeredIconCount++;
    
    return true;
}

const Icon* getIcon(const char* id) {
    if (!id) return nullptr;
    
    for (int i = 0; i < registeredIconCount; i++) {
        if (strcmp(iconRegistry[i].id, id) == 0) {
            return &iconRegistry[i].icon;
        }
    }
    
    return nullptr;
}

const Icon* getFolderIcon() {
    return getIcon(ICON_FOLDER);
}

const Icon* getFileIcon() {
    return getIcon(ICON_FILE);
}

const Icon* getAppIcon() {
    return getIcon(ICON_APP);
}

const Icon* getBackIcon() {
    return getIcon(ICON_BACK);
}

