MB001 COMMS BOARD (Wi-Fi bridge) — 0.1.0-comms-bench

WHAT IT DOES
Passes text commands from the Python script (Wi-Fi, UDP) and from the USB
Serial Monitor to the control board over a UART link, and passes the control
board's replies back. It never arms, drives or overrides anything itself: the
control board stays the only authority over arming, faults and outputs.

CURRENT LIMITS (read before testing)
- The control board does NOT listen on the UART link yet (next step), so
  BRIDGE shows link=DOWN and forwarded commands get "WARN BRIDGE_LINK_DOWN".
- Only the control board's existing commands are forwarded:
  HELP VERSION PING STATUS HEALTH DISARM STOP CLEAR_FAULT TELEMETRY 0|1.
  Movement over Wi-Fi (ARM/SET) is not supported; Xbox still owns movement.
  Keep this list in sync with MB001_Control/Protocol.cpp (BridgeLogic.cpp).
- No request IDs yet: every control board reply goes to BOTH the USB monitor
  (prefixed "CTRL> ") and the current Wi-Fi client.
- No authentication. Anyone on the same network can send commands. Use a
  private network for bench testing only.

WIRING (3 wires, both boards 3.3 V logic)
  comms GPIO17 (TX)  ->  control GPIO18 (RX)
  comms GPIO18 (RX)  <-  control GPIO17 (TX)
  comms GND          --  control GND
UART: 115200 baud, 8N1, one text command per line ending in '\n'.
Commands are at most 95 characters; replies at most 255.
At boot this board sends a single '\n' so the control board drops any half
line it saw while this board was starting.

WI-FI SETUP
1. Copy WifiSecrets.example.h to WifiSecrets.h in this folder.
2. Fill in the network name and password (2.4 GHz networks only).
   WifiSecrets.h is in .gitignore; the build stops with a clear message if
   it is missing.
3. Upload, open Serial Monitor (115200, Newline) and wait for:
   WIFI connected ip=192.168.x.x udp_port=4210 hostname=mb001-comms.local
The board reconnects on its own if the network drops.

UDP PROTOCOL FOR THE PYTHON SCRIPT
Send text commands to UDP port 4210. One command per packet is simplest; a
trailing '\n' is optional. Replies come back to the address and port that
sent the command.
One client at a time: the first sender becomes the client. Packets from any
other address/port get "ERR BRIDGE_BUSY" until the client has been quiet for
1000 ms. The client must send something (PING is fine) at least every
1000 ms to keep control; about every 250 ms is recommended. When it goes
quiet, the bridge stops sending it replies.
Bridge replies: ERR BRIDGE_UNKNOWN_COMMAND, ERR BRIDGE_BAD_LINE,
ERR BRIDGE_BUSY, ERR BRIDGE_PACKET_TOO_LONG, WARN BRIDGE_LINK_DOWN,
BRIDGE link=UP / link=DOWN (sent when the link changes), and the BRIDGE status.

Minimal Python test (laptop on the same network):
  import socket
  s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
  s.settimeout(1.0)
  s.sendto(b"BRIDGE", ("192.168.x.x", 4210))   # use the IP printed on USB
  print(s.recvfrom(512)[0].decode())

USB COMMANDS
BRIDGE        this board's status: version, link UP/DOWN, Wi-Fi state, IP,
              signal strength, current client, counters.
anything else forwarded to the control board if it is a known command.

LINK KEEPALIVE
If nothing arrives from the control board for 1000 ms, the bridge sends its
own PING (the matching PONG is not relayed). After 2500 ms with nothing
heard, the link is reported DOWN.

SAFETY NOTES
If Wi-Fi drops or the Python script stops, the bridge simply stops
forwarding. It does not send STOP, because Wi-Fi has no movement control in
this version and a STOP would latch a fault during Xbox testing. Once Wi-Fi
can send movement commands, the control board's own command watchdog must
stop the vehicle when commands stop arriving; decide then whether the bridge
should also send STOP when its client times out.

ARDUINO IDE SETUP
Either board package works: "esp32 by Espressif Systems" (ESP32S3 Dev Module)
or the esp32_bluepad32 package already used for the control board.
Flash Size: 16MB. PSRAM: OPI. USB CDC On Boot: Disabled (same as control).
Default partition scheme is fine; this sketch has no custom partitions.csv.
Keep all .h/.cpp files alongside MB001_Comms.ino.

FILES
MB001_Comms.ino: setup/loop, USB commands, UART link, routing replies.
BridgeLogic.h/.cpp: plain C++ (tested on a PC): line reader, command filter,
  single-client gate, link keepalive.
WifiLink.h/.cpp: Wi-Fi connection, UDP packets, mDNS name.
CommsConfig.h: pins, baud rate, UDP port, hostname.
WifiSecrets.example.h: template for your Wi-Fi name/password.

VALIDATION
Host tests: verification/comms_test.cpp (AddressSanitizer + UBSan) pass.
Compiled for ESP32-S3 with esp32 3.0.7 (900,021 bytes program, 44,976 bytes
RAM) and esp32_bluepad32 4.1.0 (954,137 bytes, 90,696 bytes RAM).
Not yet run on hardware: Wi-Fi join, UDP, mDNS and the UART link are untested
on real boards.
