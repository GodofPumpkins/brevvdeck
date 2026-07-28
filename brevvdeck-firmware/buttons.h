// BrevvDeck Firmware — Button Matrix Scanning
//
// Scans a 10×10 button matrix via 2× MCP23017 I2C GPIO expanders.
// Port A = column outputs (active low), Port B = row inputs (pull-up).
// MCP23017 #1 handles columns 0-7 and rows 0-7.
// MCP23017 #2 handles columns 8-9 and rows 8-9 (plus toggle switches).

#pragma once

#include <Arduino.h>

void buttonsInit();
void buttonsScan();
