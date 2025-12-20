#ifndef ICONS_H
#define ICONS_H

#include <Arduino.h>


#define ICON_WIDTH 8
#define ICON_HEIGHT 8


struct Icon {
    const uint8_t* data;
    uint8_t width;
    uint8_t height;
};


struct IconEntry {
    const char* id;
    Icon icon;
};


#define MAX_CUSTOM_ICONS 32


#define ICON_FOLDER     "folder"
#define ICON_FILE       "file"
#define ICON_APP        "app"
#define ICON_BACK       "back"
#define ICON_SETTINGS   "settings"
#define ICON_INFO       "info"
#define ICON_WIFI       "wifi"
#define ICON_BLUETOOTH  "bluetooth"
#define ICON_SD         "sd"
#define ICON_USB        "usb"
#define ICON_NFC        "nfc"
#define ICON_RFID       "rfid"
#define ICON_IR         "ir"
#define ICON_GPIO       "gpio"
#define ICON_SUBGHZ     "subghz"
#define ICON_GAME       "game"
#define ICON_MUSIC      "music"
#define ICON_PHOTO      "photo"


void initIcons();


bool registerIcon(const char* id, const uint8_t* data, uint8_t width = ICON_WIDTH, uint8_t height = ICON_HEIGHT);


const Icon* getIcon(const char* id);


const Icon* getFolderIcon();


const Icon* getFileIcon();


const Icon* getAppIcon();


const Icon* getBackIcon();





extern const uint8_t ICON_DATA_FOLDER[];
extern const uint8_t ICON_DATA_FILE[];
extern const uint8_t ICON_DATA_APP[];
extern const uint8_t ICON_DATA_BACK[];
extern const uint8_t ICON_DATA_SETTINGS[];
extern const uint8_t ICON_DATA_INFO[];
extern const uint8_t ICON_DATA_WIFI[];
extern const uint8_t ICON_DATA_BLUETOOTH[];
extern const uint8_t ICON_DATA_SD[];
extern const uint8_t ICON_DATA_USB[];
extern const uint8_t ICON_DATA_NFC[];
extern const uint8_t ICON_DATA_RFID[];
extern const uint8_t ICON_DATA_IR[];
extern const uint8_t ICON_DATA_GPIO[];
extern const uint8_t ICON_DATA_SUBGHZ[];
extern const uint8_t ICON_DATA_GAME[];
extern const uint8_t ICON_DATA_MUSIC[];
extern const uint8_t ICON_DATA_PHOTO[];






#define DEFINE_ICON(name, ...) \
    const uint8_t name[] PROGMEM = { __VA_ARGS__ }


#define REGISTER_ICON(id, data) \
    registerIcon(id, data, ICON_WIDTH, ICON_HEIGHT)

#endif 

