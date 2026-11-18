#include "Protocol.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
bool parseCommand(char* line, Command& command) {
  command = Command();
  char* words[2]; int count = 0; char* saved = nullptr;
  for (char* word = strtok_r(line, " \t", &saved); word != nullptr;
       word = strtok_r(nullptr, " \t", &saved)) {
    if (count == 2) return false;
    words[count++] = word;
  }
  if (count == 0) return false;
  struct Entry { const char* name; CommandKind kind; int arguments; };
  const Entry entries[] = {
    {"HELP", CMD_HELP, 0}, {"VERSION", CMD_VERSION, 0}, {"PING", CMD_PING, 0},
    {"STATUS", CMD_STATUS, 0}, {"HEALTH", CMD_HEALTH, 0},
    {"DISARM", CMD_DISARM, 0}, {"STOP", CMD_STOP, 0}, {"CLEAR_FAULT", CMD_CLEAR_FAULT, 0},
    {"TELEMETRY", CMD_TELEMETRY, 1}
  };
  for (const Entry& entry : entries) {
    if (strcmp(words[0], entry.name) != 0) continue;
    if (count != entry.arguments + 1) return false;
    if (entry.arguments == 1) {
      char* end; errno = 0;
      long value = strtol(words[1], &end, 10);
      if (errno == ERANGE || end == words[1] || *end != '\0' ||
          value < INT_MIN || value > INT_MAX) return false;
      command.value = int(value);
    }
    command.kind = entry.kind;
    return true;
  }
  return false;
}
