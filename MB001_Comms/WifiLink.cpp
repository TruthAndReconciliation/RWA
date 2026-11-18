#include "WifiLink.h"
#include "BridgeLogic.h"
#include "CommsConfig.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>

// Wi-Fi name and password live in a file that is not committed to GitHub.
#if __has_include("WifiSecrets.h")
#include "WifiSecrets.h"
#else
#error "Copy WifiSecrets.example.h to WifiSecrets.h and fill in your Wi-Fi name and password."
#endif

static WiFiUDP udp;
static ClientGate gate;                            // which device is in control
static LineReader packetReader(MAX_COMMAND_LINE);  // splits packets into lines
static bool udpRunning = false;
static bool clientWasFresh = false;
static uint32_t packetsRejected = 0;

// Biggest packet we accept. Commands are short, so this is plenty.
static const int MAX_PACKET = 512;

void beginWifi() {
  WiFi.mode(WIFI_STA);          // join a network (not create our own)
  WiFi.setHostname(HOSTNAME);
  WiFi.setAutoReconnect(true);  // keep retrying if the network drops
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("WIFI connecting to \"%s\"...\n", WIFI_SSID);
}

// Reply straight to whoever sent the current packet (used for refusals).
static void replyToSender(const char* line) {
  udp.beginPacket(udp.remoteIP(), udp.remotePort());
  udp.print(line);
  udp.print('\n');
  udp.endPacket();
}

// Start/stop the UDP listener when the Wi-Fi connection comes and goes.
static void updateConnection() {
  bool connected = WiFi.status() == WL_CONNECTED;
  if (connected && !udpRunning) {
    udp.begin(UDP_PORT);
    MDNS.begin(HOSTNAME);
    udpRunning = true;
    Serial.printf("WIFI connected ip=%s udp_port=%u hostname=%s.local\n",
                  WiFi.localIP().toString().c_str(), UDP_PORT, HOSTNAME);
  } else if (!connected && udpRunning) {
    udp.stop();
    MDNS.end();
    udpRunning = false;
    Serial.println("WIFI disconnected (retrying automatically)");
  }
}

void pollWifi(uint32_t now, void (*onLine)(const char* line)) {
  updateConnection();
  if (!udpRunning) return;

  // Tell the USB monitor once when the Python script goes quiet.
  bool fresh = gate.isFresh(now);
  if (clientWasFresh && !fresh)
    Serial.printf("WIFI client timed out (no packet for %lu ms)\n",
                  (unsigned long)ClientGate::TIMEOUT_MS);
  clientWasFresh = fresh;

  // Handle a few packets per loop so the UART side never waits long.
  for (int count = 0; count < 4; count++) {
    int size = udp.parsePacket();
    if (size <= 0) break; // nothing waiting

    // Only one client may control the bridge at a time.
    uint32_t senderIp = (uint32_t)udp.remoteIP();
    if (!gate.accept(senderIp, udp.remotePort(), now)) {
      packetsRejected++;
      replyToSender("ERR BRIDGE_BUSY");
      continue; // the next parsePacket() discards this packet
    }
    if (size > MAX_PACKET) {
      packetsRejected++;
      replyToSender("ERR BRIDGE_PACKET_TOO_LONG");
      continue;
    }

    uint8_t data[MAX_PACKET];
    int length = udp.read(data, sizeof(data));
    for (int i = 0; i < length; i++) {
      FeedResult result = packetReader.feed((char)data[i], now);
      if (result == LINE_READY) onLine(packetReader.line());
      else if (result == LINE_REJECTED) sendToWifiClient("ERR BRIDGE_BAD_LINE");
    }
    // The end of a packet also ends a line, so "PING" works without "\n".
    if (packetReader.feed('\n', now) == LINE_READY) onLine(packetReader.line());
  }
}

void sendToWifiClient(const char* line) {
  if (!udpRunning || !gate.isFresh(millis())) return;
  udp.beginPacket(IPAddress(gate.ip), gate.port);
  udp.print(line);
  udp.print('\n');
  udp.endPacket();
}

void formatWifiStatus(char* out, size_t size, uint32_t now) {
  bool connected = WiFi.status() == WL_CONNECTED;
  const char* client = gate.isFresh(now) ? "FRESH" : (gate.hasClient ? "STALE" : "NONE");
  snprintf(out, size,
           "wifi=%s ip=%s rssi=%d udp_port=%u client=%s client_addr=%s:%u wifi_rejected=%lu",
           connected ? "CONNECTED" : "DISCONNECTED",
           WiFi.localIP().toString().c_str(), connected ? (int)WiFi.RSSI() : 0,
           UDP_PORT, client, IPAddress(gate.ip).toString().c_str(), gate.port,
           (unsigned long)packetsRejected);
}
