#include <Arduino.h>
#include <WebSocketsServer.h>
#include "globals.h"
#include "net_helpers.h"

static WebSocketsServer  wsServer(WS_PORT);
static std::atomic<bool> wsReady{false};

// Last broadcast surprise name + timestamp — used to include the effect name
// for clients that reconnect after missing the original broadcast (e.g. if WS
// was idle during the synchronous Gemini call). Core 1 only; plain types safe.
static char     s_surpriseName[32];
static uint32_t s_surpriseAt = 0;

static void buildSurpriseJSON(char *j, size_t len) {
    char state[96];
    formatState(state, sizeof(state));
    int slen = strlen(state);
    if (slen > 0 && state[slen - 1] == '}')
        snprintf(j, len, "%.*s,\"name\":\"%s\"}", slen - 1, state, s_surpriseName);
    else
        strncpy(j, state, len);
}

static void onWsEvent(uint8_t num, WStype_t type, uint8_t *, size_t) {
    if (type == WStype_CONNECTED) {
        // Include recent surprise name (within 60 s) so reconnecting browsers
        // catch up on an effect they missed while the WS connection was idle.
        if (s_surpriseAt > 0 && millis() - s_surpriseAt < 60000 && s_surpriseName[0]) {
            char j[160];
            buildSurpriseJSON(j, sizeof(j));
            wsServer.sendTXT(num, j);
        } else {
            char j[96];
            formatState(j, sizeof(j));
            wsServer.sendTXT(num, j);
        }
    }
}

void wsSetup() {
    wsServer.begin();
    wsServer.onEvent(onWsEvent);
    // Ping every 10 s; wait 3 s for pong; drop after 2 missed pongs.
    // Prevents NAT/router idle-TCP teardown that silently kills WS connections.
    wsServer.enableHeartbeat(10000, 3000, 2);
    // release ensures all wsServer writes above are visible before wsReady is seen true on Core 1
    wsReady.store(true, std::memory_order_release);
}

void wsLoop() {
    if (!wsReady.load(std::memory_order_acquire)) return;
    wsServer.loop();
}

void wsPushState() {
    if (!wsReady.load(std::memory_order_acquire)) return;
    if (wsServer.connectedClients() == 0) return;
    char j[96];
    formatState(j, sizeof(j));
    wsServer.broadcastTXT(j);
}

// Called from handleSurprise() on Core 1 after a successful Gemini call.
// Stores the name for reconnecting clients and broadcasts state+name to all
// currently connected browsers so they update sliders and show the effect name.
void wsPushSurprise(const char *escapedName) {
    strncpy(s_surpriseName, escapedName, sizeof(s_surpriseName) - 1);
    s_surpriseName[sizeof(s_surpriseName) - 1] = '\0';
    s_surpriseAt = millis();
    if (!wsReady.load(std::memory_order_acquire)) return;
    if (wsServer.connectedClients() == 0) return;
    char j[160];
    buildSurpriseJSON(j, sizeof(j));
    wsServer.broadcastTXT(j);
}
