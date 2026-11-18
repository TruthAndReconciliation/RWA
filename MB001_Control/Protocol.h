#pragma once
enum CommandKind { CMD_INVALID, CMD_HELP, CMD_VERSION, CMD_PING, CMD_STATUS, CMD_HEALTH,
  CMD_DISARM, CMD_STOP, CMD_CLEAR_FAULT, CMD_TELEMETRY };
struct Command { CommandKind kind = CMD_INVALID; int value = 0; };
// Splits the input buffer in place. Commands must be uppercase.
bool parseCommand(char* line, Command& command);
