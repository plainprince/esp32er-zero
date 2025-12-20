#ifndef IR_REMOTE_H
#define IR_REMOTE_H

#include <Arduino.h>

// IR LED pin - GPIO 25
#define IR_LED_PIN 25

// Initialize IR remote functionality
void initIRRemote();

// Send NEC protocol IR code
void sendNEC(uint32_t address, uint32_t command);

// Send RC5 protocol IR code
void sendRC5(uint8_t address, uint8_t command);

// Send Sony protocol IR code
void sendSony(uint32_t data, uint8_t nbits = 12);

// Send raw IR signal (for custom protocols)
void sendRaw(uint16_t* rawData, uint16_t length, uint32_t frequency = 38000);

// Send a simple on/off pulse (for testing)
void sendIRPulse(uint16_t onTime, uint16_t offTime, uint32_t frequency = 38000);

#endif
