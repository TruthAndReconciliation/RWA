#pragma once
#include <stdint.h>

// ---------------------------------------------------------------------------
// UART link to the control board (uses Serial1).
// GPIO numbers are the printed board labels. The control board reserves the
// same two numbers, so the wires cross over:
//   comms GPIO17 (TX)  ->  control GPIO18 (RX)
//   comms GPIO18 (RX)  <-  control GPIO17 (TX)
//   comms GND          --  control GND   (required: shared ground)
// Both boards are 3.3 V logic, so no level shifter is needed between them.
// ---------------------------------------------------------------------------
namespace CommsPins {
  const int LINK_TX = 17;
  const int LINK_RX = 18;
}
const uint32_t LINK_BAUD = 115200;

// ---------------------------------------------------------------------------
// Wi-Fi. The board joins an existing network (see WifiSecrets.h) and listens
// for UDP packets from the Python script on this port.
// ---------------------------------------------------------------------------
const uint16_t UDP_PORT = 4210;
// Lets the Python script use "mb001-comms.local" instead of an IP address
// (works on most Mac/Linux machines; Windows may need the IP printed on USB).
const char HOSTNAME[] = "mb001-comms";
