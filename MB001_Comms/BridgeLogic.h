#pragma once
#include <stdint.h>
#include <stddef.h>

// Plain C++ only (no Arduino calls) so these classes can be tested on a PC.
// See verification/comms_test.cpp.

// Longest command the control board accepts (its input buffer is 96 bytes).
const size_t MAX_COMMAND_LINE = 95;
// Control board replies (STATUS, HEALTH) are longer than commands.
const size_t MAX_REPLY_LINE = 255;

// ---------------------------------------------------------------------------
// LineReader: collects incoming bytes into one text line ending in '\n'.
// Rejects non-printable bytes and lines that are too long, then ignores
// everything until the next '\n' so it can resynchronize.
// ---------------------------------------------------------------------------
enum FeedResult { LINE_NOT_READY, LINE_READY, LINE_REJECTED };

class LineReader {
 public:
  static const uint32_t PARTIAL_TIMEOUT_MS = 250; // same as the control board

  explicit LineReader(size_t maxLength);
  // Give the reader one byte. LINE_READY means line() holds a full line.
  // LINE_REJECTED is returned once when a bad line starts being thrown away.
  FeedResult feed(char c, uint32_t now);
  // Drop a half-finished line if no byte arrived for PARTIAL_TIMEOUT_MS.
  // Returns true if something was dropped.
  bool expire(uint32_t now);
  // The finished line. Only valid until the next feed() call.
  const char* line() const { return buffer; }
  uint32_t errors = 0; // count of rejected or expired lines

 private:
  char buffer[MAX_REPLY_LINE + 1];
  size_t maxLength;
  size_t length = 0;
  bool discarding = false;
  uint32_t lastByteMs = 0;
  FeedResult reject();
};

// ---------------------------------------------------------------------------
// isForwardableCommand: true if the first word is a command the control
// board understands. Keep this list in sync with MB001_Control/Protocol.cpp.
// ---------------------------------------------------------------------------
bool isForwardableCommand(const char* line);

// ---------------------------------------------------------------------------
// ClientGate: only one Wi-Fi device (IP address + port) may send commands at a
// time. Another device can take over once the current one has been quiet for
// TIMEOUT_MS. The Python script should send PING about every 250 ms.
// ---------------------------------------------------------------------------
class ClientGate {
 public:
  static const uint32_t TIMEOUT_MS = 1000;
  bool hasClient = false;
  uint32_t ip = 0;
  uint16_t port = 0;
  uint32_t lastPacketMs = 0;

  // Called for every packet. Returns false if a different client is active.
  bool accept(uint32_t senderIp, uint16_t senderPort, uint32_t now);
  // True while the current client has sent something recently.
  bool isFresh(uint32_t now) const;
};

// ---------------------------------------------------------------------------
// LinkMonitor: is the control board answering on the UART link?
// If the link is quiet, the bridge sends its own PING. The matching PONG is
// swallowed so the Python script does not see replies it never asked for.
// ---------------------------------------------------------------------------
class LinkMonitor {
 public:
  static const uint32_t KEEPALIVE_MS = 1000; // send PING after this much quiet
  static const uint32_t TIMEOUT_MS = 2500;   // link is DOWN after this much quiet
  bool heardAnything = false;
  bool pingPending = false;
  uint32_t lastHeardMs = 0;
  uint32_t lastPingMs = 0;

  // True means "send PING to the control board now".
  bool shouldSendPing(uint32_t now);
  // Call for every line from the control board. Returns false for our own
  // keepalive PONG (do not relay it), true for everything else.
  bool onLine(const char* line, uint32_t now);
  bool isUp(uint32_t now) const;
};
