/*
  MB001 COMMS BOARD (Wi-Fi bridge). ESP32-S3-N16R8. USB uploads only.

  Job: pass text commands from the Python script (Wi-Fi, UDP) and from the
  USB Serial Monitor to the control board (UART), and pass the control
  board's replies back to both.

  This board never decides anything about the motors. The control board is
  the only board that arms, disarms, latches faults or (later) drives
  outputs. If Wi-Fi or this board fails, the control board's own watchdogs
  must stop the vehicle; this board just stops forwarding.

  Serial Monitor: 115200 baud, Newline. Type BRIDGE for this board's status;
  any other command is forwarded to the control board.
*/
#include "CommsConfig.h"
#include "BridgeLogic.h"
#include "WifiLink.h"
#include <string.h>

const char FIRMWARE_VERSION[] = "0.1.0-comms-bench";

// Where a command came from, so BRIDGE/errors are answered in the right place.
enum Source { FROM_USB, FROM_WIFI };

LineReader usbReader(MAX_COMMAND_LINE);   // lines typed in the Serial Monitor
LineReader controlReader(MAX_REPLY_LINE); // lines coming back from the control board
LinkMonitor controlLink;                  // is the control board answering?
bool linkWasUp = false;

// Counters shown by the BRIDGE command.
uint32_t linesToControl = 0;
uint32_t linesFromControl = 0;
uint32_t commandsRejected = 0;

// Send a reply back to wherever the command came from.
void replyTo(Source source, const char* text) {
  if (source == FROM_WIFI) sendToWifiClient(text);
  else Serial.println(text);
}

// Print this board's own status (the control board has its own STATUS).
void sendBridgeStatus(Source source) {
  uint32_t now = millis();
  char wifiStatus[160];
  formatWifiStatus(wifiStatus, sizeof(wifiStatus), now);
  char text[320];
  snprintf(text, sizeof(text),
           "BRIDGE version=%s link=%s %s to_control=%lu from_control=%lu "
           "rejected=%lu usb_errors=%lu link_errors=%lu",
           FIRMWARE_VERSION, controlLink.isUp(now) ? "UP" : "DOWN", wifiStatus,
           (unsigned long)linesToControl, (unsigned long)linesFromControl,
           (unsigned long)commandsRejected, (unsigned long)usbReader.errors,
           (unsigned long)controlReader.errors);
  replyTo(source, text);
}

// One complete command line from USB or Wi-Fi.
void handleCommand(const char* line, Source source) {
  if (strcmp(line, "BRIDGE") == 0) {
    sendBridgeStatus(source);
    return;
  }
  // Only forward commands the control board knows; it re-checks everything.
  if (!isForwardableCommand(line)) {
    commandsRejected++;
    replyTo(source, "ERR BRIDGE_UNKNOWN_COMMAND");
    return;
  }
  Serial1.print(line);
  Serial1.print('\n');
  linesToControl++;
  // Still forwarded, but warn the sender that nobody may be listening.
  if (!controlLink.isUp(millis())) replyTo(source, "WARN BRIDGE_LINK_DOWN");
}

// pollWifi() needs a function with exactly this shape.
void handleWifiCommand(const char* line) {
  handleCommand(line, FROM_WIFI);
}

// Read what is typed in the Serial Monitor.
void readUsb() {
  if (usbReader.expire(millis())) Serial.println("ERR BRIDGE_PARTIAL_LINE_DROPPED");
  // Limit work per loop so one input cannot starve the others.
  for (int count = 0; count < 64 && Serial.available() > 0; count++) {
    FeedResult result = usbReader.feed((char)Serial.read(), millis());
    if (result == LINE_READY) handleCommand(usbReader.line(), FROM_USB);
    else if (result == LINE_REJECTED) Serial.println("ERR BRIDGE_BAD_LINE");
  }
}

// Read replies from the control board and pass them on.
void readControlBoard() {
  controlReader.expire(millis()); // counted in link_errors
  for (int count = 0; count < 256 && Serial1.available() > 0; count++) {
    FeedResult result = controlReader.feed((char)Serial1.read(), millis());
    if (result != LINE_READY) continue;
    const char* line = controlReader.line();
    // false means this is the PONG for our own keepalive: keep it to ourselves.
    if (!controlLink.onLine(line, millis())) continue;
    linesFromControl++;
    Serial.print("CTRL> ");
    Serial.println(line);
    sendToWifiClient(line); // no request IDs yet, so every reply goes to both
  }
}

// Keep checking that the control board is alive, and report changes.
void checkControlLink() {
  uint32_t now = millis();
  if (controlLink.shouldSendPing(now)) Serial1.print("PING\n");
  bool up = controlLink.isUp(now);
  if (up != linkWasUp) {
    const char* message = up ? "BRIDGE link=UP" : "BRIDGE link=DOWN";
    Serial.println(message);
    sendToWifiClient(message);
    linkWasUp = up;
  }
}

void setup() {
  Serial.setTxBufferSize(1024);
  Serial.begin(115200);
  // UART link to the control board on our chosen pins.
  Serial1.setRxBufferSize(1024);
  Serial1.begin(LINK_BAUD, SERIAL_8N1, CommsPins::LINK_RX, CommsPins::LINK_TX);
  // A lone newline makes the control board drop any half line it saw while
  // this board was booting.
  Serial1.print('\n');
  Serial.printf("MB001 comms bridge %s\n", FIRMWARE_VERSION);
  beginWifi();
}

void loop() {
  readUsb();
  readControlBoard();
  pollWifi(millis(), handleWifiCommand);
  checkControlLink();
  delay(1); // let the Wi-Fi tasks run
}
