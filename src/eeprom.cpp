#include "eeprom.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>

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

static void getStorageKey(const char* key, char* outKey, size_t outSize, bool createMeta = true) {
    size_t keyLen = strlen(key);
    if (keyLen <= MAX_KEY_LEN) {
        strncpy(outKey, key, outSize - 1);
        outKey[outSize - 1] = '\0';
        return;
    }
    
    uint32_t hash = simpleHash(key);
    char hashStr[9];
    snprintf(hashStr, sizeof(hashStr), "%08X", hash);
    
    // keyStr.substring(0, MAX_KEY_LEN - 9) + hashStr
    size_t prefixLen = MAX_KEY_LEN - 8;
    if (prefixLen > keyLen) prefixLen = keyLen;
    
    strncpy(outKey, key, prefixLen);
    outKey[prefixLen] = '\0';
    strncat(outKey, hashStr, outSize - strlen(outKey) - 1);
    
    if (createMeta) {
        // Meta key for original key storage
        uint32_t metaHash = simpleHash(key); // Hash of the original key string
        char metaHashStr[9];
        snprintf(metaHashStr, sizeof(metaHashStr), "%08X", metaHash);
        
        char metaKey[20];
        snprintf(metaKey, sizeof(metaKey), "m%s", metaHashStr);
        nvs_set_str(nvsHandle, metaKey, key);
    }
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
    
    char storageKey[MAX_KEY_LEN + 1];
    getStorageKey(key, storageKey, sizeof(storageKey));
    
    size_t valueLen = strlen(value);
    
    if (valueLen <= CHUNK_SIZE) {
        esp_err_t err = nvs_set_str(nvsHandle, storageKey, value);
        if (err == ESP_OK) {
            nvs_commit(nvsHandle);
            return true;
        }
        return false;
    }
    
    // Chunked storage
    int chunkNum = 0;
    size_t offset = 0;
    
    while (offset < valueLen) {
        size_t chunkLen = (valueLen - offset > CHUNK_SIZE) ? CHUNK_SIZE : (valueLen - offset);
        
        // Create chunk buffer
        char* chunk = (char*)malloc(chunkLen + 1);
        if (!chunk) return false;
        
        memcpy(chunk, value + offset, chunkLen);
        chunk[chunkLen] = '\0';
        
        char chunkKey[32];
        snprintf(chunkKey, sizeof(chunkKey), "%s_c%d", storageKey, chunkNum);
        
        esp_err_t err = nvs_set_str(nvsHandle, chunkKey, chunk);
        free(chunk);
        
        if (err != ESP_OK) {
            return false;
        }
        
        offset += chunkLen;
        chunkNum++;
    }
    
    char countKey[32];
    snprintf(countKey, sizeof(countKey), "%s_cnt", storageKey);
    nvs_set_i32(nvsHandle, countKey, chunkNum);
    nvs_commit(nvsHandle);
    
    return true;
}

bool eepromReadString(const char* key, char* buffer, size_t bufferSize) {
    if (!nvs_initialized) {
        buffer[0] = '\0';
        return false;
    }
    
    size_t keyLen = strlen(key);
    size_t required_size = 0;
    
    // Try direct key first if short enough
    if (keyLen <= MAX_KEY_LEN) {
        esp_err_t err = nvs_get_str(nvsHandle, key, NULL, &required_size);
        if (err == ESP_OK) {
            if (required_size <= bufferSize) {
                nvs_get_str(nvsHandle, key, buffer, &required_size);
                return true;
            }
        }
    }
    
    char storageKey[MAX_KEY_LEN + 1];
    getStorageKey(key, storageKey, sizeof(storageKey), false);
    
    required_size = 0;
    esp_err_t err = nvs_get_str(nvsHandle, storageKey, NULL, &required_size);
    if (err == ESP_OK) {
        if (required_size <= bufferSize) {
            nvs_get_str(nvsHandle, storageKey, buffer, &required_size);
            return true;
        }
    }
    
    char countKey[32];
    snprintf(countKey, sizeof(countKey), "%s_cnt", storageKey);
    
    int32_t chunkCount = 0;
    err = nvs_get_i32(nvsHandle, countKey, &chunkCount);
    if (err != ESP_OK || chunkCount == 0) {
        buffer[0] = '\0';
        return false;
    }
    
    std::string fullValue = "";
    // Pre-allocate approximate memory if possible, but chunks vary.
    
    for (int i = 0; i < chunkCount; i++) {
        char chunkKey[32];
        snprintf(chunkKey, sizeof(chunkKey), "%s_c%d", storageKey, i);
        
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
    
    size_t keyLen = strlen(key);
    
    if (keyLen <= MAX_KEY_LEN) {
        size_t required_size;
        esp_err_t err = nvs_get_str(nvsHandle, key, NULL, &required_size);
        if (err == ESP_OK) {
            return true;
        }
    }
    
    char storageKey[MAX_KEY_LEN + 1];
    getStorageKey(key, storageKey, sizeof(storageKey), false);
    
    size_t required_size;
    esp_err_t err = nvs_get_str(nvsHandle, storageKey, NULL, &required_size);
    if (err == ESP_OK) {
        return true;
    }
    
    char countKey[32];
    snprintf(countKey, sizeof(countKey), "%s_cnt", storageKey);
    int32_t chunkCount;
    err = nvs_get_i32(nvsHandle, countKey, &chunkCount);
    return (err == ESP_OK);
}



