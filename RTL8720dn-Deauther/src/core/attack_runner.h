#ifndef ATTACK_RUNNER_H
#define ATTACK_RUNNER_H

#include <Arduino.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#include <map>
#include <vector>
#include "../core/app_state.h"

class WiFiServer;

extern std::map<int, std::vector<int>> g_deauth_channels;

// Transition radio to STA mode cleanly for raw packet injection / interference.
void attackPrepareStaRadio();

// Execute the active attack loop tick (no-op when AttackMode::IDLE).
void attackRunTick(WiFiServer& server);

#endif // ATTACK_RUNNER_H
