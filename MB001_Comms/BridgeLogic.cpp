#include "BridgeLogic.h"
#include <string.h>

// ----------------------------- LineReader ---------------------------------

LineReader::LineReader(size_t maxLength) {
  // Never allow more than the buffer can hold (one byte is kept for '\0').
  this->maxLength = maxLength > MAX_REPLY_LINE ? MAX_REPLY_LINE : maxLength;
  buffer[0] = '\0';
}

// Start throwing away the current line. Only counts one error per bad line.
FeedResult LineReader::reject() {
  length = 0;
  if (discarding) return LINE_NOT_READY; // already counted this line
  discarding = true;
  errors++;
  return LINE_REJECTED;
}

FeedResult LineReader::feed(char c, uint32_t now) {
  lastByteMs = now;
  if (c == '\r') return LINE_NOT_READY; // allow Windows "\r\n" endings

  if (c == '\n') {
    // End of line: hand it over unless it was bad or empty.
    bool ready = !discarding && length > 0;
    buffer[length] = '\0';
    length = 0;
    discarding = false; // a newline always resynchronizes
    return ready ? LINE_READY : LINE_NOT_READY;
  }

  if (discarding) return LINE_NOT_READY; // waiting for the next newline

  // Only plain printable ASCII (and tab) is allowed, like the control board.
  bool printable = (c >= 32 && c <= 126) || c == '\t';
  if (!printable || length >= maxLength) return reject();

  buffer[length++] = c;
  return LINE_NOT_READY;
}

bool LineReader::expire(uint32_t now) {
  // Unsigned subtraction still works when millis() rolls over.
  if (length > 0 && uint32_t(now - lastByteMs) >= PARTIAL_TIMEOUT_MS) {
    reject(); // drop the partial line and skip to the next newline
    return true;
  }
  return false;
}

// -------------------------- isForwardableCommand --------------------------

bool isForwardableCommand(const char* line) {
  // Commands the control board accepts (MB001_Control/Protocol.cpp).
  const char* known[] = {"HELP", "VERSION", "PING", "STATUS", "HEALTH",
                         "DISARM", "STOP", "CLEAR_FAULT", "TELEMETRY"};

  // Find the length of the first word (stops at a space or tab).
  size_t wordLength = strcspn(line, " \t");
  if (wordLength == 0) return false;

  for (const char* name : known) {
    if (strlen(name) == wordLength && strncmp(line, name, wordLength) == 0)
      return true;
  }
  // The control board still checks the arguments itself.
  return false;
}

// ------------------------------ ClientGate --------------------------------

bool ClientGate::isFresh(uint32_t now) const {
  return hasClient && uint32_t(now - lastPacketMs) < TIMEOUT_MS;
}

bool ClientGate::accept(uint32_t senderIp, uint16_t senderPort, uint32_t now) {
  bool sameClient = hasClient && senderIp == ip && senderPort == port;
  // Someone else is in control and still active: refuse this sender.
  if (isFresh(now) && !sameClient) return false;

  // Same client, or the old one went quiet: this sender is now the client.
  hasClient = true;
  ip = senderIp;
  port = senderPort;
  lastPacketMs = now;
  return true;
}

// ------------------------------ LinkMonitor -------------------------------

bool LinkMonitor::shouldSendPing(uint32_t now) {
  bool quiet = uint32_t(now - lastHeardMs) >= KEEPALIVE_MS;
  bool pingedRecently = uint32_t(now - lastPingMs) < KEEPALIVE_MS;
  if (!quiet || pingedRecently) return false;
  lastPingMs = now;
  pingPending = true;
  return true;
}

bool LinkMonitor::onLine(const char* line, uint32_t now) {
  heardAnything = true;
  lastHeardMs = now;
  // The first PONG after our own keepalive PING belongs to us.
  if (pingPending && strcmp(line, "PONG") == 0) {
    pingPending = false;
    return false;
  }
  return true;
}

bool LinkMonitor::isUp(uint32_t now) const {
  return heardAnything && uint32_t(now - lastHeardMs) < TIMEOUT_MS;
}
