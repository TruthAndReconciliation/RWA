// Host-side tests for the comms board's plain C++ logic.
// Build: g++ -std=c++17 -fsanitize=address,undefined comms_test.cpp ../MB001_Comms/BridgeLogic.cpp
#include "../MB001_Comms/BridgeLogic.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// Feed a whole string; returns the last result.
static FeedResult feedAll(LineReader& r, const char* text, uint32_t now) {
  FeedResult last = LINE_NOT_READY;
  for (const char* p = text; *p; p++) last = r.feed(*p, now);
  return last;
}

static void testLineReader() {
  LineReader r(MAX_COMMAND_LINE);
  // Normal line, with and without "\r\n".
  assert(feedAll(r, "PING\n", 0) == LINE_READY && strcmp(r.line(), "PING") == 0);
  assert(feedAll(r, "STATUS\r\n", 0) == LINE_READY && strcmp(r.line(), "STATUS") == 0);
  // Empty lines are ignored.
  assert(r.feed('\n', 0) == LINE_NOT_READY);
  // A non-printable byte rejects the line once, then waits for '\n'.
  assert(r.feed('P', 0) == LINE_NOT_READY);
  assert(r.feed('\x01', 0) == LINE_REJECTED);
  assert(feedAll(r, "ING", 0) == LINE_NOT_READY);
  assert(r.feed('\n', 0) == LINE_NOT_READY && r.errors == 1);
  assert(feedAll(r, "PING\n", 0) == LINE_READY); // resynchronized

  // Exactly 95 characters is allowed, 96 is rejected.
  char longLine[100];
  memset(longLine, 'A', 95); longLine[95] = '\n'; longLine[96] = '\0';
  assert(feedAll(r, longLine, 0) == LINE_READY && strlen(r.line()) == 95);
  memset(longLine, 'A', 96); longLine[96] = '\0';
  assert(feedAll(r, longLine, 0) == LINE_REJECTED && r.errors == 2);
  assert(r.feed('\n', 0) == LINE_NOT_READY);

  // A half line expires after 250 ms; the rest of it is thrown away.
  feedAll(r, "STA", 1000);
  assert(!r.expire(1249));
  assert(r.expire(1250) && r.errors == 3);
  assert(feedAll(r, "TUS\n", 1300) == LINE_NOT_READY);
  assert(feedAll(r, "PING\n", 1300) == LINE_READY);
  // Expiry still works across millis() rollover.
  feedAll(r, "PI", UINT32_MAX - 100);
  assert(!r.expire(100));
  assert(r.expire(149));

  // Reply reader allows long STATUS lines from the control board.
  LineReader replies(MAX_REPLY_LINE);
  char status[300];
  memset(status, 'S', 255); status[255] = '\n'; status[256] = '\0';
  assert(feedAll(replies, status, 0) == LINE_READY && strlen(replies.line()) == 255);
  // A size bigger than the buffer is clamped to the buffer.
  LineReader clamped(10000);
  memset(status, 'S', 256); status[256] = '\0';
  assert(feedAll(clamped, status, 0) == LINE_REJECTED);
}

static void testCommandFilter() {
  assert(isForwardableCommand("PING"));
  assert(isForwardableCommand("TELEMETRY 1"));
  assert(isForwardableCommand("CLEAR_FAULT"));
  assert(isForwardableCommand("STOP\tEXTRA")); // control board rejects the extra word
  assert(!isForwardableCommand(""));
  assert(!isForwardableCommand(" PING"));
  assert(!isForwardableCommand("PINGX"));
  assert(!isForwardableCommand("PIN"));
  assert(!isForwardableCommand("ping")); // commands are uppercase
  assert(!isForwardableCommand("ARM"));  // movement over Wi-Fi not supported yet
  assert(!isForwardableCommand("BRIDGE")); // handled by the comms board itself
}

static void testClientGate() {
  ClientGate g;
  assert(!g.isFresh(0));
  assert(g.accept(111, 5000, 0));       // first client takes control
  assert(g.accept(111, 5000, 500));     // same client keeps it
  assert(!g.accept(222, 5000, 600));    // other device refused while fresh
  assert(!g.accept(111, 5001, 600));    // new port counts as a new client
  assert(g.isFresh(1499) && !g.isFresh(1500));
  assert(g.accept(222, 6000, 1500));    // old client quiet: takeover allowed
  assert(g.ip == 222 && g.port == 6000);
  // Rollover.
  assert(g.accept(222, 6000, UINT32_MAX - 10));
  assert(g.isFresh(500) && !g.isFresh(990));
}

static void testLinkMonitor() {
  LinkMonitor m;
  assert(!m.isUp(0));
  assert(!m.shouldSendPing(500));
  assert(m.shouldSendPing(1000));       // quiet since boot: ping
  assert(!m.shouldSendPing(1500));      // not again so soon
  assert(!m.onLine("PONG", 1600));      // our own PONG is swallowed
  assert(m.isUp(1600));
  assert(m.onLine("PONG", 1700));       // a second PONG belongs to someone else
  assert(m.onLine("STATUS version=x", 1800));
  assert(!m.shouldSendPing(2700));      // heard from it recently
  assert(m.shouldSendPing(2800));
  assert(m.isUp(4299) && !m.isUp(4300)); // 2500 ms with nothing heard
  assert(m.shouldSendPing(3800));       // keeps retrying while down
  // A user's PONG arriving while our ping is pending is swallowed once,
  // which is harmless because it carries no information.
  assert(!m.onLine("PONG", 3900));
  assert(m.onLine("PONG", 3901));
}

int main() {
  testLineReader();
  testCommandFilter();
  testClientGate();
  testLinkMonitor();
  puts("PASS: comms line reader, command filter, single Wi-Fi client gate, link keepalive and rollover");
  return 0;
}
