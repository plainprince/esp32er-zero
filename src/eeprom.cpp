#include "eeprom.h"
#include <string.h>

static nvs_handle_t nvsHandle;
static bool nvs_initialized = false;

#define MAX_KEY_LEN 15
#define CHUNK_SIZE 1000
#define META_SUFFIX "_meta"

static uint32_t simpleHash(const char* str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

static String getStorageKey(const char* key, bool createMeta = true) {
    String keyStr = String(key);
    if (keyStr.length() <= MAX_KEY_LEN) {
        return keyStr;
    }
    
    uint32_t hash = simpleHash(key);
    char hashStr[9];
    snprintf(hashStr, sizeof(hashStr), "%08X", hash);
    
    String storageKey = keyStr.substring(0, MAX_KEY_LEN - 9);
    storageKey += hashStr;
    
    if (createMeta) {
        uint32_t metaHash = simpleHash(keyStr.c_str());
        char metaHashStr[9];
        snprintf(metaHashStr, sizeof(metaHashStr), "%08X", metaHash);
        String metaKey = String("m") + metaHashStr;
        nvs_set_str(nvsHandle, metaKey.c_str(), keyStr.c_str());
    }
    
    return storageKey;
}

void initEEPROM() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    
    err = nvs_open(EEPROM_NAMESPACE, NVS_READWRITE, &nvsHandle);
    if (err == ESP_OK) {
        nvs_initialized = true;
    }
}

bool eepromWriteString(const char* key, const char* value) {
    if (!nvs_initialized) return false;
    
    String storageKey = getStorageKey(key);
    size_t valueLen = strlen(value);
    
    if (valueLen <= CHUNK_SIZE) {
        esp_err_t err = nvs_set_str(nvsHandle, storageKey.c_str(), value);
        if (err == ESP_OK) {
            nvs_commit(nvsHandle);
            return true;
        }
        return false;
    }
    
    String baseKey = storageKey;
    int chunkNum = 0;
    size_t offset = 0;
    
    while (offset < valueLen) {
        size_t chunkLen = (valueLen - offset > CHUNK_SIZE) ? CHUNK_SIZE : (valueLen - offset);
        String chunk(value + offset, chunkLen);
        
        char chunkKey[20];
        snprintf(chunkKey, sizeof(chunkKey), "%s_c%d", baseKey.c_str(), chunkNum);
        
        esp_err_t err = nvs_set_str(nvsHandle, chunkKey, chunk.c_str());
        if (err != ESP_OK) {
            return false;
        }
        
        offset += chunkLen;
        chunkNum++;
    }
    
    char countKey[20];
    snprintf(countKey, sizeof(countKey), "%s_cnt", baseKey.c_str());
    nvs_set_i32(nvsHandle, countKey, chunkNum);
    nvs_commit(nvsHandle);
    
    return true;
}

bool eepromReadString(const char* key, char* buffer, size_t bufferSize) {
    if (!nvs_initialized) {
        buffer[0] = '\0';
        return false;
    }
    
    String keyStr = String(key);
    
    size_t required_size = 0;
    if (keyStr.length() <= MAX_KEY_LEN) {
        esp_err_t err = nvs_get_str(nvsHandle, keyStr.c_str(), NULL, &required_size);
        if (err == ESP_OK) {
            if (required_size <= bufferSize) {
                nvs_get_str(nvsHandle, keyStr.c_str(), buffer, &required_size);
                return true;
            }
        }
    }
    
    String storageKey = getStorageKey(key, false);
    
    required_size = 0;
    esp_err_t err = nvs_get_str(nvsHandle, storageKey.c_str(), NULL, &required_size);
    if (err == ESP_OK) {
        if (required_size <= bufferSize) {
            nvs_get_str(nvsHandle, storageKey.c_str(), buffer, &required_size);
            return true;
        }
    }
    
    char countKey[20];
    snprintf(countKey, sizeof(countKey), "%s_cnt", storageKey.c_str());
    
    int32_t chunkCount = 0;
    err = nvs_get_i32(nvsHandle, countKey, &chunkCount);
    if (err != ESP_OK || chunkCount == 0) {
        buffer[0] = '\0';
        return false;
    }
    
    String fullValue = "";
    for (int i = 0; i < chunkCount; i++) {
        char chunkKey[20];
        snprintf(chunkKey, sizeof(chunkKey), "%s_c%d", storageKey.c_str(), i);
        
        required_size = 0;
        err = nvs_get_str(nvsHandle, chunkKey, NULL, &required_size);
        if (err != ESP_OK) {
            buffer[0] = '\0';
            return false;
        }
        
        char* chunkBuffer = (char*)malloc(required_size);
        if (chunkBuffer) {
            nvs_get_str(nvsHandle, chunkKey, chunkBuffer, &required_size);
            fullValue += chunkBuffer;
            free(chunkBuffer);
        }
    }
    
    size_t copyLen = (fullValue.length() < bufferSize - 1) ? fullValue.length() : (bufferSize - 1);
    strncpy(buffer, fullValue.c_str(), copyLen);
    buffer[copyLen] = '\0';
    return true;
}

bool eepromWriteInt(const char* key, int value) {
    if (!nvs_initialized) return false;
    esp_err_t err = nvs_set_i32(nvsHandle, key, value);
    if (err == ESP_OK) {
        nvs_commit(nvsHandle);
        return true;
    }
    return false;
}

int eepromReadInt(const char* key, int defaultValue) {
    if (!nvs_initialized) return defaultValue;
    int32_t value;
    esp_err_t err = nvs_get_i32(nvsHandle, key, &value);
    return (err == ESP_OK) ? value : defaultValue;
}

bool eepromClear() {
    if (!nvs_initialized) return false;
    esp_err_t err = nvs_erase_all(nvsHandle);
    if (err == ESP_OK) {
        nvs_commit(nvsHandle);
        return true;
    }
    return false;
}

bool eepromKeyExists(const char* key) {
    if (!nvs_initialized) return false;
    
    String keyStr = String(key);
    
    if (keyStr.length() <= MAX_KEY_LEN) {
        size_t required_size;
        esp_err_t err = nvs_get_str(nvsHandle, keyStr.c_str(), NULL, &required_size);
        if (err == ESP_OK) {
            return true;
        }
    }
    
    String storageKey = getStorageKey(key, false);
    
    size_t required_size;
    esp_err_t err = nvs_get_str(nvsHandle, storageKey.c_str(), NULL, &required_size);
    if (err == ESP_OK) {
        return true;
    }
    
    char countKey[20];
    snprintf(countKey, sizeof(countKey), "%s_cnt", storageKey.c_str());
    int32_t chunkCount;
    err = nvs_get_i32(nvsHandle, countKey, &chunkCount);
    return (err == ESP_OK);
}

