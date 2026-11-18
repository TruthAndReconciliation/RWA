#pragma once
#include <stdint.h>
#include <stddef.h>

// Wi-Fi side of the bridge: joins the network and talks UDP to the Python script.
// Each UDP packet holds one or more text lines (same commands as the USB
// Serial Monitor). A packet without a trailing '\n' still counts as one line.

// Start connecting. Reconnects automatically if the network drops.
void beginWifi();
// Call every loop(). Calls onLine() once for each complete line received
// from the current Wi-Fi client.
void pollWifi(uint32_t now, void (*onLine)(const char* line));
// Send one line to the current Wi-Fi client (does nothing if there is none,
// or if it has gone quiet).
void sendToWifiClient(const char* line);
// Writes "wifi=... ip=... client=..." into out, for the BRIDGE command.
void formatWifiStatus(char* out, size_t size, uint32_t now);
