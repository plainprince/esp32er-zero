#ifndef EEPROM_H
#define EEPROM_H

#include <Arduino.h>
#include "nvs_flash.h"
#include "nvs.h"

#define EEPROM_NAMESPACE "flipper_ui"

void initEEPROM();

bool eepromWriteString(const char* key, const char* value);
bool eepromReadString(const char* key, char* buffer, size_t bufferSize);
bool eepromWriteInt(const char* key, int value);
int eepromReadInt(const char* key, int defaultValue);
bool eepromClear();
bool eepromKeyExists(const char* key);

#endif



