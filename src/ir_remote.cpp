#include "ir_remote.h"
#include <IRremoteESP8266.h>
#include <IRsend.h>

static IRsend irsend(IR_LED_PIN);

void initIRRemote() {
    irsend.begin();
    pinMode(IR_LED_PIN, OUTPUT);
    digitalWrite(IR_LED_PIN, LOW);
    Serial.println(F("IR Remote initialized on GPIO 25"));
}

void sendNEC(uint32_t address, uint32_t command) {
    // Combine address and command into a single 32-bit value
    uint32_t data = (address << 16) | (command & 0xFFFF);
    irsend.sendNEC(data, 32);
    delay(50); // Small delay between sends
}

void sendRC5(uint8_t address, uint8_t command) {
    irsend.sendRC5(address, command);
    delay(50);
}

void sendSony(uint32_t data, uint8_t nbits) {
    irsend.sendSony(data, nbits);
    delay(50);
}

void sendRaw(uint16_t* rawData, uint16_t length, uint32_t frequency) {
    irsend.sendRaw(rawData, length, frequency);
    delay(50);
}

void sendIRPulse(uint16_t onTime, uint16_t offTime, uint32_t frequency) {
    // Create a simple raw signal
    uint16_t rawData[2] = {onTime, offTime};
    irsend.sendRaw(rawData, 2, frequency);
    delay(50);
}
